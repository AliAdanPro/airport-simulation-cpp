/**
 * Smart Airport Operations Management System
 * ground_services.cpp - Ground Services Implementation
 */

#include "ground_services.h"
#include <algorithm>
#include <chrono>

// ============================================================================
// ResourcePool Implementation
// ============================================================================

ResourcePool::ResourcePool(ResourceType type, int count, DataManager* dm)
    : type(type), dataManager(dm) {
    pthread_mutex_init(&mutex, nullptr);
    
    for (int i = 0; i < count; i++) {
        available[i] = true;
        dataManager->setResourceAvailable(type, i, true);
    }
}

int ResourcePool::acquire(int flightId) {
    pthread_mutex_lock(&mutex);
    
    TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    for (auto& pair : available) {
        // Check if in failure
        auto failIt = failureEnd.find(pair.first);
        if (failIt != failureEnd.end() && failIt->second > currentTime) {
            continue;
        }
        
        if (pair.second) {
            pair.second = false;
            assignments[pair.first] = flightId;
            dataManager->setResourceAvailable(type, pair.first, false);
            pthread_mutex_unlock(&mutex);
            return pair.first;
        }
    }
    
    pthread_mutex_unlock(&mutex);
    return -1;
}

void ResourcePool::release(int resourceId) {
    pthread_mutex_lock(&mutex);
    
    auto it = available.find(resourceId);
    if (it != available.end()) {
        it->second = true;
        assignments.erase(resourceId);
        dataManager->setResourceAvailable(type, resourceId, true);
    }
    
    pthread_mutex_unlock(&mutex);
}

int ResourcePool::getAvailableCount() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mutex));
    int count = 0;
    for (const auto& pair : available) {
        if (pair.second) count++;
    }
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mutex));
    return count;
}

bool ResourcePool::isAvailable(int resourceId) const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mutex));
    bool avail = false;
    auto it = available.find(resourceId);
    if (it != available.end()) {
        avail = it->second;
    }
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mutex));
    return avail;
}

void ResourcePool::simulateFailure(int resourceId, TimeUnit duration) {
    pthread_mutex_lock(&mutex);
    
    TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    failureEnd[resourceId] = currentTime + duration;
    
    // Mark as unavailable
    auto it = available.find(resourceId);
    if (it != available.end()) {
        it->second = false;
        dataManager->setResourceAvailable(type, resourceId, false);
    }
    
    pthread_mutex_unlock(&mutex);
}

void ResourcePool::clearFailure(int resourceId) {
    pthread_mutex_lock(&mutex);
    failureEnd.erase(resourceId);
    
    // Make available again if not assigned
    auto assignIt = assignments.find(resourceId);
    if (assignIt == assignments.end()) {
        available[resourceId] = true;
        dataManager->setResourceAvailable(type, resourceId, true);
    }
    pthread_mutex_unlock(&mutex);
}

// ============================================================================
// GroundServiceManager Implementation
// ============================================================================

GroundServiceManager::GroundServiceManager(DataManager* dm, Scheduler* sched)
    : dataManager(dm), scheduler(sched), rng(std::random_device{}()) {
    pthread_mutex_init(&turnaroundMutex, nullptr);
}

GroundServiceManager::~GroundServiceManager() {
    pthread_mutex_destroy(&turnaroundMutex);
}

void GroundServiceManager::initialize() {
    // Create resource pools
    resourcePools[ResourceType::FUEL_TRUCK] = 
        std::make_unique<ResourcePool>(ResourceType::FUEL_TRUCK, Config::NUM_FUEL_TRUCKS, dataManager);
    resourcePools[ResourceType::BAGGAGE_CART] = 
        std::make_unique<ResourcePool>(ResourceType::BAGGAGE_CART, Config::NUM_BAGGAGE_CARTS, dataManager);
    resourcePools[ResourceType::CATERING_TRUCK] = 
        std::make_unique<ResourcePool>(ResourceType::CATERING_TRUCK, Config::NUM_CATERING_TRUCKS, dataManager);
    resourcePools[ResourceType::CLEANING_CREW] = 
        std::make_unique<ResourcePool>(ResourceType::CLEANING_CREW, Config::NUM_CLEANING_CREWS, dataManager);
    resourcePools[ResourceType::MAINTENANCE_TEAM] = 
        std::make_unique<ResourcePool>(ResourceType::MAINTENANCE_TEAM, Config::NUM_MAINTENANCE_TEAMS, dataManager);
    resourcePools[ResourceType::JETBRIDGE] = 
        std::make_unique<ResourcePool>(ResourceType::JETBRIDGE, Config::NUM_JETBRIDGES, dataManager);
    resourcePools[ResourceType::DEICING_TRUCK] = 
        std::make_unique<ResourcePool>(ResourceType::DEICING_TRUCK, Config::NUM_DEICING_TRUCKS, dataManager);
    
    buildDependencyGraph();
}

void GroundServiceManager::buildDependencyGraph() {
    dependencyGraph.clear();
    
    // Passenger Deboarding - immediate after arrival (FAST: 1-2 seconds)
    dependencyGraph.push_back({
        ServiceType::PASSENGER_DEBOARDING,
        {},  // No dependencies
        {ResourceType::JETBRIDGE},
        1, 2,  // FAST demo
        false
    });
    
    // Baggage Unloading - immediate after arrival (FAST: 1-2 seconds)
    dependencyGraph.push_back({
        ServiceType::BAGGAGE_UNLOADING,
        {},
        {ResourceType::BAGGAGE_CART},
        1, 2,  // FAST demo
        true  // Can overlap with deboarding
    });
    
    // Fire Safety Check - required before refueling (FAST: 1 second)
    dependencyGraph.push_back({
        ServiceType::FIRE_SAFETY_CHECK,
        {ServiceType::PASSENGER_DEBOARDING},
        {},  // Internal crew
        1, 1,  // FAST demo
        false
    });
    
    // Refueling - after fire safety check
    dependencyGraph.push_back({
        ServiceType::REFUELING,
        {ServiceType::FIRE_SAFETY_CHECK},
        {ResourceType::FUEL_TRUCK},
        Config::REFUEL_TIME_MIN, Config::REFUEL_TIME_MAX,
        false
    });
    
    // Cleaning - after deboarding
    dependencyGraph.push_back({
        ServiceType::CLEANING,
        {ServiceType::PASSENGER_DEBOARDING},
        {ResourceType::CLEANING_CREW},
        Config::CLEANING_TIME_MIN, Config::CLEANING_TIME_MAX,
        false
    });
    
    // Catering - after cleaning
    dependencyGraph.push_back({
        ServiceType::CATERING,
        {ServiceType::CLEANING},
        {ResourceType::CATERING_TRUCK},
        Config::CATERING_TIME_MIN, Config::CATERING_TIME_MAX,
        false
    });
    
    // Maintenance Check - can overlap with cleaning (FAST: 1-2 seconds)
    dependencyGraph.push_back({
        ServiceType::MAINTENANCE_CHECK,
        {ServiceType::PASSENGER_DEBOARDING},
        {ResourceType::MAINTENANCE_TEAM},
        1, 2,  // FAST demo
        true
    });
    
    // Baggage Loading - after catering (FAST: 1-2 seconds)
    dependencyGraph.push_back({
        ServiceType::BAGGAGE_LOADING,
        {ServiceType::CATERING, ServiceType::BAGGAGE_UNLOADING},
        {ResourceType::BAGGAGE_CART},
        1, 2,  // FAST demo
        false
    });
    
    // Passenger Boarding - after cleaning and catering
    dependencyGraph.push_back({
        ServiceType::PASSENGER_BOARDING,
        {ServiceType::CLEANING, ServiceType::CATERING},
        {ResourceType::JETBRIDGE},
        Config::BOARDING_TIME_MIN, Config::BOARDING_TIME_MAX,
        false
    });
    
    // Pushback - after all services complete (FAST: 1-2 seconds)
    dependencyGraph.push_back({
        ServiceType::PUSHBACK,
        {ServiceType::PASSENGER_BOARDING, ServiceType::BAGGAGE_LOADING, 
         ServiceType::REFUELING, ServiceType::MAINTENANCE_CHECK},
        {},  // Tug included
        1, 2,  // FAST demo
        false
    });
    
    // De-icing (optional, winter only)
    dependencyGraph.push_back({
        ServiceType::DEICING,
        {ServiceType::PUSHBACK},
        {ResourceType::DEICING_TRUCK},
        Config::DEICING_TIME_MIN, Config::DEICING_TIME_MAX,
        false
    });
}

void GroundServiceManager::startTurnaround(int flightId, AircraftType aircraftType,
                                            FlightType flightType, bool requiresDeicing) {
    std::vector<ServiceTask> tasks = createServiceTasks(flightId, aircraftType, 
                                                         flightType, requiresDeicing);
    
    pthread_mutex_lock(&turnaroundMutex);
    activeTurnarounds[flightId] = tasks;
    pthread_mutex_unlock(&turnaroundMutex);
    
    // Submit first services to scheduler
    for (auto& task : tasks) {
        if (task.dependencies.empty()) {
            Operation op;
            op.type = OperationType::GROUND_SERVICE;
            op.priority = 50;
            op.flightId = flightId;
            op.resourceId = static_cast<int>(task.type);
            scheduler->submitOperation(op);
        }
    }
}

std::vector<ServiceTask> GroundServiceManager::createServiceTasks(int flightId, 
                                                                   AircraftType type,
                                                                   FlightType flightType,
                                                                   bool requiresDeicing) {
    std::vector<ServiceTask> tasks;
    int taskId = 0;
    
    // Get aircraft multiplier
    double aircraftMultiplier = 1.0;
    auto specIt = Config::AIRCRAFT_SPECS.find(type);
    if (specIt != Config::AIRCRAFT_SPECS.end()) {
        aircraftMultiplier = specIt->second.serviceTimeMultiplier;
    }
    
    // International multiplier
    double intlMultiplier = (flightType == FlightType::INTERNATIONAL) ? 
                             Config::INTERNATIONAL_TIME_MULTIPLIER : 1.0;
    
    // Weather multiplier
    WeatherCondition weather = dataManager->getCurrentWeather();
    double weatherMultiplier = weather.operationMultiplier;
    
    // Crew skill (random)
    std::uniform_real_distribution<double> skillDist(Config::CREW_SKILL_MIN, Config::CREW_SKILL_MAX);
    double crewSkill = skillDist(rng);
    
    for (const auto& dep : dependencyGraph) {
        // Skip de-icing if not required
        if (dep.service == ServiceType::DEICING && !requiresDeicing) continue;
        
        ServiceTask task;
        task.id = taskId++;
        task.flightId = flightId;
        task.type = dep.service;
        task.dependencies = dep.dependencies;
        task.requiredResources = getRequiredResources(dep.service, flightType);
        task.completed = false;
        task.inProgress = false;
        task.progressPercent = 0;
        
        // Calculate duration with all factors
        TimeUnit baseTime = calculateServiceDuration(dep.service, type, flightType,
                                                      crewSkill, weatherMultiplier);
        task.estimatedDuration = baseTime;
        
        tasks.push_back(task);
    }
    
    return tasks;
}

TimeUnit GroundServiceManager::calculateServiceDuration(ServiceType type, 
                                                         AircraftType aircraftType,
                                                         FlightType flightType,
                                                         double crewSkill,
                                                         double weatherMultiplier) {
    // Find base times from dependency graph
    int baseMin = 300, baseMax = 600;
    for (const auto& dep : dependencyGraph) {
        if (dep.service == type) {
            baseMin = dep.baseTimeMin;
            baseMax = dep.baseTimeMax;
            break;
        }
    }
    
    // Random base time
    std::uniform_int_distribution<int> timeDist(baseMin, baseMax);
    int baseTime = timeDist(rng);
    
    // Apply multipliers
    double aircraftMult = 1.0;
    auto specIt = Config::AIRCRAFT_SPECS.find(aircraftType);
    if (specIt != Config::AIRCRAFT_SPECS.end()) {
        aircraftMult = specIt->second.serviceTimeMultiplier;
    }
    
    double intlMult = (flightType == FlightType::INTERNATIONAL) ? 
                       Config::INTERNATIONAL_TIME_MULTIPLIER : 1.0;
    
    // Crew skill affects duration inversely
    double skillMult = 2.0 - crewSkill; // skill 0.8 -> 1.2x time, skill 1.2 -> 0.8x time
    
    return static_cast<TimeUnit>(baseTime * aircraftMult * intlMult * skillMult * weatherMultiplier);
}

void GroundServiceManager::updateServices(TimeUnit simulationTime) {
    pthread_mutex_lock(&turnaroundMutex);
    
    // Use simulation time instead of wall-clock time
    TimeUnit currentTime = simulationTime;
    
    for (auto& pair : activeTurnarounds) {
        int flightId = pair.first;
        auto& tasks = pair.second;
        
        for (auto& task : tasks) {
            if (task.completed) continue;
            
            if (task.inProgress) {
                // Update progress using simulation time
                TimeUnit elapsed = currentTime - task.startTime;
                task.progressPercent = std::min(100.0, 
                    (static_cast<double>(elapsed) / task.estimatedDuration) * 100);
                
                if (elapsed >= task.estimatedDuration) {
                    task.completed = true;
                    task.inProgress = false;
                    task.endTime = currentTime;
                    task.actualDuration = elapsed;
                    task.progressPercent = 100;
                    
                    // Release resources
                    for (auto resType : task.requiredResources) {
                        // Would track which specific resource was assigned
                    }
                }
            } else {
                // Check if can start
                if (areDependenciesMet(flightId, task.type)) {
                    // Try to acquire resources
                    bool gotResources = true;
                    for (auto resType : task.requiredResources) {
                        if (requestResource(resType, flightId) == -1) {
                            gotResources = false;
                            break;
                        }
                    }
                    
                    if (gotResources) {
                        task.inProgress = true;
                        task.startTime = currentTime;  // Use simulation time
                    }
                }
            }
        }
    }
    
    pthread_mutex_unlock(&turnaroundMutex);
}

bool GroundServiceManager::isTurnaroundComplete(int flightId) {
    pthread_mutex_lock(&turnaroundMutex);
    
    auto it = activeTurnarounds.find(flightId);
    if (it == activeTurnarounds.end()) {
        pthread_mutex_unlock(&turnaroundMutex);
        return true;
    }
    
    bool complete = true;
    for (const auto& task : it->second) {
        if (!task.completed) {
            complete = false;
            break;
        }
    }
    
    pthread_mutex_unlock(&turnaroundMutex);
    return complete;
}

double GroundServiceManager::getTurnaroundProgress(int flightId) {
    pthread_mutex_lock(&turnaroundMutex);
    
    auto it = activeTurnarounds.find(flightId);
    if (it == activeTurnarounds.end()) {
        pthread_mutex_unlock(&turnaroundMutex);
        return 100.0;
    }
    
    double totalProgress = 0;
    for (const auto& task : it->second) {
        totalProgress += task.progressPercent;
    }
    
    double avgProgress = totalProgress / it->second.size();
    
    pthread_mutex_unlock(&turnaroundMutex);
    return avgProgress;
}

std::vector<ServiceTask> GroundServiceManager::getActiveServices(int flightId) {
    pthread_mutex_lock(&turnaroundMutex);
    
    std::vector<ServiceTask> active;
    auto it = activeTurnarounds.find(flightId);
    if (it != activeTurnarounds.end()) {
        for (const auto& task : it->second) {
            if (task.inProgress) {
                active.push_back(task);
            }
        }
    }
    
    pthread_mutex_unlock(&turnaroundMutex);
    return active;
}

int GroundServiceManager::requestResource(ResourceType type, int flightId) {
    auto it = resourcePools.find(type);
    if (it != resourcePools.end()) {
        return it->second->acquire(flightId);
    }
    return -1;
}

void GroundServiceManager::releaseResource(ResourceType type, int resourceId) {
    auto it = resourcePools.find(type);
    if (it != resourcePools.end()) {
        it->second->release(resourceId);
    }
}

int GroundServiceManager::getAvailableResources(ResourceType type) {
    auto it = resourcePools.find(type);
    if (it != resourcePools.end()) {
        return it->second->getAvailableCount();
    }
    return 0;
}

void GroundServiceManager::simulateRandomFailures() {
    std::uniform_real_distribution<double> failureDist(0, 1);
    std::uniform_int_distribution<int> durationDist(300, 1800); // 5-30 min
    
    for (auto& pair : resourcePools) {
        if (failureDist(rng) < Config::EQUIPMENT_FAILURE_PROBABILITY) {
            // Random resource of this type fails
            std::uniform_int_distribution<int> resDist(0, pair.second->getAvailableCount());
            int resId = resDist(rng);
            TimeUnit duration = durationDist(rng);
            pair.second->simulateFailure(resId, duration);
            
            dataManager->addEmergencyAlert(
                "Equipment failure: Resource type " + std::to_string(static_cast<int>(pair.first)),
                EmergencyType::MECHANICAL_FAILURE);
        }
    }
}

bool GroundServiceManager::areDependenciesMet(int flightId, ServiceType service) {
    // Find this service in dependency graph
    const ServiceDependency* dep = nullptr;
    for (const auto& d : dependencyGraph) {
        if (d.service == service) {
            dep = &d;
            break;
        }
    }
    
    if (!dep) return true;
    if (dep->dependencies.empty()) return true;
    
    // Check if all dependencies are complete
    auto it = activeTurnarounds.find(flightId);
    if (it == activeTurnarounds.end()) return false;
    
    for (ServiceType requiredService : dep->dependencies) {
        bool found = false;
        for (const auto& task : it->second) {
            if (task.type == requiredService) {
                if (!task.completed) return false;
                found = true;
                break;
            }
        }
        // If service not found (e.g., de-icing skipped), that's OK
    }
    
    return true;
}

ServiceType GroundServiceManager::getNextService(int flightId) {
    auto it = activeTurnarounds.find(flightId);
    if (it == activeTurnarounds.end()) {
        return ServiceType::PUSHBACK; // Shouldn't happen
    }
    
    for (const auto& task : it->second) {
        if (!task.completed && !task.inProgress) {
            if (areDependenciesMet(flightId, task.type)) {
                return task.type;
            }
        }
    }
    
    return ServiceType::PUSHBACK;
}

std::vector<ResourceType> GroundServiceManager::getRequiredResources(ServiceType type, 
                                                                      FlightType flightType) {
    for (const auto& dep : dependencyGraph) {
        if (dep.service == type) {
            std::vector<ResourceType> resources = dep.requiredResources;
            
            // International flights need 2 catering trucks
            if (type == ServiceType::CATERING && flightType == FlightType::INTERNATIONAL) {
                if (!resources.empty() && resources[0] == ResourceType::CATERING_TRUCK) {
                    resources.push_back(ResourceType::CATERING_TRUCK);
                }
            }
            
            return resources;
        }
    }
    
    return {};
}
