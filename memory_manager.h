/**
 * Smart Airport Operations Management System
 * memory_manager.h - AWSC-PPC Page Replacement Algorithm
 * 
 * Advanced Working Set Clock with Predictive Prefetching
 * and Adaptive Compression (AWSC-PPC)
 * 
 * Implements all 8 components:
 * 1. Enhanced Working Set Tracking
 * 2. Multi-Pass Clock with Intelligence
 * 3. Predictive Prefetching
 * 4. Adaptive Page Compression
 * 5. Frequency Decay
 * 6. Process Priority Integration
 * 7. Thrashing Detection and Prevention
 * 8. Machine Learning Prediction (Markov Chain)
 */

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "types.h"
#include "config.h"
#include <vector>
#include <map>
#include <deque>
#include <atomic>

/**
 * TLB Entry - Translation Lookaside Buffer entry
 */
struct TLBEntry {
    int virtualPage;
    int physicalFrame;
    bool valid;
    TimeUnit lastAccess;
};

/**
 * Write-Back Buffer Entry
 */
struct WriteBackEntry {
    int pageId;
    int processId;
    std::vector<char> data;
    TimeUnit timestamp;
};

/**
 * Memory Manager Metrics
 */
struct MemoryMetrics {
    int totalPageFaults = 0;
    int totalPageHits = 0;
    int tlbHits = 0;
    int tlbMisses = 0;
    int pagesCompressed = 0;
    int pagesDecompressed = 0;
    int prefetchHits = 0;
    int prefetchMisses = 0;
    int thrashingEvents = 0;
    
    double pageFaultRate = 0.0;
    double tlbHitRate = 0.0;
    double memoryUtilization = 0.0;
};

/**
 * AWSC-PPC Memory Manager
 * 
 * Main memory management class implementing all 8 components
 */
class MemoryManager {
public:
    MemoryManager(int numFrames = Config::TOTAL_PAGE_FRAMES);
    ~MemoryManager();
    
    // Initialize memory manager for a process
    void registerProcess(int processId, ProcessPhase initialPhase);
    
    // Unregister process
    void unregisterProcess(int processId);
    
    // Access a page (returns frame number or -1 if fault)
    int accessPage(int processId, int pageId);
    
    // Handle page fault
    int handlePageFault(int processId, int pageId);
    
    // Update process phase (affects working set window)
    void updateProcessPhase(int processId, ProcessPhase phase);
    
    // Set process priority
    void setProcessPriority(int processId, int priority);
    
    // Get metrics
    MemoryMetrics getMetrics() const;
    
    // Get replacement decision log
    std::vector<LogEntry> getReplacementLog() const;
    
    // Start background threads
    void start();
    void stop();
    
    // Get current memory utilization (0.0 to 1.0)
    double getUtilization() const {
        return static_cast<double>(usedFrames) / totalFrames;
    }
    
    // Check if in thrashing prevention mode
    bool isInThrashingMode() const {
        return thrashingMode.load();
    }
    
private:
    // ========================================================================
    // COMPONENT 1: Enhanced Working Set Tracking
    // ========================================================================
    
    // Per-process memory state
    std::map<int, ProcessMemoryState> processStates;
    pthread_rwlock_t processStateLock;
    
    // Calculate dynamic working set window
    int calculateWorkingSetWindow(int processId);
    
    // Get phase multiplier
    double getPhaseMultiplier(ProcessPhase phase);
    
    // Get load multiplier
    double getLoadMultiplier();
    
    // Get fault rate multiplier
    double getFaultRateMultiplier(double faultRate);
    
    // Update reference history
    void updateReferenceHistory(int processId, int pageId);
    
    // Check if page is in working set
    bool isInWorkingSet(int processId, int pageId);
    
    // ========================================================================
    // COMPONENT 2: Multi-Pass Clock with Intelligence
    // ========================================================================
    
    // Page frames
    std::vector<PageFrame> pageFrames;
    int clockHand = 0;
    pthread_mutex_t frameMutex;
    
    // Page table (processId -> (virtualPage -> frameId))
    std::map<int, std::map<int, int>> pageTables;
    pthread_rwlock_t pageTableLock;
    
    // Pass 1: Working set protection
    int pass1WorkingSetProtection(int processId);
    
    // Pass 2: Scoring and selection
    int pass2ScoringSelection(int processId);
    
    // Pass 3: Forced replacement
    int pass3ForcedReplacement(int processId);
    
    // Calculate victim score
    double calculateVictimScore(const PageFrame& frame, int requestingProcess);
    
    // Calculate individual score components
    double calcFrequencyComponent(const PageFrame& frame);
    double calcRecencyComponent(const PageFrame& frame);
    double calcPatternComponent(const PageFrame& frame);
    double calcPredictionComponent(const PageFrame& frame);
    
    // ========================================================================
    // COMPONENT 3: Predictive Prefetching
    // ========================================================================
    
    // Detect access patterns
    AccessPattern detectPattern(int processId);
    
    // Prefetch pages based on pattern
    void prefetchPages(int processId, int currentPage);
    
    // Get prefetch count based on available memory
    int getPrefetchCount();
    
    // Calculate next page in pattern
    int getNextPageInPattern(int processId, int currentPage, int offset);
    
    // ========================================================================
    // COMPONENT 4: Adaptive Page Compression
    // ========================================================================
    
    // Compress a page
    bool compressPage(int frameId);
    
    // Decompress a page
    bool decompressPage(int frameId);
    
    // Check if page is compression candidate
    bool isCompressionCandidate(const PageFrame& frame);
    
    // Simple RLE compression
    std::vector<char> compressRLE(const std::vector<char>& data);
    std::vector<char> decompressRLE(const std::vector<char>& compressed);
    
    // ========================================================================
    // COMPONENT 5: Frequency Decay
    // ========================================================================
    
    std::atomic<int> decayCounter{0};
    
    // Apply frequency decay to all pages
    void applyFrequencyDecay();
    
    // ========================================================================
    // COMPONENT 6: Process Priority Integration
    // ========================================================================
    
    // Process priorities (lower = higher priority)
    std::map<int, int> processPriorities;
    pthread_mutex_t priorityMutex;
    
    // Get priority multiplier for victim score
    double getPriorityMultiplier(int processId);
    
    // ========================================================================
    // COMPONENT 7: Thrashing Detection and Prevention
    // ========================================================================
    
    std::atomic<bool> thrashingMode{false};
    std::deque<TimeUnit> recentFaults;
    pthread_mutex_t thrashingMutex;
    
    // Check for thrashing condition
    void checkThrashing();
    
    // Enter thrashing prevention mode
    void enterThrashingPrevention();
    
    // Exit thrashing prevention mode
    void exitThrashingPrevention();
    
    // Suspend low priority processes
    void suspendLowPriorityProcesses();
    
    // ========================================================================
    // COMPONENT 8: Machine Learning Prediction (Markov Chain)
    // ========================================================================
    
    // Update transition matrix
    void updateTransitionMatrix(int processId, int fromPage, int toPage);
    
    // Predict next page
    int predictNextPage(int processId, int currentPage);
    
    // Get prediction confidence
    double getPredictionConfidence(int processId, int currentPage, int predictedPage);
    
    // ========================================================================
    // TLB Management
    // ========================================================================
    
    std::vector<TLBEntry> tlb;
    int tlbHand = 0;
    pthread_mutex_t tlbMutex;
    
    // TLB lookup
    int tlbLookup(int processId, int virtualPage);
    
    // TLB update
    void tlbUpdate(int processId, int virtualPage, int physicalFrame);
    
    // TLB invalidate
    void tlbInvalidate(int virtualPage);
    
    // ========================================================================
    // Write-Back Buffer
    // ========================================================================
    
    std::deque<WriteBackEntry> writeBackBuffer;
    pthread_mutex_t writeBackMutex;
    static constexpr int WRITE_BACK_THRESHOLD = 8;
    
    // Add to write-back buffer
    void addToWriteBack(int pageId, int processId, const std::vector<char>& data);
    
    // Flush write-back buffer
    void flushWriteBack();
    
    // ========================================================================
    // General Components
    // ========================================================================
    
    int totalFrames;
    int usedFrames = 0;
    
    // Background threads
    std::atomic<bool> running{false};
    pthread_t decayThread;
    pthread_t compressionThread;
    pthread_t writeBackThread;
    
    static void* decayLoop(void* arg);
    static void* compressionLoop(void* arg);
    static void* writeBackLoop(void* arg);
    
    void runDecay();
    void runCompression();
    void runWriteBack();
    
    // Metrics
    MemoryMetrics metrics;
    
    // Logging
    std::vector<LogEntry> replacementLog;
    pthread_mutex_t logMutex;
    void logDecision(const std::string& message, int pageId = -1, int processId = -1);
    
    // Get current time
    TimeUnit getCurrentTime();
    
    // Find free frame
    int findFreeFrame();
    
    // Evict page from frame
    void evictPage(int frameId);
    
    // Calculate average page lifetime
    double getAvgPageLifetime();
};

#endif // MEMORY_MANAGER_H
