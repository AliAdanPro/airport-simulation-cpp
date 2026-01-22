/**
 * Smart Airport Operations Management System
 * memory_manager.cpp - AWSC-PPC Page Replacement Algorithm Implementation
 * 
 * Implements all 8 components of the advanced page replacement algorithm
 */

#include "memory_manager.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <numeric>

// ============================================================================
// Constructor/Destructor
// ============================================================================

MemoryManager::MemoryManager(int numFrames) : totalFrames(numFrames) {
    // Initialize page frames
    pageFrames.resize(numFrames);
    for (int i = 0; i < numFrames; i++) {
        pageFrames[i].pageId = -1;
        pageFrames[i].processId = -1;
        pageFrames[i].referenceBit = false;
        pageFrames[i].modifiedBit = false;
        pageFrames[i].accessFrequency = 0;
        pageFrames[i].lastAccessTime = 0;
        pageFrames[i].accessPattern = AccessPattern::RANDOM;
        pageFrames[i].workingSetMember = false;
        pageFrames[i].prefetchCandidate = false;
        pageFrames[i].compressionRatio = 0.0;
        pageFrames[i].lockBit = false;
        pageFrames[i].predictionScore = 0.0;
        pageFrames[i].isCompressed = false;
    }
    
    // Initialize TLB
    tlb.resize(Config::TLB_SIZE);
    for (auto& entry : tlb) {
        entry.valid = false;
    }
    
    // Initialize mutexes and locks
    pthread_rwlock_init(&processStateLock, nullptr);
    pthread_mutex_init(&frameMutex, nullptr);
    pthread_rwlock_init(&pageTableLock, nullptr);
    pthread_mutex_init(&tlbMutex, nullptr);
    pthread_mutex_init(&writeBackMutex, nullptr);
    pthread_mutex_init(&priorityMutex, nullptr);
    pthread_mutex_init(&thrashingMutex, nullptr);
    pthread_mutex_init(&logMutex, nullptr);
}

MemoryManager::~MemoryManager() {
    stop();
    
    pthread_rwlock_destroy(&processStateLock);
    pthread_mutex_destroy(&frameMutex);
    pthread_rwlock_destroy(&pageTableLock);
    pthread_mutex_destroy(&tlbMutex);
    pthread_mutex_destroy(&writeBackMutex);
    pthread_mutex_destroy(&priorityMutex);
    pthread_mutex_destroy(&thrashingMutex);
    pthread_mutex_destroy(&logMutex);
}

void MemoryManager::start() {
    running = true;
    pthread_create(&decayThread, nullptr, decayLoop, this);
    pthread_create(&compressionThread, nullptr, compressionLoop, this);
    pthread_create(&writeBackThread, nullptr, writeBackLoop, this);
    logDecision("Memory manager started with AWSC-PPC algorithm");
}

void MemoryManager::stop() {
    running = false;
    pthread_join(decayThread, nullptr);
    pthread_join(compressionThread, nullptr);
    pthread_join(writeBackThread, nullptr);
}

// ============================================================================
// Process Management
// ============================================================================

void MemoryManager::registerProcess(int processId, ProcessPhase initialPhase) {
    pthread_rwlock_wrlock(&processStateLock);
    
    ProcessMemoryState state;
    state.processId = processId;
    state.phase = initialPhase;
    state.workingSetWindow = Config::WORKING_SET_BASE_WINDOW;
    state.faultCount = 0;
    state.accessCount = 0;
    state.faultRate = 0.0;
    state.lastAccessedPage = -1;
    
    processStates[processId] = state;
    
    pthread_rwlock_unlock(&processStateLock);
    
    pthread_rwlock_wrlock(&pageTableLock);
    pageTables[processId] = std::map<int, int>();
    pthread_rwlock_unlock(&pageTableLock);
    
    pthread_mutex_lock(&priorityMutex);
    processPriorities[processId] = 50; // Medium priority by default
    pthread_mutex_unlock(&priorityMutex);
    
    logDecision("Process registered", -1, processId);
}

void MemoryManager::unregisterProcess(int processId) {
    // Free all pages belonging to this process
    pthread_mutex_lock(&frameMutex);
    for (int i = 0; i < totalFrames; i++) {
        if (pageFrames[i].processId == processId) {
            pageFrames[i].pageId = -1;
            pageFrames[i].processId = -1;
            usedFrames--;
        }
    }
    pthread_mutex_unlock(&frameMutex);
    
    // Remove from page tables
    pthread_rwlock_wrlock(&pageTableLock);
    pageTables.erase(processId);
    pthread_rwlock_unlock(&pageTableLock);
    
    // Remove from process states
    pthread_rwlock_wrlock(&processStateLock);
    processStates.erase(processId);
    pthread_rwlock_unlock(&processStateLock);
    
    logDecision("Process unregistered", -1, processId);
}

void MemoryManager::updateProcessPhase(int processId, ProcessPhase phase) {
    pthread_rwlock_wrlock(&processStateLock);
    auto it = processStates.find(processId);
    if (it != processStates.end()) {
        it->second.phase = phase;
        it->second.workingSetWindow = calculateWorkingSetWindow(processId);
    }
    pthread_rwlock_unlock(&processStateLock);
}

void MemoryManager::setProcessPriority(int processId, int priority) {
    pthread_mutex_lock(&priorityMutex);
    processPriorities[processId] = priority;
    pthread_mutex_unlock(&priorityMutex);
}

// ============================================================================
// Page Access and Fault Handling
// ============================================================================

int MemoryManager::accessPage(int processId, int pageId) {
    // Update access count
    pthread_rwlock_wrlock(&processStateLock);
    if (processStates.find(processId) != processStates.end()) {
        processStates[processId].accessCount++;
    }
    pthread_rwlock_unlock(&processStateLock);
    
    // Try TLB first
    int frame = tlbLookup(processId, pageId);
    if (frame != -1) {
        metrics.tlbHits++;
        
        // Update page metadata
        pthread_mutex_lock(&frameMutex);
        pageFrames[frame].referenceBit = true;
        pageFrames[frame].accessFrequency++;
        pageFrames[frame].lastAccessTime = getCurrentTime();
        
        // Decompress if needed
        if (pageFrames[frame].isCompressed) {
            decompressPage(frame);
        }
        pthread_mutex_unlock(&frameMutex);
        
        // Update Markov chain (Component 8)
        pthread_rwlock_rdlock(&processStateLock);
        int lastPage = -1;
        if (processStates.find(processId) != processStates.end()) {
            lastPage = processStates[processId].lastAccessedPage;
        }
        pthread_rwlock_unlock(&processStateLock);
        
        if (lastPage != -1) {
            updateTransitionMatrix(processId, lastPage, pageId);
        }
        
        pthread_rwlock_wrlock(&processStateLock);
        if (processStates.find(processId) != processStates.end()) {
            processStates[processId].lastAccessedPage = pageId;
        }
        pthread_rwlock_unlock(&processStateLock);
        
        // Update reference history
        updateReferenceHistory(processId, pageId);
        
        // Check for prefetch opportunity (Component 3)
        if (static_cast<double>(usedFrames) / totalFrames < (1.0 - Config::PREFETCH_MEMORY_THRESHOLD)) {
            prefetchPages(processId, pageId);
        }
        
        metrics.totalPageHits++;
        return frame;
    }
    
    metrics.tlbMisses++;
    
    // Check page table
    pthread_rwlock_rdlock(&pageTableLock);
    bool pagePresent = false;
    if (pageTables.find(processId) != pageTables.end()) {
        auto& table = pageTables[processId];
        if (table.find(pageId) != table.end()) {
            frame = table[pageId];
            pagePresent = true;
        }
    }
    pthread_rwlock_unlock(&pageTableLock);
    
    if (pagePresent && frame != -1) {
        // Page hit - update TLB
        tlbUpdate(processId, pageId, frame);
        metrics.totalPageHits++;
        
        pthread_mutex_lock(&frameMutex);
        pageFrames[frame].referenceBit = true;
        pageFrames[frame].accessFrequency++;
        pageFrames[frame].lastAccessTime = getCurrentTime();
        pthread_mutex_unlock(&frameMutex);
        
        return frame;
    }
    
    // Page fault
    return handlePageFault(processId, pageId);
}

int MemoryManager::handlePageFault(int processId, int pageId) {
    metrics.totalPageFaults++;
    
    // Update fault count for process
    pthread_rwlock_wrlock(&processStateLock);
    if (processStates.find(processId) != processStates.end()) {
        auto& state = processStates[processId];
        state.faultCount++;
        if (state.accessCount > 0) {
            state.faultRate = static_cast<double>(state.faultCount) / state.accessCount;
        }
        // Update working set window based on fault rate
        state.workingSetWindow = calculateWorkingSetWindow(processId);
    }
    pthread_rwlock_unlock(&processStateLock);
    
    // Record fault time for thrashing detection
    pthread_mutex_lock(&thrashingMutex);
    recentFaults.push_back(getCurrentTime());
    // Keep only last minute of faults
    TimeUnit oneMinuteAgo = getCurrentTime() - 60;
    while (!recentFaults.empty() && recentFaults.front() < oneMinuteAgo) {
        recentFaults.pop_front();
    }
    pthread_mutex_unlock(&thrashingMutex);
    
    // Check for thrashing (Component 7)
    checkThrashing();
    
    // Find frame for new page using multi-pass clock (Component 2)
    int frame = findFreeFrame();
    bool foundFreeFrame = (frame != -1);  // Track if this was a truly free frame
    
    if (frame == -1) {
        // No free frames - use replacement algorithm
        frame = pass1WorkingSetProtection(processId);
        if (frame == -1) {
            frame = pass2ScoringSelection(processId);
        }
        if (frame == -1) {
            frame = pass3ForcedReplacement(processId);
        }
        
        if (frame != -1) {
            evictPage(frame);  // This decrements usedFrames
        }
    }
    
    if (frame == -1) {
        logDecision("CRITICAL: No frame available for page fault", pageId, processId);
        return -1;
    }
    
    // Load page into frame
    pthread_mutex_lock(&frameMutex);
    
    pageFrames[frame].pageId = pageId;
    pageFrames[frame].processId = processId;
    pageFrames[frame].referenceBit = true;
    pageFrames[frame].modifiedBit = false;
    pageFrames[frame].accessFrequency = 1;
    pageFrames[frame].lastAccessTime = getCurrentTime();
    pageFrames[frame].workingSetMember = true;
    pageFrames[frame].prefetchCandidate = false;
    pageFrames[frame].isCompressed = false;
    pageFrames[frame].predictionScore = 0.0;
    
    // Only increment usedFrames if we found a truly free frame (not replacement)
    // Replacement case: evictPage already decremented, then we reuse (net zero change)
    if (foundFreeFrame && usedFrames < totalFrames) {
        usedFrames++;
    }
    pthread_mutex_unlock(&frameMutex);
    
    // Update page table
    pthread_rwlock_wrlock(&pageTableLock);
    pageTables[processId][pageId] = frame;
    pthread_rwlock_unlock(&pageTableLock);
    
    // Update TLB
    tlbUpdate(processId, pageId, frame);
    
    // Update reference history
    updateReferenceHistory(processId, pageId);
    
    std::stringstream ss;
    ss << "Page fault handled: page " << pageId << " loaded to frame " << frame;
    logDecision(ss.str(), pageId, processId);
    
    return frame;
}

// ============================================================================
// COMPONENT 1: Enhanced Working Set Tracking
// ============================================================================

int MemoryManager::calculateWorkingSetWindow(int processId) {
    // Δ_actual = Δ_base * Phase_Multiplier * Load_Multiplier * Fault_Rate_Multiplier
    
    double phaseMultiplier = 1.0;
    double faultRate = 0.0;
    
    pthread_rwlock_rdlock(&processStateLock);
    auto it = processStates.find(processId);
    if (it != processStates.end()) {
        phaseMultiplier = getPhaseMultiplier(it->second.phase);
        faultRate = it->second.faultRate;
    }
    pthread_rwlock_unlock(&processStateLock);
    
    double loadMultiplier = getLoadMultiplier();
    double faultMultiplier = getFaultRateMultiplier(faultRate);
    
    int window = static_cast<int>(Config::WORKING_SET_BASE_WINDOW * 
                                   phaseMultiplier * loadMultiplier * faultMultiplier);
    
    return std::max(5, window); // Minimum window of 5
}

double MemoryManager::getPhaseMultiplier(ProcessPhase phase) {
    switch (phase) {
        case ProcessPhase::INITIALIZATION: return Config::PHASE_MULTIPLIER_INIT;
        case ProcessPhase::COMPUTATION: return Config::PHASE_MULTIPLIER_COMPUTE;
        case ProcessPhase::IO: return Config::PHASE_MULTIPLIER_IO;
        case ProcessPhase::TERMINATION: return Config::PHASE_MULTIPLIER_TERMINATE;
    }
    return Config::PHASE_MULTIPLIER_COMPUTE;
}

double MemoryManager::getLoadMultiplier() {
    double utilization = static_cast<double>(usedFrames) / totalFrames;
    
    if (utilization < 0.4) return Config::LOAD_MULT_LIGHT;
    if (utilization < 0.7) return Config::LOAD_MULT_MEDIUM;
    if (utilization < 0.9) return Config::LOAD_MULT_HEAVY;
    return Config::LOAD_MULT_CRITICAL;
}

double MemoryManager::getFaultRateMultiplier(double faultRate) {
    if (faultRate < 0.05) return Config::FAULT_MULT_LOW;
    if (faultRate < 0.15) return Config::FAULT_MULT_NORMAL;
    return Config::FAULT_MULT_HIGH;
}

void MemoryManager::updateReferenceHistory(int processId, int pageId) {
    pthread_rwlock_wrlock(&processStateLock);
    auto it = processStates.find(processId);
    if (it != processStates.end()) {
        auto& history = it->second.referenceHistory;
        history.push_back(pageId);
        
        // Keep history size bounded
        int maxHistory = it->second.workingSetWindow * 2;
        while (history.size() > static_cast<size_t>(maxHistory)) {
            history.erase(history.begin());
        }
    }
    pthread_rwlock_unlock(&processStateLock);
}

bool MemoryManager::isInWorkingSet(int processId, int pageId) {
    pthread_rwlock_rdlock(&processStateLock);
    auto it = processStates.find(processId);
    if (it == processStates.end()) {
        pthread_rwlock_unlock(&processStateLock);
        return false;
    }
    
    int window = it->second.workingSetWindow;
    const auto& history = it->second.referenceHistory;
    
    // Check if page was accessed within working set window
    int lookback = std::min(window, static_cast<int>(history.size()));
    for (int i = history.size() - lookback; i < static_cast<int>(history.size()); i++) {
        if (history[i] == pageId) {
            pthread_rwlock_unlock(&processStateLock);
            return true;
        }
    }
    
    pthread_rwlock_unlock(&processStateLock);
    return false;
}

// ============================================================================
// COMPONENT 2: Multi-Pass Clock with Intelligence
// ============================================================================

int MemoryManager::pass1WorkingSetProtection(int processId) {
    // Pass 1: Find page not in working set with R=0
    pthread_mutex_lock(&frameMutex);
    
    int scanned = 0;
    int startHand = clockHand;
    
    while (scanned < totalFrames) {
        PageFrame& frame = pageFrames[clockHand];
        
        // Skip locked pages
        if (frame.lockBit || frame.pageId == -1) {
            clockHand = (clockHand + 1) % totalFrames;
            scanned++;
            continue;
        }
        
        // Calculate working set age
        TimeUnit age = getCurrentTime() - frame.lastAccessTime;
        int window = 0;
        pthread_rwlock_rdlock(&processStateLock);
        if (processStates.find(frame.processId) != processStates.end()) {
            window = processStates[frame.processId].workingSetWindow;
        }
        pthread_rwlock_unlock(&processStateLock);
        
        // Skip if in working set
        if (age <= window && frame.workingSetMember) {
            clockHand = (clockHand + 1) % totalFrames;
            scanned++;
            continue;
        }
        
        // Check reference bit
        if (frame.referenceBit) {
            frame.referenceBit = false;
            frame.accessFrequency++;
            clockHand = (clockHand + 1) % totalFrames;
            scanned++;
            continue;
        }
        
        // Found potential victim
        int victim = clockHand;
        clockHand = (clockHand + 1) % totalFrames;
        pthread_mutex_unlock(&frameMutex);
        return victim;
    }
    
    clockHand = startHand;
    pthread_mutex_unlock(&frameMutex);
    return -1;
}

int MemoryManager::pass2ScoringSelection(int processId) {
    pthread_mutex_lock(&frameMutex);
    
    std::vector<std::pair<int, double>> candidates;
    
    for (int i = 0; i < totalFrames; i++) {
        PageFrame& frame = pageFrames[i];
        
        if (frame.pageId == -1 || frame.lockBit) continue;
        if (frame.referenceBit) {
            frame.referenceBit = false;
            continue;
        }
        
        double score = calculateVictimScore(frame, processId);
        candidates.push_back({i, score});
    }
    
    pthread_mutex_unlock(&frameMutex);
    
    if (candidates.empty()) return -1;
    
    // Select page with highest victim score
    auto best = std::max_element(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    std::stringstream ss;
    ss << "Pass 2 selected frame " << best->first << " with victim score " << best->second;
    logDecision(ss.str());
    
    return best->first;
}

int MemoryManager::pass3ForcedReplacement(int processId) {
    pthread_mutex_lock(&frameMutex);
    
    // Strategy 1: Oldest clean page outside working set
    int oldestClean = -1;
    TimeUnit oldestTime = getCurrentTime();
    
    for (int i = 0; i < totalFrames; i++) {
        PageFrame& frame = pageFrames[i];
        if (frame.pageId == -1 || frame.lockBit) continue;
        if (!frame.modifiedBit && !isInWorkingSet(frame.processId, frame.pageId)) {
            if (frame.lastAccessTime < oldestTime) {
                oldestTime = frame.lastAccessTime;
                oldestClean = i;
            }
        }
    }
    
    if (oldestClean != -1) {
        pthread_mutex_unlock(&frameMutex);
        logDecision("Pass 3 Strategy 1: oldest clean page", pageFrames[oldestClean].pageId);
        return oldestClean;
    }
    
    // Strategy 2: Least frequently used modified page outside working set
    int leastFreq = -1;
    int minFreq = 99999;
    
    for (int i = 0; i < totalFrames; i++) {
        PageFrame& frame = pageFrames[i];
        if (frame.pageId == -1 || frame.lockBit) continue;
        if (!isInWorkingSet(frame.processId, frame.pageId)) {
            if (frame.accessFrequency < minFreq) {
                minFreq = frame.accessFrequency;
                leastFreq = i;
            }
        }
    }
    
    if (leastFreq != -1) {
        pthread_mutex_unlock(&frameMutex);
        logDecision("Pass 3 Strategy 2: LFU modified page", pageFrames[leastFreq].pageId);
        return leastFreq;
    }
    
    // Strategy 3: Random page from lowest priority process
    int lowestPriority = 0;
    int lowestPriorityProcess = -1;
    
    pthread_mutex_lock(&priorityMutex);
    for (const auto& pair : processPriorities) {
        if (pair.second > lowestPriority) {
            lowestPriority = pair.second;
            lowestPriorityProcess = pair.first;
        }
    }
    pthread_mutex_unlock(&priorityMutex);
    
    for (int i = 0; i < totalFrames; i++) {
        PageFrame& frame = pageFrames[i];
        if (frame.processId == lowestPriorityProcess && !frame.lockBit) {
            pthread_mutex_unlock(&frameMutex);
            logDecision("Pass 3 Strategy 3: page from low priority process", frame.pageId);
            return i;
        }
    }
    
    // Strategy 4: Any page from requesting process (last resort)
    for (int i = 0; i < totalFrames; i++) {
        PageFrame& frame = pageFrames[i];
        if (frame.processId == processId && !frame.lockBit) {
            pthread_mutex_unlock(&frameMutex);
            logDecision("Pass 3 Strategy 4: own page (last resort)", frame.pageId);
            return i;
        }
    }
    
    pthread_mutex_unlock(&frameMutex);
    return -1;
}

double MemoryManager::calculateVictimScore(const PageFrame& frame, int requestingProcess) {
    double freqComponent = calcFrequencyComponent(frame);
    double recencyComponent = calcRecencyComponent(frame);
    double patternComponent = calcPatternComponent(frame);
    double predictionComponent = calcPredictionComponent(frame);
    
    // Base score
    double score = (freqComponent * Config::W_FREQ) +
                   (recencyComponent * Config::W_REC) +
                   (patternComponent * Config::W_PAT) +
                   (predictionComponent * Config::W_PRED);
    
    // Dirty penalty
    if (frame.modifiedBit) {
        score -= Config::DIRTY_PENALTY;
    }
    
    // Compression benefit
    if (frame.isCompressed && frame.compressionRatio > 0.5) {
        score += Config::COMPRESSION_BENEFIT;
    }
    
    // Apply process priority multiplier (Component 6)
    double priorityMult = getPriorityMultiplier(frame.processId);
    score *= priorityMult;
    
    return score;
}

double MemoryManager::calcFrequencyComponent(const PageFrame& frame) {
    // Higher frequency -> lower component -> less likely to evict
    return 1.0 / (frame.accessFrequency + 1);
}

double MemoryManager::calcRecencyComponent(const PageFrame& frame) {
    // Older pages -> higher component -> more likely to evict
    double age = static_cast<double>(getCurrentTime() - frame.lastAccessTime);
    double avgLifetime = getAvgPageLifetime();
    if (avgLifetime <= 0) avgLifetime = 100;
    return age / avgLifetime;
}

double MemoryManager::calcPatternComponent(const PageFrame& frame) {
    switch (frame.accessPattern) {
        case AccessPattern::SEQUENTIAL: return Config::PATTERN_SEQUENTIAL;
        case AccessPattern::RANDOM: return Config::PATTERN_RANDOM;
        case AccessPattern::STRIDED: return Config::PATTERN_STRIDED;
        case AccessPattern::LOOP: return 0.3; // Loop pages shouldn't be evicted
    }
    return Config::PATTERN_RANDOM;
}

double MemoryManager::calcPredictionComponent(const PageFrame& frame) {
    // Higher prediction of future access -> lower component -> less likely to evict
    return 1.0 - frame.predictionScore;
}

// ============================================================================
// COMPONENT 3: Predictive Prefetching
// ============================================================================

AccessPattern MemoryManager::detectPattern(int processId) {
    pthread_rwlock_rdlock(&processStateLock);
    auto it = processStates.find(processId);
    if (it == processStates.end() || it->second.referenceHistory.size() < 4) {
        pthread_rwlock_unlock(&processStateLock);
        return AccessPattern::RANDOM;
    }
    
    const auto& history = it->second.referenceHistory;
    int size = history.size();
    pthread_rwlock_unlock(&processStateLock);
    
    // Check for sequential pattern
    bool isSequential = true;
    for (int i = size - 3; i < size - 1 && isSequential; i++) {
        if (history[i + 1] - history[i] != 1) {
            isSequential = false;
        }
    }
    if (isSequential) return AccessPattern::SEQUENTIAL;
    
    // Check for strided pattern
    if (size >= 4) {
        int stride1 = history[size - 3] - history[size - 4];
        int stride2 = history[size - 2] - history[size - 3];
        int stride3 = history[size - 1] - history[size - 2];
        
        if (stride1 == stride2 && stride2 == stride3 && stride1 != 0) {
            return AccessPattern::STRIDED;
        }
    }
    
    // Check for loop pattern
    if (size >= 6) {
        bool isLoop = true;
        int loopSize = 3;
        for (int i = 0; i < loopSize && isLoop; i++) {
            if (history[size - 1 - i] != history[size - 1 - i - loopSize]) {
                isLoop = false;
            }
        }
        if (isLoop) return AccessPattern::LOOP;
    }
    
    return AccessPattern::RANDOM;
}

void MemoryManager::prefetchPages(int processId, int currentPage) {
    AccessPattern pattern = detectPattern(processId);
    
    if (pattern == AccessPattern::RANDOM) return;
    
    int prefetchCount = getPrefetchCount();
    
    for (int i = 1; i <= prefetchCount; i++) {
        int nextPage = getNextPageInPattern(processId, currentPage, i);
        if (nextPage == -1) break;
        
        // Check if page already loaded
        pthread_rwlock_rdlock(&pageTableLock);
        bool alreadyLoaded = false;
        if (pageTables.find(processId) != pageTables.end()) {
            alreadyLoaded = pageTables[processId].find(nextPage) != pageTables[processId].end();
        }
        pthread_rwlock_unlock(&pageTableLock);
        
        if (!alreadyLoaded) {
            int frame = findFreeFrame();
            if (frame != -1) {
                // Load prefetched page
                pthread_mutex_lock(&frameMutex);
                pageFrames[frame].pageId = nextPage;
                pageFrames[frame].processId = processId;
                pageFrames[frame].prefetchCandidate = true;
                pageFrames[frame].referenceBit = false;
                pageFrames[frame].accessFrequency = 0;
                pageFrames[frame].lastAccessTime = getCurrentTime();
                usedFrames++;
                pthread_mutex_unlock(&frameMutex);
                
                pthread_rwlock_wrlock(&pageTableLock);
                pageTables[processId][nextPage] = frame;
                pthread_rwlock_unlock(&pageTableLock);
                
                metrics.prefetchHits++;
            }
        }
    }
}

int MemoryManager::getPrefetchCount() {
    int freeFrames = totalFrames - usedFrames;
    int maxPrefetch = std::min(Config::MAX_PREFETCH_COUNT, freeFrames / 4);
    return std::max(0, maxPrefetch);
}

int MemoryManager::getNextPageInPattern(int processId, int currentPage, int offset) {
    AccessPattern pattern = detectPattern(processId);
    
    switch (pattern) {
        case AccessPattern::SEQUENTIAL:
            return currentPage + offset;
            
        case AccessPattern::STRIDED: {
            pthread_rwlock_rdlock(&processStateLock);
            auto it = processStates.find(processId);
            if (it != processStates.end() && it->second.referenceHistory.size() >= 2) {
                const auto& history = it->second.referenceHistory;
                int stride = history.back() - history[history.size() - 2];
                pthread_rwlock_unlock(&processStateLock);
                return currentPage + stride * offset;
            }
            pthread_rwlock_unlock(&processStateLock);
            return -1;
        }
        
        case AccessPattern::LOOP:
            // Don't prefetch for loops - pages already in memory
            return -1;
            
        default:
            return -1;
    }
}

// ============================================================================
// COMPONENT 4: Adaptive Page Compression
// ============================================================================

bool MemoryManager::compressPage(int frameId) {
    pthread_mutex_lock(&frameMutex);
    
    PageFrame& frame = pageFrames[frameId];
    if (frame.isCompressed || frame.lockBit) {
        pthread_mutex_unlock(&frameMutex);
        return false;
    }
    
    // Simulate page data (in real system, would be actual page content)
    std::vector<char> pageData(Config::PAGE_SIZE, 0);
    
    std::vector<char> compressed = compressRLE(pageData);
    double ratio = static_cast<double>(compressed.size()) / Config::PAGE_SIZE;
    
    if (ratio < Config::COMPRESSION_SIZE_THRESHOLD) {
        frame.isCompressed = true;
        frame.compressionRatio = ratio;
        frame.compressedData = compressed;
        metrics.pagesCompressed++;
        pthread_mutex_unlock(&frameMutex);
        
        logDecision("Page compressed", frame.pageId, frame.processId);
        return true;
    }
    
    pthread_mutex_unlock(&frameMutex);
    return false;
}

bool MemoryManager::decompressPage(int frameId) {
    pthread_mutex_lock(&frameMutex);
    
    PageFrame& frame = pageFrames[frameId];
    if (!frame.isCompressed) {
        pthread_mutex_unlock(&frameMutex);
        return false;
    }
    
    std::vector<char> decompressed = decompressRLE(frame.compressedData);
    frame.isCompressed = false;
    frame.compressionRatio = 0.0;
    frame.compressedData.clear();
    metrics.pagesDecompressed++;
    
    pthread_mutex_unlock(&frameMutex);
    return true;
}

bool MemoryManager::isCompressionCandidate(const PageFrame& frame) {
    if (frame.lockBit || frame.isCompressed) return false;
    if (frame.accessFrequency > 5) return false;
    
    TimeUnit age = getCurrentTime() - frame.lastAccessTime;
    return age > Config::COMPRESSION_AGE_THRESHOLD;
}

std::vector<char> MemoryManager::compressRLE(const std::vector<char>& data) {
    std::vector<char> compressed;
    if (data.empty()) return compressed;
    
    char current = data[0];
    int count = 1;
    
    for (size_t i = 1; i < data.size(); i++) {
        if (data[i] == current && count < 255) {
            count++;
        } else {
            compressed.push_back(static_cast<char>(count));
            compressed.push_back(current);
            current = data[i];
            count = 1;
        }
    }
    compressed.push_back(static_cast<char>(count));
    compressed.push_back(current);
    
    return compressed;
}

std::vector<char> MemoryManager::decompressRLE(const std::vector<char>& compressed) {
    std::vector<char> decompressed;
    
    for (size_t i = 0; i + 1 < compressed.size(); i += 2) {
        int count = static_cast<unsigned char>(compressed[i]);
        char value = compressed[i + 1];
        for (int j = 0; j < count; j++) {
            decompressed.push_back(value);
        }
    }
    
    return decompressed;
}

// ============================================================================
// COMPONENT 5: Frequency Decay
// ============================================================================

void MemoryManager::applyFrequencyDecay() {
    pthread_mutex_lock(&frameMutex);
    
    for (int i = 0; i < totalFrames; i++) {
        if (pageFrames[i].pageId != -1) {
            pageFrames[i].accessFrequency = static_cast<int>(
                pageFrames[i].accessFrequency * Config::FREQUENCY_DECAY_FACTOR);
        }
    }
    
    pthread_mutex_unlock(&frameMutex);
}

// ============================================================================
// COMPONENT 6: Process Priority Integration
// ============================================================================

double MemoryManager::getPriorityMultiplier(int processId) {
    pthread_mutex_lock(&priorityMutex);
    int priority = 50; // Default medium
    auto it = processPriorities.find(processId);
    if (it != processPriorities.end()) {
        priority = it->second;
    }
    pthread_mutex_unlock(&priorityMutex);
    
    // Lower priority value = higher process priority = higher multiplier
    // (makes pages less likely to be evicted)
    if (priority < 33) return Config::PRIORITY_MULT_HIGH;
    if (priority < 66) return Config::PRIORITY_MULT_MEDIUM;
    return Config::PRIORITY_MULT_LOW;
}

// ============================================================================
// COMPONENT 7: Thrashing Detection and Prevention
// ============================================================================

void MemoryManager::checkThrashing() {
    pthread_mutex_lock(&thrashingMutex);
    
    if (recentFaults.empty()) {
        pthread_mutex_unlock(&thrashingMutex);
        return;
    }
    
    // Calculate fault rate (faults per second in last minute)
    double faultRate = static_cast<double>(recentFaults.size()) / 60.0;
    
    pthread_mutex_unlock(&thrashingMutex);
    
    metrics.pageFaultRate = faultRate;
    
    if (faultRate > Config::THRASHING_THRESHOLD && !thrashingMode) {
        enterThrashingPrevention();
    } else if (faultRate < Config::THRASHING_THRESHOLD * 0.5 && thrashingMode) {
        exitThrashingPrevention();
    }
}

void MemoryManager::enterThrashingPrevention() {
    thrashingMode = true;
    metrics.thrashingEvents++;
    
    logDecision("Entering thrashing prevention mode");
    
    // Increase working set windows by 50%
    pthread_rwlock_wrlock(&processStateLock);
    for (auto& pair : processStates) {
        pair.second.workingSetWindow = static_cast<int>(pair.second.workingSetWindow * 1.5);
    }
    pthread_rwlock_unlock(&processStateLock);
    
    // Suspend low priority processes
    suspendLowPriorityProcesses();
}

void MemoryManager::exitThrashingPrevention() {
    thrashingMode = false;
    
    logDecision("Exiting thrashing prevention mode");
    
    // Restore normal working set windows
    pthread_rwlock_wrlock(&processStateLock);
    for (auto& pair : processStates) {
        pair.second.workingSetWindow = calculateWorkingSetWindow(pair.first);
    }
    pthread_rwlock_unlock(&processStateLock);
}

void MemoryManager::suspendLowPriorityProcesses() {
    // In a real system, this would suspend processes
    // For simulation, we just log it
    logDecision("Suspending low priority processes due to thrashing");
}

// ============================================================================
// COMPONENT 8: Machine Learning Prediction (Markov Chain)
// ============================================================================

void MemoryManager::updateTransitionMatrix(int processId, int fromPage, int toPage) {
    pthread_rwlock_wrlock(&processStateLock);
    auto it = processStates.find(processId);
    if (it != processStates.end()) {
        it->second.transitionMatrix[fromPage][toPage]++;
    }
    pthread_rwlock_unlock(&processStateLock);
}

int MemoryManager::predictNextPage(int processId, int currentPage) {
    pthread_rwlock_rdlock(&processStateLock);
    auto it = processStates.find(processId);
    if (it == processStates.end()) {
        pthread_rwlock_unlock(&processStateLock);
        return -1;
    }
    
    const auto& matrix = it->second.transitionMatrix;
    auto rowIt = matrix.find(currentPage);
    if (rowIt == matrix.end() || rowIt->second.empty()) {
        pthread_rwlock_unlock(&processStateLock);
        return -1;
    }
    
    // Find page with highest transition probability
    int bestPage = -1;
    int maxCount = 0;
    
    for (const auto& pair : rowIt->second) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            bestPage = pair.first;
        }
    }
    
    pthread_rwlock_unlock(&processStateLock);
    return bestPage;
}

double MemoryManager::getPredictionConfidence(int processId, int currentPage, int predictedPage) {
    pthread_rwlock_rdlock(&processStateLock);
    auto it = processStates.find(processId);
    if (it == processStates.end()) {
        pthread_rwlock_unlock(&processStateLock);
        return 0.0;
    }
    
    const auto& matrix = it->second.transitionMatrix;
    auto rowIt = matrix.find(currentPage);
    if (rowIt == matrix.end() || rowIt->second.empty()) {
        pthread_rwlock_unlock(&processStateLock);
        return 0.0;
    }
    
    int totalTransitions = 0;
    int predictedTransitions = 0;
    
    for (const auto& pair : rowIt->second) {
        totalTransitions += pair.second;
        if (pair.first == predictedPage) {
            predictedTransitions = pair.second;
        }
    }
    
    pthread_rwlock_unlock(&processStateLock);
    
    if (totalTransitions == 0) return 0.0;
    return static_cast<double>(predictedTransitions) / totalTransitions;
}

// ============================================================================
// TLB Management
// ============================================================================

int MemoryManager::tlbLookup(int processId, int virtualPage) {
    pthread_mutex_lock(&tlbMutex);
    
    for (const auto& entry : tlb) {
        if (entry.valid && entry.virtualPage == virtualPage) {
            pthread_mutex_unlock(&tlbMutex);
            return entry.physicalFrame;
        }
    }
    
    pthread_mutex_unlock(&tlbMutex);
    return -1;
}

void MemoryManager::tlbUpdate(int processId, int virtualPage, int physicalFrame) {
    pthread_mutex_lock(&tlbMutex);
    
    // Check if already in TLB
    for (auto& entry : tlb) {
        if (entry.valid && entry.virtualPage == virtualPage) {
            entry.physicalFrame = physicalFrame;
            entry.lastAccess = getCurrentTime();
            pthread_mutex_unlock(&tlbMutex);
            return;
        }
    }
    
    // Find empty slot or LRU entry
    int targetSlot = -1;
    TimeUnit oldestTime = getCurrentTime();
    
    for (int i = 0; i < Config::TLB_SIZE; i++) {
        if (!tlb[i].valid) {
            targetSlot = i;
            break;
        }
        if (tlb[i].lastAccess < oldestTime) {
            oldestTime = tlb[i].lastAccess;
            targetSlot = i;
        }
    }
    
    if (targetSlot != -1) {
        tlb[targetSlot].virtualPage = virtualPage;
        tlb[targetSlot].physicalFrame = physicalFrame;
        tlb[targetSlot].valid = true;
        tlb[targetSlot].lastAccess = getCurrentTime();
    }
    
    pthread_mutex_unlock(&tlbMutex);
}

void MemoryManager::tlbInvalidate(int virtualPage) {
    pthread_mutex_lock(&tlbMutex);
    
    for (auto& entry : tlb) {
        if (entry.valid && entry.virtualPage == virtualPage) {
            entry.valid = false;
        }
    }
    
    pthread_mutex_unlock(&tlbMutex);
}

// ============================================================================
// Write-Back Buffer
// ============================================================================

void MemoryManager::addToWriteBack(int pageId, int processId, const std::vector<char>& data) {
    pthread_mutex_lock(&writeBackMutex);
    
    WriteBackEntry entry;
    entry.pageId = pageId;
    entry.processId = processId;
    entry.data = data;
    entry.timestamp = getCurrentTime();
    
    writeBackBuffer.push_back(entry);
    
    if (writeBackBuffer.size() >= WRITE_BACK_THRESHOLD) {
        pthread_mutex_unlock(&writeBackMutex);
        flushWriteBack();
    } else {
        pthread_mutex_unlock(&writeBackMutex);
    }
}

void MemoryManager::flushWriteBack() {
    pthread_mutex_lock(&writeBackMutex);
    
    // In real system, would write to disk
    // For simulation, just clear the buffer
    int flushed = writeBackBuffer.size();
    writeBackBuffer.clear();
    
    pthread_mutex_unlock(&writeBackMutex);
    
    if (flushed > 0) {
        std::stringstream ss;
        ss << "Flushed " << flushed << " pages from write-back buffer";
        logDecision(ss.str());
    }
}

// ============================================================================
// Background Threads
// ============================================================================

void* MemoryManager::decayLoop(void* arg) {
    MemoryManager* mm = static_cast<MemoryManager*>(arg);
    mm->runDecay();
    return nullptr;
}

void MemoryManager::runDecay() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(Config::FREQUENCY_DECAY_INTERVAL / 10));
        if (!running) break;
        
        decayCounter++;
        if (decayCounter >= 100) { // Every ~1000 time units
            applyFrequencyDecay();
            decayCounter = 0;
        }
    }
}

void* MemoryManager::compressionLoop(void* arg) {
    MemoryManager* mm = static_cast<MemoryManager*>(arg);
    mm->runCompression();
    return nullptr;
}

void MemoryManager::runCompression() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (!running) break;
        
        // Only compress when memory pressure is high
        if (static_cast<double>(usedFrames) / totalFrames > 0.8) {
            pthread_mutex_lock(&frameMutex);
            for (int i = 0; i < totalFrames; i++) {
                if (isCompressionCandidate(pageFrames[i])) {
                    pthread_mutex_unlock(&frameMutex);
                    compressPage(i);
                    pthread_mutex_lock(&frameMutex);
                }
            }
            pthread_mutex_unlock(&frameMutex);
        }
    }
}

void* MemoryManager::writeBackLoop(void* arg) {
    MemoryManager* mm = static_cast<MemoryManager*>(arg);
    mm->runWriteBack();
    return nullptr;
}

void MemoryManager::runWriteBack() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!running) break;
        
        pthread_mutex_lock(&writeBackMutex);
        if (!writeBackBuffer.empty()) {
            pthread_mutex_unlock(&writeBackMutex);
            flushWriteBack();
        } else {
            pthread_mutex_unlock(&writeBackMutex);
        }
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

TimeUnit MemoryManager::getCurrentTime() {
    static auto startTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
}

int MemoryManager::findFreeFrame() {
    pthread_mutex_lock(&frameMutex);
    
    for (int i = 0; i < totalFrames; i++) {
        if (pageFrames[i].pageId == -1) {
            pthread_mutex_unlock(&frameMutex);
            return i;
        }
    }
    
    pthread_mutex_unlock(&frameMutex);
    return -1;
}

void MemoryManager::evictPage(int frameId) {
    pthread_mutex_lock(&frameMutex);
    
    PageFrame& frame = pageFrames[frameId];
    
    // Write back if modified
    if (frame.modifiedBit) {
        std::vector<char> data; // Simulated page data
        pthread_mutex_unlock(&frameMutex);
        addToWriteBack(frame.pageId, frame.processId, data);
        pthread_mutex_lock(&frameMutex);
    }
    
    // Invalidate TLB entry
    int pageId = frame.pageId;
    int processId = frame.processId;
    pthread_mutex_unlock(&frameMutex);
    
    tlbInvalidate(pageId);
    
    // Remove from page table
    pthread_rwlock_wrlock(&pageTableLock);
    if (pageTables.find(processId) != pageTables.end()) {
        pageTables[processId].erase(pageId);
    }
    pthread_rwlock_unlock(&pageTableLock);
    
    pthread_mutex_lock(&frameMutex);
    frame.pageId = -1;
    frame.processId = -1;
    usedFrames--;
    pthread_mutex_unlock(&frameMutex);
    
    std::stringstream ss;
    ss << "Evicted page " << pageId << " from frame " << frameId;
    logDecision(ss.str(), pageId, processId);
}

double MemoryManager::getAvgPageLifetime() {
    pthread_mutex_lock(&frameMutex);
    
    TimeUnit currentTime = getCurrentTime();
    double totalLifetime = 0;
    int count = 0;
    
    for (const auto& frame : pageFrames) {
        if (frame.pageId != -1) {
            totalLifetime += (currentTime - frame.lastAccessTime);
            count++;
        }
    }
    
    pthread_mutex_unlock(&frameMutex);
    
    return count > 0 ? totalLifetime / count : 100.0;
}

MemoryMetrics MemoryManager::getMetrics() const {
    MemoryMetrics m = metrics;
    m.memoryUtilization = static_cast<double>(usedFrames) / totalFrames;
    
    int totalAccesses = m.totalPageFaults + m.totalPageHits;
    if (totalAccesses > 0) {
        m.pageFaultRate = static_cast<double>(m.totalPageFaults) / totalAccesses;
    }
    
    int totalTLBAccesses = m.tlbHits + m.tlbMisses;
    if (totalTLBAccesses > 0) {
        m.tlbHitRate = static_cast<double>(m.tlbHits) / totalTLBAccesses;
    }
    
    return m;
}

std::vector<LogEntry> MemoryManager::getReplacementLog() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&logMutex));
    auto log = replacementLog;
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&logMutex));
    return log;
}

void MemoryManager::logDecision(const std::string& message, int pageId, int processId) {
    pthread_mutex_lock(&logMutex);
    
    LogEntry entry;
    entry.timestamp = getCurrentTime();
    entry.level = LogLevel::LOG_INFO;
    entry.subsystem = "MEMORY";
    entry.message = message;
    entry.relatedFlightId = pageId; // Reusing field
    entry.relatedOperationId = processId;
    
    replacementLog.push_back(entry);
    
    // Keep log bounded
    if (replacementLog.size() > 10000) {
        replacementLog.erase(replacementLog.begin(), replacementLog.begin() + 5000);
    }
    
    pthread_mutex_unlock(&logMutex);
}
