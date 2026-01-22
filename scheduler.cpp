/**
 * Smart Airport Operations Management System
 * scheduler.cpp - HMFQ-PPRA Scheduling Algorithm Implementation
 * 
 * Implements all 8 layers of the hybrid scheduling algorithm
 */

#include "scheduler.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>

// ============================================================================
// OperationStats Implementation
// ============================================================================

void OperationStats::update(TimeUnit actualTime, bool success, double resourceUsage) {
    totalExecutions++;
    
    // Update average completion time using weighted moving average
    avgCompletionTime = (Config::LEARNING_OLD_WEIGHT * avgCompletionTime) +
                        (Config::LEARNING_NEW_WEIGHT * actualTime);
    
    // Update variance (simplified)
    double diff = actualTime - avgCompletionTime;
    varianceCompletionTime = (Config::LEARNING_OLD_WEIGHT * varianceCompletionTime) +
                             (Config::LEARNING_NEW_WEIGHT * diff * diff);
    
    // Update success rate
    double successVal = success ? 1.0 : 0.0;
    historicalSuccessRate = (Config::LEARNING_OLD_WEIGHT * historicalSuccessRate) +
                            (Config::LEARNING_NEW_WEIGHT * successVal);
    
    // Update resource consumption
    avgResourceConsumption = (Config::LEARNING_OLD_WEIGHT * avgResourceConsumption) +
                             (Config::LEARNING_NEW_WEIGHT * resourceUsage);
}

// ============================================================================
// Scheduler Constructor/Destructor
// ============================================================================

Scheduler::Scheduler() {
    // Initialize queue mutexes
    for (int i = 0; i < 5; i++) {
        pthread_mutex_init(&queueMutexes[i], nullptr);
    }
    
    // Set quantum values
    queueQuantums[0] = Config::QUEUE_0_QUANTUM;
    queueQuantums[1] = Config::QUEUE_1_QUANTUM;
    queueQuantums[2] = Config::QUEUE_2_QUANTUM;
    queueQuantums[3] = Config::QUEUE_3_QUANTUM;
    queueQuantums[4] = Config::QUEUE_4_QUANTUM;
    
    // Initialize other mutexes
    pthread_rwlock_init(&systemStateLock, nullptr);
    pthread_mutex_init(&inheritanceMutex, nullptr);
    pthread_rwlock_init(&statsLock, nullptr);
    pthread_mutex_init(&operationsMutex, nullptr);
    pthread_mutex_init(&currentOpMutex, nullptr);
    pthread_mutex_init(&logMutex, nullptr);
    pthread_mutex_init(&condMutex, nullptr);
    pthread_cond_init(&operationAvailable, nullptr);
    
    // Initialize operation statistics for each type
    for (int i = 0; i <= static_cast<int>(OperationType::TAXIWAY_MOVEMENT); i++) {
        OperationType type = static_cast<OperationType>(i);
        operationStatistics[type] = {type, 60.0, 100.0, 0.95, 1.0, 0};
    }
}

Scheduler::~Scheduler() {
    stop();
    
    // Destroy mutexes
    for (int i = 0; i < 5; i++) {
        pthread_mutex_destroy(&queueMutexes[i]);
    }
    
    pthread_rwlock_destroy(&systemStateLock);
    pthread_mutex_destroy(&inheritanceMutex);
    pthread_rwlock_destroy(&statsLock);
    pthread_mutex_destroy(&operationsMutex);
    pthread_mutex_destroy(&currentOpMutex);
    pthread_mutex_destroy(&logMutex);
    pthread_mutex_destroy(&condMutex);
    pthread_cond_destroy(&operationAvailable);
}

void Scheduler::initialize(DataManager* dm) {
    dataManager = dm;
    logDecision("Scheduler initialized with HMFQ-PPRA algorithm");
}

// ============================================================================
// Scheduler Control
// ============================================================================

void Scheduler::start() {
    running = true;
    pthread_create(&schedulerThread, nullptr, schedulerLoop, this);
    pthread_create(&agingThread, nullptr, agingLoop, this);
    logDecision("Scheduler started");
}

void Scheduler::stop() {
    running = false;
    pthread_cond_broadcast(&operationAvailable);
    pthread_join(schedulerThread, nullptr);
    pthread_join(agingThread, nullptr);
    logDecision("Scheduler stopped");
}

void* Scheduler::schedulerLoop(void* arg) {
    Scheduler* scheduler = static_cast<Scheduler*>(arg);
    scheduler->runScheduler();
    return nullptr;
}

void Scheduler::runScheduler() {
    while (running) {
        pthread_mutex_lock(&condMutex);
        
        // Wait for operations if queues are empty
        bool hasOperations = false;
        for (int i = 0; i < 5; i++) {
            pthread_mutex_lock(&queueMutexes[i]);
            if (!queues[i].empty()) hasOperations = true;
            pthread_mutex_unlock(&queueMutexes[i]);
        }
        
        if (!hasOperations && running) {
            pthread_cond_wait(&operationAvailable, &condMutex);
        }
        pthread_mutex_unlock(&condMutex);
        
        if (!running) break;
        
        // Select and execute next operation
        Operation* op = selectNextOperation();
        if (op) {
            pthread_mutex_lock(&currentOpMutex);
            currentOperation = op;
            pthread_mutex_unlock(&currentOpMutex);
            
            TimeUnit quantum = calculateActualQuantum(*op);
            
            // Execute operation for quantum time or until complete
            TimeUnit startTime = getCurrentTime();
            
            // Helper lambda to check completion safely
            auto checkComplete = [op]() -> bool {
                if (op->isComplete) {
                    return op->isComplete();
                }
                // Default: complete after estimated duration or max quantum
                return op->totalQuantumUsed >= op->estimatedDuration;
            };
            
            while (op->quantumUsed < quantum && !checkComplete()) {
                // Execute one time unit of work (if execute function is set)
                if (op->execute) {
                    op->execute();
                }
                op->quantumUsed++;
                op->totalQuantumUsed++;
                
                // Check for preemption
                for (int qIdx = 0; qIdx < static_cast<int>(op->currentQueue); qIdx++) {
                    pthread_mutex_lock(&queueMutexes[qIdx]);
                    if (!queues[qIdx].empty()) {
                        Operation* highPriOp = queues[qIdx].front();
                        pthread_mutex_unlock(&queueMutexes[qIdx]);
                        
                        if (shouldPreempt(*highPriOp, *op)) {
                            executePreemption(highPriOp, op);
                            break;
                        }
                    } else {
                        pthread_mutex_unlock(&queueMutexes[qIdx]);
                    }
                }
            }
            
            TimeUnit endTime = getCurrentTime();
            reportProgress(op->id, endTime - startTime);
            
            if (checkComplete()) {
                completeOperation(op->id, true);
            } else {
                // Quantum expired - handle demotion
                op->quantumExpirations++;
                op->quantumUsed = 0;
                
                int demotionThreshold = 0;
                switch (op->currentQueue) {
                    case SchedulerQueue::CRITICAL: demotionThreshold = Config::QUEUE_1_DEMOTION_THRESHOLD; break;
                    case SchedulerQueue::HIGH: demotionThreshold = Config::QUEUE_2_DEMOTION_THRESHOLD; break;
                    case SchedulerQueue::NORMAL: demotionThreshold = Config::QUEUE_3_DEMOTION_THRESHOLD; break;
                    default: demotionThreshold = 999; break;
                }
                
                if (op->quantumExpirations >= demotionThreshold) {
                    demoteOperation(op);
                } else {
                    // Re-add to same queue
                    int qIdx = static_cast<int>(op->currentQueue);
                    pthread_mutex_lock(&queueMutexes[qIdx]);
                    queues[qIdx].push_back(op);
                    pthread_mutex_unlock(&queueMutexes[qIdx]);
                }
            }
            
            pthread_mutex_lock(&currentOpMutex);
            currentOperation = nullptr;
            pthread_mutex_unlock(&currentOpMutex);
        }
        
        // Check for starvation periodically
        checkStarvation();
    }
}

void* Scheduler::agingLoop(void* arg) {
    Scheduler* scheduler = static_cast<Scheduler*>(arg);
    scheduler->runAging();
    return nullptr;
}

void Scheduler::runAging() {
    while (running) {
        // Apply aging every 100ms for faster response at high sim speeds
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!running) break;
        
        applyAging();
        recalculatePriorities();
    }
}

// ============================================================================
// Operation Submission and Management
// ============================================================================

int Scheduler::submitOperation(Operation op) {
    op.id = nextOperationId++;
    op.arrivalTime = getCurrentTime();
    op.waitTime = 0;
    op.quantumUsed = 0;
    op.totalQuantumUsed = 0;
    op.quantumExpirations = 0;
    op.isPreempted = false;
    op.hasGuaranteedService = false;
    op.preemptionCount = 0;
    
    // Calculate PIS and adjust priority
    op.predictedImpactScore = calculatePIS(op);
    
    // Adjust priority based on PIS (lower score = higher priority)
    int priorityAdjustment = static_cast<int>(op.predictedImpactScore * 10);
    op.priority = std::max(0, op.priority - priorityAdjustment);
    op.originalPriority = op.priority;
    
    // Determine queue
    op.currentQueue = determineInitialQueue(op);
    
    // Get predicted duration from historical data
    op.estimatedDuration = getPredictedDuration(op.type);
    
    // Store operation
    pthread_mutex_lock(&operationsMutex);
    allOperations[op.id] = op;
    Operation* opPtr = &allOperations[op.id];
    pthread_mutex_unlock(&operationsMutex);
    
    // Add to appropriate queue
    int queueIdx = static_cast<int>(op.currentQueue);
    pthread_mutex_lock(&queueMutexes[queueIdx]);
    queues[queueIdx].push_back(opPtr);
    pthread_mutex_unlock(&queueMutexes[queueIdx]);
    
    // Signal scheduler
    pthread_mutex_lock(&condMutex);
    pthread_cond_signal(&operationAvailable);
    pthread_mutex_unlock(&condMutex);
    
    metrics.totalOperationsScheduled++;
    metrics.operationsPending++;
    
    std::stringstream ss;
    ss << "Operation " << op.id << " submitted to queue " << queueIdx 
       << " with priority " << op.priority << ", PIS=" << std::fixed 
       << std::setprecision(3) << op.predictedImpactScore;
    logDecision(ss.str(), op.flightId, op.id);
    
    return op.id;
}

bool Scheduler::cancelOperation(int operationId) {
    pthread_mutex_lock(&operationsMutex);
    auto it = allOperations.find(operationId);
    if (it == allOperations.end()) {
        pthread_mutex_unlock(&operationsMutex);
        return false;
    }
    
    Operation* op = &it->second;
    int queueIdx = static_cast<int>(op->currentQueue);
    pthread_mutex_unlock(&operationsMutex);
    
    // Remove from queue
    pthread_mutex_lock(&queueMutexes[queueIdx]);
    auto& queue = queues[queueIdx];
    queue.erase(std::remove_if(queue.begin(), queue.end(),
                [operationId](Operation* o) { return o->id == operationId; }),
                queue.end());
    pthread_mutex_unlock(&queueMutexes[queueIdx]);
    
    logDecision("Operation cancelled", op->flightId, operationId);
    metrics.operationsPending--;
    return true;
}

Operation* Scheduler::getOperation(int operationId) {
    pthread_mutex_lock(&operationsMutex);
    auto it = allOperations.find(operationId);
    if (it != allOperations.end()) {
        Operation* op = &it->second;
        pthread_mutex_unlock(&operationsMutex);
        return op;
    }
    pthread_mutex_unlock(&operationsMutex);
    return nullptr;
}

void Scheduler::completeOperation(int operationId, bool success) {
    pthread_mutex_lock(&operationsMutex);
    auto it = allOperations.find(operationId);
    if (it != allOperations.end()) {
        Operation& op = it->second;
        op.actualDuration = getCurrentTime() - op.arrivalTime;
        
        // Update statistics (Layer 8)
        updateStatistics(op, op.actualDuration, success);
        metrics.operationsCompleted++;
        metrics.operationsPending--;
        
        // Restore priority if inherited
        if (op.hasPriorityInheritance) {
            restorePriority(&op);
        }
        
        std::stringstream ss;
        ss << "Operation " << operationId << " completed in " 
           << op.actualDuration << " time units (estimated: " 
           << op.estimatedDuration << ")";
        logDecision(ss.str(), op.flightId, operationId);
    }
    pthread_mutex_unlock(&operationsMutex);
}

void Scheduler::reportProgress(int operationId, TimeUnit timeUsed) {
    pthread_mutex_lock(&operationsMutex);
    auto it = allOperations.find(operationId);
    if (it != allOperations.end()) {
        it->second.waitTime = getCurrentTime() - it->second.arrivalTime - timeUsed;
    }
    pthread_mutex_unlock(&operationsMutex);
}

void Scheduler::updateSystemState(int totalFlights, int totalConnectingPassengers,
                                   int totalResources, double weatherSeverity) {
    pthread_rwlock_wrlock(&systemStateLock);
    systemState.totalFlights = totalFlights;
    systemState.totalConnectingPassengers = totalConnectingPassengers;
    systemState.totalResources = totalResources;
    systemState.weatherSeverity = weatherSeverity;
    pthread_rwlock_unlock(&systemStateLock);
}

void Scheduler::notifyResourceAvailable(int resourceId) {
    // Check for priority inheritance chains waiting on this resource
    pthread_mutex_lock(&inheritanceMutex);
    for (auto& pair : inheritanceChains) {
        if (pair.second.blockingResourceId == resourceId) {
            // Restore priorities
            pthread_mutex_lock(&operationsMutex);
            auto it = allOperations.find(pair.first);
            if (it != allOperations.end()) {
                restorePriority(&it->second);
            }
            pthread_mutex_unlock(&operationsMutex);
        }
    }
    pthread_mutex_unlock(&inheritanceMutex);
}

SchedulerMetrics Scheduler::getMetrics() const {
    return metrics;
}

std::vector<LogEntry> Scheduler::getSchedulingLog() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&logMutex));
    std::vector<LogEntry> log = schedulingLog;
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&logMutex));
    return log;
}

// ============================================================================
// LAYER 1: Multi-Level Feedback Queue
// ============================================================================

SchedulerQueue Scheduler::determineInitialQueue(const Operation& op) {
    // Emergency operations always go to Queue 0
    if (op.type == OperationType::EMERGENCY_RESPONSE) {
        return SchedulerQueue::EMERGENCY;
    }
    
    // Priority-based queue assignment
    if (op.priority <= Config::QUEUE_0_PRIORITY_MAX) {
        return SchedulerQueue::EMERGENCY;
    } else if (op.priority <= Config::QUEUE_1_PRIORITY_MAX) {
        return SchedulerQueue::CRITICAL;
    } else if (op.priority <= Config::QUEUE_2_PRIORITY_MAX) {
        return SchedulerQueue::HIGH;
    } else if (op.priority <= Config::QUEUE_3_PRIORITY_MAX) {
        return SchedulerQueue::NORMAL;
    }
    return SchedulerQueue::LOW;
}

void Scheduler::demoteOperation(Operation* op) {
    // Cannot demote from Queue 4
    if (op->currentQueue == SchedulerQueue::LOW) {
        int qIdx = static_cast<int>(op->currentQueue);
        pthread_mutex_lock(&queueMutexes[qIdx]);
        queues[qIdx].push_back(op);
        pthread_mutex_unlock(&queueMutexes[qIdx]);
        return;
    }
    
    int oldQueue = static_cast<int>(op->currentQueue);
    int newQueue = oldQueue + 1;
    op->currentQueue = static_cast<SchedulerQueue>(newQueue);
    op->quantumExpirations = 0;
    
    pthread_mutex_lock(&queueMutexes[newQueue]);
    queues[newQueue].push_back(op);
    pthread_mutex_unlock(&queueMutexes[newQueue]);
    
    metrics.totalDemotions++;
    
    std::stringstream ss;
    ss << "Operation " << op->id << " demoted from queue " << oldQueue 
       << " to queue " << newQueue;
    logDecision(ss.str(), op->flightId, op->id);
}

void Scheduler::promoteOperation(Operation* op, SchedulerQueue targetQueue) {
    int oldQueue = static_cast<int>(op->currentQueue);
    int newQueue = static_cast<int>(targetQueue);
    
    // Remove from old queue
    pthread_mutex_lock(&queueMutexes[oldQueue]);
    auto& queue = queues[oldQueue];
    queue.erase(std::remove(queue.begin(), queue.end(), op), queue.end());
    pthread_mutex_unlock(&queueMutexes[oldQueue]);
    
    op->currentQueue = targetQueue;
    
    // Add to new queue
    pthread_mutex_lock(&queueMutexes[newQueue]);
    queues[newQueue].push_back(op);
    pthread_mutex_unlock(&queueMutexes[newQueue]);
    
    metrics.totalPromotions++;
    
    std::stringstream ss;
    ss << "Operation " << op->id << " promoted from queue " << oldQueue 
       << " to queue " << newQueue;
    logDecision(ss.str(), op->flightId, op->id);
}

// ============================================================================
// LAYER 2: Predictive Impact Score (PIS)
// ============================================================================

double Scheduler::calculatePIS(const Operation& op) {
    double dpf = calcDelayPropagationFactor(op);
    double crf = calcConnectionRiskFactor(op);
    double rui = calcResourceUtilizationImpact(op);
    double wrf = calcWeatherRiskFactor(op);
    double fcf = calcFuelCriticalityFactor(op);
    
    return Config::PIS_ALPHA * dpf +
           Config::PIS_BETA * crf +
           Config::PIS_GAMMA * rui +
           Config::PIS_DELTA * wrf +
           Config::PIS_EPSILON * fcf;
}

double Scheduler::calcDelayPropagationFactor(const Operation& op) {
    pthread_rwlock_rdlock(&systemStateLock);
    double factor = 0.0;
    if (systemState.totalFlights > 0) {
        factor = static_cast<double>(systemState.affectedFlights) / 
                 systemState.totalFlights;
    }
    pthread_rwlock_unlock(&systemStateLock);
    return factor;
}

double Scheduler::calcConnectionRiskFactor(const Operation& op) {
    pthread_rwlock_rdlock(&systemStateLock);
    double factor = 0.0;
    if (systemState.totalConnectingPassengers > 0) {
        factor = static_cast<double>(systemState.passengersAtRisk) / 
                 systemState.totalConnectingPassengers;
    }
    pthread_rwlock_unlock(&systemStateLock);
    return factor;
}

double Scheduler::calcResourceUtilizationImpact(const Operation& op) {
    pthread_rwlock_rdlock(&systemStateLock);
    double factor = 0.0;
    if (systemState.totalResources > 0) {
        factor = static_cast<double>(systemState.blockedResources) / 
                 systemState.totalResources;
    }
    pthread_rwlock_unlock(&systemStateLock);
    return factor;
}

double Scheduler::calcWeatherRiskFactor(const Operation& op) {
    pthread_rwlock_rdlock(&systemStateLock);
    double factor = (systemState.weatherSeverity * systemState.timeWindowAffected) / 
                    systemState.totalResources;
    if (factor > 1.0) factor = 1.0;
    pthread_rwlock_unlock(&systemStateLock);
    return factor;
}

double Scheduler::calcFuelCriticalityFactor(const Operation& op) {
    // This would normally be calculated from flight data
    // For now, return 0 unless it's a fuel emergency
    if (op.type == OperationType::EMERGENCY_RESPONSE) {
        return 0.8; // High criticality for emergencies
    }
    return 0.0;
}

// ============================================================================
// LAYER 3: Exponential Aging
// ============================================================================

void Scheduler::applyAging() {
    TimeUnit currentTime = getCurrentTime();
    
    // Apply aging to operations in queues 1-4 (not emergency queue)
    for (int qIdx = 1; qIdx < 5; qIdx++) {
        pthread_mutex_lock(&queueMutexes[qIdx]);
        
        std::vector<Operation*> toPromote;
        
        for (Operation* op : queues[qIdx]) {
            TimeUnit waitTime = currentTime - op->arrivalTime;
            double ageIncrement = calculateAgeIncrement(*op);
            
            // Apply priority boost (lower priority number = higher priority)
            int priorityBoost = static_cast<int>(ageIncrement * Config::AGE_WEIGHT);
            op->priority = std::max(0, op->priority - priorityBoost);
            
            op->waitTime = waitTime;
            
            // Check if priority has dropped below threshold for promotion
            // Operations in Queue 3 or 4 with boosted priority should be promoted
            if (qIdx >= 2 && op->priority <= Config::AGING_PROMOTION_PRIORITY_THRESHOLD) {
                toPromote.push_back(op);
            }
        }
        
        // Remove operations that will be promoted
        for (Operation* op : toPromote) {
            queues[qIdx].erase(std::remove(queues[qIdx].begin(), queues[qIdx].end(), op), queues[qIdx].end());
        }
        
        pthread_mutex_unlock(&queueMutexes[qIdx]);
        
        // Promote operations one level up (outside the lock to avoid deadlock)
        for (Operation* op : toPromote) {
            // Promote one level up: Queue 4→3, Queue 3→2, Queue 2→1
            SchedulerQueue targetQueue = static_cast<SchedulerQueue>(qIdx - 1);
            promoteOperation(op, targetQueue);
        }
    }
}

double Scheduler::calculateAgeIncrement(const Operation& op) {
    double timeConstant = getTimeConstant(op.currentQueue);
    double waitTime = static_cast<double>(op.waitTime);
    
    // Age_Increment = Base_Age_Rate * e^(Wait_Time / Time_Constant)
    return Config::BASE_AGE_RATE * std::exp(waitTime / timeConstant);
}

double Scheduler::getTimeConstant(SchedulerQueue queue) {
    switch (queue) {
        case SchedulerQueue::EMERGENCY: return 10000.0; // Very slow aging (effectively none)
        case SchedulerQueue::CRITICAL: return Config::QUEUE_1_TIME_CONSTANT;
        case SchedulerQueue::HIGH: return Config::QUEUE_2_TIME_CONSTANT;
        case SchedulerQueue::NORMAL: return Config::QUEUE_3_TIME_CONSTANT;
        case SchedulerQueue::LOW: return Config::QUEUE_4_TIME_CONSTANT;
    }
    return Config::QUEUE_3_TIME_CONSTANT;
}

// ============================================================================
// LAYER 4: Dynamic Quantum Adjustment
// ============================================================================

TimeUnit Scheduler::calculateActualQuantum(const Operation& op) {
    // Emergency queue has unlimited quantum
    if (op.currentQueue == SchedulerQueue::EMERGENCY) {
        return 999999; // Effectively unlimited
    }
    
    TimeUnit baseQuantum = queueQuantums[static_cast<int>(op.currentQueue)];
    double loadFactor = getLoadFactor();
    double operationFactor = getOperationFactor(op.type);
    
    TimeUnit actualQuantum = static_cast<TimeUnit>(baseQuantum * loadFactor * operationFactor);
    return std::max(actualQuantum, static_cast<TimeUnit>(10)); // Minimum 10 time units
}

double Scheduler::getLoadFactor() {
    // Calculate based on total operations in all queues
    int totalOps = 0;
    for (int i = 0; i < 5; i++) {
        pthread_mutex_lock(&queueMutexes[i]);
        totalOps += queues[i].size();
        pthread_mutex_unlock(&queueMutexes[i]);
    }
    
    // Load_Factor = 1 - (Active_Operations / Max_Operations)^2
    double ratio = static_cast<double>(totalOps) / Config::MAX_OPERATIONS_IN_QUEUE;
    double loadFactor = 1.0 - (ratio * ratio);
    
    // Clamp to range [0.4, 1.0]
    return std::max(Config::LOAD_FACTOR_MIN, std::min(loadFactor, Config::LOAD_FACTOR_MAX));
}

double Scheduler::getOperationFactor(OperationType type) {
    switch (type) {
        case OperationType::TAXIWAY_MOVEMENT:
            return Config::OPERATION_FACTOR_SIMPLE;
        
        case OperationType::RUNWAY_LANDING:
        case OperationType::RUNWAY_TAKEOFF:
        case OperationType::GATE_ASSIGNMENT:
            return Config::OPERATION_FACTOR_COMPLEX;
        
        default:
            return Config::OPERATION_FACTOR_MEDIUM;
    }
}

// ============================================================================
// LAYER 5: Priority Inheritance
// ============================================================================

void Scheduler::applyPriorityInheritance(Operation* blocked, Operation* blocker) {
    pthread_mutex_lock(&inheritanceMutex);
    
    // Boost blocker's priority to blocked operation's level
    if (blocker->priority > blocked->priority) {
        blocker->originalPriority = blocker->priority;
        blocker->priority = blocked->priority;
        blocker->hasPriorityInheritance = true;
        blocker->inheritedPriority = blocked->priority;
        
        // Track inheritance chain
        PriorityInheritanceChain chain;
        chain.operationId = blocker->id;
        chain.originalPriority = blocker->originalPriority;
        chain.inheritedPriority = blocker->priority;
        chain.waitingOperations.push_back(blocked->id);
        
        inheritanceChains[blocker->id] = chain;
        
        metrics.priorityInheritances++;
        
        std::stringstream ss;
        ss << "Priority inheritance: Operation " << blocker->id 
           << " boosted from priority " << blocker->originalPriority 
           << " to " << blocker->priority;
        logDecision(ss.str(), blocker->flightId, blocker->id);
    }
    
    pthread_mutex_unlock(&inheritanceMutex);
}

void Scheduler::restorePriority(Operation* op) {
    pthread_mutex_lock(&inheritanceMutex);
    
    if (op->hasPriorityInheritance) {
        op->priority = op->originalPriority;
        op->hasPriorityInheritance = false;
        
        // Remove from inheritance chain
        inheritanceChains.erase(op->id);
        
        std::stringstream ss;
        ss << "Priority restored: Operation " << op->id 
           << " back to priority " << op->priority;
        logDecision(ss.str(), op->flightId, op->id);
    }
    
    pthread_mutex_unlock(&inheritanceMutex);
}

// ============================================================================
// LAYER 6: Cost-Benefit Preemption
// ============================================================================

double Scheduler::calculatePreemptionBenefit(const Operation& highPriority) {
    // Benefit = Urgency * Delay_Cost
    double urgency = 1.0 - (highPriority.priority / 100.0);
    double delayCost = highPriority.waitTime * 0.1; // Cost per time unit of waiting
    
    return urgency * delayCost;
}

double Scheduler::calculatePreemptionCost(const Operation& lowPriority) {
    // Cost = Progress_Lost * Context_Switch_Cost + Resource_Config_Cost + Downstream_Impact
    double progressLost = lowPriority.quantumUsed * 0.5;
    double contextSwitchCost = 5.0;
    double resourceConfigCost = 2.0;
    double downstreamImpact = lowPriority.predictedImpactScore * 10.0;
    
    return progressLost + contextSwitchCost + resourceConfigCost + downstreamImpact;
}

bool Scheduler::shouldPreempt(const Operation& highPriority, const Operation& lowPriority) {
    // Don't preempt emergency operations
    if (lowPriority.currentQueue == SchedulerQueue::EMERGENCY) {
        return false;
    }
    
    double benefit = calculatePreemptionBenefit(highPriority);
    double cost = calculatePreemptionCost(lowPriority);
    
    // Preempt if benefit > 1.5 * cost
    return benefit > (Config::PREEMPTION_THRESHOLD * cost);
}

void Scheduler::executePreemption(Operation* highPriority, Operation* lowPriority) {
    // Save state of preempted operation
    saveOperationState(lowPriority);
    lowPriority->isPreempted = true;
    lowPriority->preemptionCount++;
    
    // Give compensation to preempted operation (reduce demotion penalty)
    if (lowPriority->quantumExpirations > 0) {
        lowPriority->quantumExpirations--;
    }
    
    // Move preempted operation back to queue
    int qIdx = static_cast<int>(lowPriority->currentQueue);
    pthread_mutex_lock(&queueMutexes[qIdx]);
    queues[qIdx].push_front(lowPriority); // Add to front for fairness
    pthread_mutex_unlock(&queueMutexes[qIdx]);
    
    metrics.totalPreemptions++;
    
    std::stringstream ss;
    ss << "Preemption: Operation " << lowPriority->id 
       << " preempted by operation " << highPriority->id;
    logDecision(ss.str(), lowPriority->flightId, lowPriority->id);
}

void Scheduler::saveOperationState(Operation* op) {
    // State is already tracked in Operation struct
    // Additional state could be saved here if needed
}

void Scheduler::restoreOperationState(Operation* op) {
    op->isPreempted = false;
}

// ============================================================================
// LAYER 7: Starvation Prevention
// ============================================================================

void Scheduler::checkStarvation() {
    TimeUnit currentTime = getCurrentTime();
    
    // Check queues 2, 3, 4 for starvation
    for (int qIdx = 2; qIdx < 5; qIdx++) {
        pthread_mutex_lock(&queueMutexes[qIdx]);
        
        for (Operation* op : queues[qIdx]) {
            TimeUnit waitTime = currentTime - op->arrivalTime;
            TimeUnit maxWait = getMaxWaitThreshold(static_cast<SchedulerQueue>(qIdx));
            
            if (waitTime > maxWait && !op->hasGuaranteedService) {
                pthread_mutex_unlock(&queueMutexes[qIdx]);
                promoteStarvingOperation(op);
                pthread_mutex_lock(&queueMutexes[qIdx]);
            }
        }
        
        pthread_mutex_unlock(&queueMutexes[qIdx]);
    }
}

TimeUnit Scheduler::getMaxWaitThreshold(SchedulerQueue queue) {
    switch (queue) {
        case SchedulerQueue::HIGH: return Config::QUEUE_2_MAX_WAIT;
        case SchedulerQueue::NORMAL: return Config::QUEUE_3_MAX_WAIT;
        case SchedulerQueue::LOW: return Config::QUEUE_4_MAX_WAIT;
        default: return Config::ABSOLUTE_MAX_WAIT;
    }
}

void Scheduler::promoteStarvingOperation(Operation* op) {
    op->hasGuaranteedService = true;
    promoteOperation(op, SchedulerQueue::CRITICAL);
    
    metrics.starvationPreventions++;
    
    std::stringstream ss;
    ss << "Starvation prevention: Operation " << op->id 
       << " promoted to critical queue with guaranteed service";
    logDecision(ss.str(), op->flightId, op->id);
}

// ============================================================================
// LAYER 8: Adaptive Learning
// ============================================================================

void Scheduler::updateStatistics(const Operation& op, TimeUnit actualTime, bool success) {
    pthread_rwlock_wrlock(&statsLock);
    
    auto it = operationStatistics.find(op.type);
    if (it != operationStatistics.end()) {
        // Calculate resource consumption (simplified)
        double resourceUsage = 1.0;
        
        it->second.update(actualTime, success, resourceUsage);
    }
    
    pthread_rwlock_unlock(&statsLock);
    
    // Periodically adjust thresholds
    if (metrics.totalOperationsScheduled % 100 == 0) {
        adjustThresholds();
    }
}

TimeUnit Scheduler::getPredictedDuration(OperationType type) {
    pthread_rwlock_rdlock(&statsLock);
    
    auto it = operationStatistics.find(type);
    TimeUnit prediction = 60; // Default
    
    if (it != operationStatistics.end() && it->second.totalExecutions > 0) {
        prediction = static_cast<TimeUnit>(it->second.avgCompletionTime);
    }
    
    pthread_rwlock_unlock(&statsLock);
    return prediction;
}

void Scheduler::adjustThresholds() {
    pthread_rwlock_rdlock(&statsLock);
    
    // Analyze historical data and adjust parameters
    // This is a simplified implementation
    
    // Check if operations are completing faster than expected
    int fasterCount = 0, slowerCount = 0;
    for (const auto& pair : operationStatistics) {
        if (pair.second.totalExecutions > 10) {
            // Compare variance to adjust quantum
            if (pair.second.varianceCompletionTime < 100) {
                fasterCount++;
            } else {
                slowerCount++;
            }
        }
    }
    
    pthread_rwlock_unlock(&statsLock);
    
    // Could adjust quantum values here based on analysis
}

// ============================================================================
// Helper Functions
// ============================================================================

TimeUnit Scheduler::getCurrentTime() {
    static auto startTime = SimClock::now();
    auto now = SimClock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
}

Operation* Scheduler::selectNextOperation() {
    // Select from highest priority non-empty queue
    for (int qIdx = 0; qIdx < 5; qIdx++) {
        pthread_mutex_lock(&queueMutexes[qIdx]);
        
        if (!queues[qIdx].empty()) {
            // Sort by priority within queue (lower priority value = higher priority)
            std::sort(queues[qIdx].begin(), queues[qIdx].end(),
                     [](Operation* a, Operation* b) {
                         return a->priority < b->priority;
                     });
            
            Operation* op = queues[qIdx].front();
            queues[qIdx].pop_front();
            pthread_mutex_unlock(&queueMutexes[qIdx]);
            
            restoreOperationState(op);
            return op;
        }
        
        pthread_mutex_unlock(&queueMutexes[qIdx]);
    }
    
    return nullptr;
}

void Scheduler::recalculatePriorities() {
    // Recalculate PIS and priorities for all queued operations
    for (int qIdx = 0; qIdx < 5; qIdx++) {
        pthread_mutex_lock(&queueMutexes[qIdx]);
        
        for (Operation* op : queues[qIdx]) {
            op->predictedImpactScore = calculatePIS(*op);
            
            // Adjust priority based on new PIS (don't go above original)
            int pisAdjustment = static_cast<int>(op->predictedImpactScore * 10);
            op->priority = std::max(0, op->originalPriority - pisAdjustment);
        }
        
        pthread_mutex_unlock(&queueMutexes[qIdx]);
    }
}

void Scheduler::logDecision(const std::string& message, int flightId, int opId) {
    pthread_mutex_lock(&logMutex);
    
    LogEntry entry;
    entry.timestamp = getCurrentTime();
    entry.level = LogLevel::LOG_INFO;
    entry.subsystem = "SCHEDULER";
    entry.message = message;
    entry.relatedFlightId = flightId;
    entry.relatedOperationId = opId;
    
    schedulingLog.push_back(entry);
    
    // Keep log size manageable
    if (schedulingLog.size() > 10000) {
        schedulingLog.erase(schedulingLog.begin(), schedulingLog.begin() + 5000);
    }
    
    pthread_mutex_unlock(&logMutex);
}

Operation* Scheduler::getNextOperation() {
    return selectNextOperation();
}
