/**
 * Smart Airport Operations Management System
 * scheduler.h - HMFQ-PPRA Scheduling Algorithm
 * 
 * Hybrid Multi-Level Feedback Queue with Predictive Priority
 * Round-Robin and Aging (HMFQ-PPRA)
 * 
 * Implements all 8 layers:
 * 1. Multi-Level Feedback Queue (5 queues)
 * 2. Predictive Impact Score (PIS)
 * 3. Exponential Aging
 * 4. Dynamic Quantum Adjustment
 * 5. Priority Inheritance
 * 6. Cost-Benefit Preemption
 * 7. Starvation Prevention
 * 8. Adaptive Learning
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "config.h"
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>

// Forward declarations
class DataManager;

/**
 * OperationStats - Historical statistics for adaptive learning
 */
struct OperationStats {
    OperationType type;
    double avgCompletionTime;
    double varianceCompletionTime;
    double historicalSuccessRate;
    double avgResourceConsumption;
    int totalExecutions;
    
    void update(TimeUnit actualTime, bool success, double resourceUsage);
};

/**
 * PriorityInheritanceChain - Track priority inheritance
 */
struct PriorityInheritanceChain {
    int operationId;
    int originalPriority;
    int inheritedPriority;
    int blockingResourceId;
    std::vector<int> waitingOperations;
};

/**
 * SchedulerMetrics - Performance metrics for the scheduler
 */
struct SchedulerMetrics {
    int totalOperationsScheduled = 0;
    int operationsCompleted = 0;
    int operationsPending = 0;

    int totalPreemptions = 0;
    int totalPromotions = 0;
    int totalDemotions = 0;
    int starvationPreventions = 0;
    int priorityInheritances = 0;
    
    double avgWaitTime = 0.0;
    double avgTurnaroundTime = 0.0;
    double fairnessIndex = 0.0;
    
    std::map<SchedulerQueue, int> queueLengths;
    std::map<OperationType, double> avgWaitByType;
};

/**
 * HMFQ-PPRA Scheduler
 * 
 * Main scheduler class implementing all 8 layers of the algorithm
 */
class Scheduler {
public:
    Scheduler();
    ~Scheduler();
    
    // Initialize the scheduler
    void initialize(DataManager* dataManager);
    
    // Start/stop the scheduler thread
    void start();
    void stop();
    
    // Submit a new operation
    int submitOperation(Operation op);
    
    // Cancel an operation
    bool cancelOperation(int operationId);
    
    // Get operation status
    Operation* getOperation(int operationId);
    
    // Get scheduler metrics
    SchedulerMetrics getMetrics() const;
    
    // Update system state for PIS calculation
    void updateSystemState(int totalFlights, int totalConnectingPassengers,
                          int totalResources, double weatherSeverity);
    
    // Notify of resource availability
    void notifyResourceAvailable(int resourceId);
    
    // Get next operation to execute (called by worker threads)
    Operation* getNextOperation();
    
    // Mark operation as complete
    void completeOperation(int operationId, bool success);
    
    // Report operation progress
    void reportProgress(int operationId, TimeUnit timeUsed);
    
    // Get scheduling decision log
    std::vector<LogEntry> getSchedulingLog() const;
    
private:
    // ========================================================================
    // LAYER 1: Multi-Level Feedback Queue
    // ========================================================================
    
    // 5 priority queues
    std::deque<Operation*> queues[5];
    pthread_mutex_t queueMutexes[5];
    
    // Queue quantum values
    TimeUnit queueQuantums[5];
    
    // Determine initial queue for operation
    SchedulerQueue determineInitialQueue(const Operation& op);
    
    // Demote operation to lower priority queue
    void demoteOperation(Operation* op);
    
    // Promote operation to higher priority queue
    void promoteOperation(Operation* op, SchedulerQueue targetQueue);
    
    // ========================================================================
    // LAYER 2: Predictive Impact Score (PIS)
    // ========================================================================
    
    // System state for PIS calculation
    struct SystemState {
        int totalFlights = 0;
        int affectedFlights = 0;
        int totalConnectingPassengers = 0;
        int passengersAtRisk = 0;
        int totalResources = 0;
        int blockedResources = 0;
        double weatherSeverity = 0.0;
        double timeWindowAffected = 0.0;
    } systemState;
    
    pthread_rwlock_t systemStateLock;
    
    // Calculate PIS for an operation
    double calculatePIS(const Operation& op);
    
    // Calculate individual PIS components
    double calcDelayPropagationFactor(const Operation& op);
    double calcConnectionRiskFactor(const Operation& op);
    double calcResourceUtilizationImpact(const Operation& op);
    double calcWeatherRiskFactor(const Operation& op);
    double calcFuelCriticalityFactor(const Operation& op);
    
    // ========================================================================
    // LAYER 3: Exponential Aging
    // ========================================================================
    
    // Calculate age increment for operation
    double calculateAgeIncrement(const Operation& op);
    
    // Apply aging to all operations (called periodically)
    void applyAging();
    
    // Get time constant for queue
    double getTimeConstant(SchedulerQueue queue);
    
    // ========================================================================
    // LAYER 4: Dynamic Quantum Adjustment
    // ========================================================================
    
    // Calculate actual quantum for operation
    TimeUnit calculateActualQuantum(const Operation& op);
    
    // Get load factor based on system state
    double getLoadFactor();
    
    // Get operation factor
    double getOperationFactor(OperationType type);
    
    // ========================================================================
    // LAYER 5: Priority Inheritance
    // ========================================================================
    
    // Priority inheritance chains
    std::map<int, PriorityInheritanceChain> inheritanceChains;
    pthread_mutex_t inheritanceMutex;
    
    // Apply priority inheritance when blocked
    void applyPriorityInheritance(Operation* blocked, Operation* blocker);
    
    // Restore original priority after resource release
    void restorePriority(Operation* op);
    
    // ========================================================================
    // LAYER 6: Cost-Benefit Preemption
    // ========================================================================
    
    // Calculate preemption benefit
    double calculatePreemptionBenefit(const Operation& highPriority);
    
    // Calculate preemption cost
    double calculatePreemptionCost(const Operation& lowPriority);
    
    // Decide whether to preempt
    bool shouldPreempt(const Operation& highPriority, const Operation& lowPriority);
    
    // Execute preemption
    void executePreemption(Operation* highPriority, Operation* lowPriority);
    
    // Save operation state for later resumption
    void saveOperationState(Operation* op);
    
    // Restore operation state
    void restoreOperationState(Operation* op);
    
    // ========================================================================
    // LAYER 7: Starvation Prevention
    // ========================================================================
    
    // Check for starving operations
    void checkStarvation();
    
    // Get max wait threshold for queue
    TimeUnit getMaxWaitThreshold(SchedulerQueue queue);
    
    // Temporarily promote starving operation
    void promoteStarvingOperation(Operation* op);
    
    // ========================================================================
    // LAYER 8: Adaptive Learning
    // ========================================================================
    
    // Historical statistics by operation type
    std::map<OperationType, OperationStats> operationStatistics;
    pthread_rwlock_t statsLock;
    
    // Update statistics after operation completion
    void updateStatistics(const Operation& op, TimeUnit actualTime, bool success);
    
    // Get predicted duration from historical data
    TimeUnit getPredictedDuration(OperationType type);
    
    // Adjust queue thresholds based on learning
    void adjustThresholds();
    
    // ========================================================================
    // General Scheduler Components
    // ========================================================================
    
    // Operation storage
    std::map<int, Operation> allOperations;
    pthread_mutex_t operationsMutex;
    std::atomic<int> nextOperationId{1};
    
    // Currently running operation
    Operation* currentOperation = nullptr;
    pthread_mutex_t currentOpMutex;
    
    // Scheduler control
    std::atomic<bool> running{false};
    pthread_t schedulerThread;
    pthread_t agingThread;
    pthread_cond_t operationAvailable;
    pthread_mutex_t condMutex;
    
    // Scheduler main loop
    static void* schedulerLoop(void* arg);
    void runScheduler();
    
    // Aging thread loop
    static void* agingLoop(void* arg);
    void runAging();
    
    // Logging
    std::vector<LogEntry> schedulingLog;
    pthread_mutex_t logMutex;
    void logDecision(const std::string& message, int flightId = -1, int opId = -1);
    
    // Data manager reference
    DataManager* dataManager = nullptr;
    
    // Metrics
    SchedulerMetrics metrics;
    
    // Get current simulation time
    TimeUnit getCurrentTime();
    
    // Select best operation from queues
    Operation* selectNextOperation();
    
    // Recalculate priorities for all operations
    void recalculatePriorities();
};

#endif // SCHEDULER_H
