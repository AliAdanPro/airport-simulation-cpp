/**
 * Smart Airport Operations Management System
 * ground_services.h - Ground Services and Resource Management
 * 
 * Implements service dependency graph with:
 * - All 7 resource types with constraints
 * - Service time variability
 * - Equipment failure simulation
 */

#ifndef GROUND_SERVICES_H
#define GROUND_SERVICES_H

#include "types.h"
#include "config.h"
#include "data_manager.h"
#include "scheduler.h"
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <random>

/**
 * Resource Pool
 * Manages a pool of resources of a specific type
 */
class ResourcePool {
public:
    ResourcePool(ResourceType type, int count, DataManager* dm);
    
    // Acquire a resource (returns resource ID or -1 if none available)
    int acquire(int flightId);
    
    // Release a resource
    void release(int resourceId);
    
    // Check availability
    int getAvailableCount() const;
    bool isAvailable(int resourceId) const;
    
    // Equipment failure
    void simulateFailure(int resourceId, TimeUnit duration);
    void clearFailure(int resourceId);
    
private:
    ResourceType type;
    DataManager* dataManager;
    std::map<int, bool> available;      // resourceId -> available
    std::map<int, int> assignments;     // resourceId -> flightId
    std::map<int, TimeUnit> failureEnd; // resourceId -> failure end time
    mutable pthread_mutex_t mutex;
};

/**
 * Service Dependency Graph
 * Defines the order and dependencies between ground services
 */
struct ServiceDependency {
    ServiceType service;
    std::vector<ServiceType> dependencies;
    std::vector<ResourceType> requiredResources;
    int baseTimeMin;    // seconds
    int baseTimeMax;
    bool canOverlap;    // Can run in parallel with previous service
};

/**
 * Ground Service Manager
 * Coordinates all ground operations for aircraft
 */
class GroundServiceManager {
public:
    GroundServiceManager(DataManager* dm, Scheduler* sched);
    ~GroundServiceManager();
    
    void initialize();
    
    // Start turnaround for a flight
    void startTurnaround(int flightId, AircraftType aircraftType, 
                         FlightType flightType, bool requiresDeicing);
    
    // Update service progress
    void updateServices(TimeUnit simulationTime);
    
    // Check if turnaround is complete
    bool isTurnaroundComplete(int flightId);
    double getTurnaroundProgress(int flightId);
    
    // Get current service for flight
    std::vector<ServiceTask> getActiveServices(int flightId);
    
    // Resource management
    int requestResource(ResourceType type, int flightId);
    void releaseResource(ResourceType type, int resourceId);
    int getAvailableResources(ResourceType type);
    
    // Simulate equipment failure
    void simulateRandomFailures();
    
private:
    DataManager* dataManager;
    Scheduler* scheduler;
    
    // Resource pools
    std::map<ResourceType, std::unique_ptr<ResourcePool>> resourcePools;
    
    // Service dependencies
    std::vector<ServiceDependency> dependencyGraph;
    
    // Active turnarounds: flightId -> services
    std::map<int, std::vector<ServiceTask>> activeTurnarounds;
    pthread_mutex_t turnaroundMutex;
    
    // Random number generator for variability
    std::mt19937 rng;
    
    // Build dependency graph
    void buildDependencyGraph();
    
    // Create service tasks for flight
    std::vector<ServiceTask> createServiceTasks(int flightId, AircraftType type,
                                                 FlightType flightType, bool requiresDeicing);
    
    // Calculate service duration with variability
    TimeUnit calculateServiceDuration(ServiceType type, AircraftType aircraftType,
                                       FlightType flightType, double crewSkill,
                                       double weatherMultiplier);
    
    // Check if service dependencies are met
    bool areDependenciesMet(int flightId, ServiceType service);
    
    // Get next available service
    ServiceType getNextService(int flightId);
    
    // Get required resources for service
    std::vector<ResourceType> getRequiredResources(ServiceType type, FlightType flightType);
};

#endif // GROUND_SERVICES_H
