/**
 * Smart Airport Operations Management System
 * crisis_manager.cpp - Crisis and Event Management Implementation
 */

#include "crisis_manager.h"
#include "aircraft.h"
#include <algorithm>
#include <chrono>

// ============================================================================
// Constructor/Destructor
// ============================================================================

CrisisManager::CrisisManager(DataManager* dm, Scheduler* sched)
    : dataManager(dm), scheduler(sched), rng(std::random_device{}()) {
    pthread_mutex_init(&crisisMutex, nullptr);
    pthread_mutex_init(&weatherMutex, nullptr);
}

CrisisManager::~CrisisManager() {
    pthread_mutex_destroy(&crisisMutex);
    pthread_mutex_destroy(&weatherMutex);
}

void CrisisManager::initialize() {
    currentWeather.type = WeatherType::CLEAR;
    currentWeather.windSpeed = 10;
    currentWeather.windDirection = 270;
    currentWeather.visibility = 10000;
    currentWeather.requiresDeicing = false;
    currentWeather.operationMultiplier = 1.0;
    
    currentThreatLevel = ThreatLevel::NORMAL;
}

// ============================================================================
// Weather Events
// ============================================================================

void CrisisManager::triggerWeatherEvent(WeatherType type, TimeUnit duration) {
    pthread_mutex_lock(&weatherMutex);
    
    TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    WeatherEvent event;
    event.type = type;
    event.startTime = currentTime;
    event.duration = duration;
    
    switch (type) {
        case WeatherType::THUNDERSTORM:
            event.operationalImpact = 0.0; // Runway closed
            event.description = "Thunderstorm - Runways closed";
            handleThunderstorm(duration);
            break;
            
        case WeatherType::FOG:
            event.operationalImpact = 0.5; // 50% capacity
            event.description = "Dense fog - Reduced operations";
            handleFog(duration);
            break;
            
        case WeatherType::SNOW:
        case WeatherType::ICE:
            event.operationalImpact = 0.6;
            event.description = "Winter conditions - De-icing required";
            handleSnowIce(duration);
            break;
            
        case WeatherType::HIGH_WIND:
            event.operationalImpact = 0.7;
            event.description = "High winds - Some runway closures";
            handleHighWind(45);
            break;
            
        case WeatherType::TORNADO_WARNING:
            event.operationalImpact = 0.0;
            event.description = "TORNADO WARNING - Ground stop";
            handleTornadoWarning();
            break;
            
        default:
            event.operationalImpact = 0.9;
            event.description = "Weather event";
    }
    
    activeWeatherEvents.push_back(event);
    
    // Update current weather
    currentWeather.type = type;
    currentWeather.operationMultiplier = event.operationalImpact;
    currentWeather.requiresDeicing = (type == WeatherType::SNOW || type == WeatherType::ICE);
    
    dataManager->updateWeather(currentWeather);
    dataManager->addEmergencyAlert(event.description, EmergencyType::NONE);
    
    pthread_mutex_unlock(&weatherMutex);
}

double CrisisManager::getCurrentWeatherImpact() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&weatherMutex));
    double impact = currentWeather.operationMultiplier;
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&weatherMutex));
    return impact;
}

bool CrisisManager::isDeicingRequired() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&weatherMutex));
    bool required = currentWeather.requiresDeicing;
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&weatherMutex));
    return required;
}

void CrisisManager::handleThunderstorm(TimeUnit duration) {
    // Close all runways
    if (runwayManager) {
        for (int i = 0; i < Config::NUM_RUNWAYS; i++) {
            runwayManager->closeRunwayForWeather(i, duration);
        }
    }
}

void CrisisManager::handleFog(TimeUnit duration) {
    // Reduce landing rate - handled by operational impact multiplier
    // Could also increase separation requirements
}

void CrisisManager::handleSnowIce(TimeUnit duration) {
    // Enable de-icing requirement
    pthread_mutex_lock(&weatherMutex);
    currentWeather.requiresDeicing = true;
    pthread_mutex_unlock(&weatherMutex);
}

void CrisisManager::handleHighWind(int windSpeed) {
    if (runwayManager) {
        runwayManager->handleWindChange(currentWeather.windDirection, windSpeed);
    }
}

void CrisisManager::handleTornadoWarning() {
    // Complete ground stop
    if (runwayManager) {
        for (int i = 0; i < Config::NUM_RUNWAYS; i++) {
            runwayManager->closeRunwayForWeather(i, 3600); // 1 hour minimum
        }
    }
    
    dataManager->addEmergencyAlert("GROUND STOP - Tornado Warning", EmergencyType::NONE);
}

// ============================================================================
// Operational Emergencies
// ============================================================================

int CrisisManager::declareEmergency(int flightId, EmergencyType type, 
                                     const std::string& description) {
    pthread_mutex_lock(&crisisMutex);
    
    TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    ActiveCrisis crisis;
    crisis.id = nextCrisisId++;
    crisis.type = type;
    crisis.description = description;
    crisis.startTime = currentTime;
    crisis.expectedEndTime = currentTime + 3600; // 1 hour default
    crisis.affectedFlightId = flightId;
    crisis.resolved = false;
    crisis.priority = 0; // Highest priority
    
    activeCrises[crisis.id] = crisis;
    
    pthread_mutex_unlock(&crisisMutex);
    
    // Handle specific emergency types
    switch (type) {
        case EmergencyType::MECHANICAL_FAILURE:
            handleMechanicalFailure(flightId);
            break;
        case EmergencyType::MEDICAL_EMERGENCY:
            handleMedicalEmergency(flightId);
            break;
        case EmergencyType::SECURITY_INCIDENT:
        case EmergencyType::SUSPICIOUS_PACKAGE:
        case EmergencyType::UNRULY_PASSENGER:
            handleSecurityIncident(flightId);
            break;
        case EmergencyType::FUEL_SHORTAGE:
            handleFuelShortage();
            break;
        default:
            break;
    }
    
    dataManager->addEmergencyAlert(description, type);
    
    return crisis.id;
}

void CrisisManager::resolveEmergency(int crisisId) {
    pthread_mutex_lock(&crisisMutex);
    
    auto it = activeCrises.find(crisisId);
    if (it != activeCrises.end()) {
        it->second.resolved = true;
        totalCrisesHandled++;
        
        // Clear from data manager
        dataManager->clearEmergency(it->second.description);
    }
    
    pthread_mutex_unlock(&crisisMutex);
}

std::vector<ActiveCrisis> CrisisManager::getActiveEmergencies() const {
    std::vector<ActiveCrisis> emergencies;
    
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&crisisMutex));
    for (const auto& pair : activeCrises) {
        if (!pair.second.resolved) {
            emergencies.push_back(pair.second);
        }
    }
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&crisisMutex));
    
    return emergencies;
}

void CrisisManager::handleMechanicalFailure(int flightId) {
    // Priority landing
    priorityLanding(flightId);
}

void CrisisManager::handleMedicalEmergency(int flightId) {
    // Priority landing
    priorityLanding(flightId);
}

void CrisisManager::handleSecurityIncident(int flightId) {
    // Increase threat level temporarily
    if (currentThreatLevel < ThreatLevel::HIGH) {
        setThreatLevel(ThreatLevel::HIGH);
    }
}

void CrisisManager::handleFuelShortage() {
    // Prioritize long-haul flights for remaining fuel
    dataManager->addEmergencyAlert("Fuel shortage - Rationing in effect", EmergencyType::FUEL_SHORTAGE);
}

void CrisisManager::priorityLanding(int flightId) {
    // Submit emergency landing operation to scheduler
    Operation op;
    op.type = OperationType::EMERGENCY_RESPONSE;
    op.priority = 0; // Highest
    op.flightId = flightId;
    
    scheduler->submitOperation(op);
}

// ============================================================================
// Mass Disruptions
// ============================================================================

void CrisisManager::triggerMassCancellation(int numFlights, const std::string& reason) {
    dataManager->addEmergencyAlert(
        "MASS CANCELLATION: " + std::to_string(numFlights) + " flights - " + reason,
        EmergencyType::NONE);
    
    // Would coordinate with flight manager to cancel flights
    // and passenger processor for rebooking
}

void CrisisManager::handleCascadingDelays(int originalFlightId, int delayMinutes) {
    // Calculate affected flights based on:
    // - Connecting passengers
    // - Crew rotations
    // - Aircraft rotations
    
    // Submit high-priority operation to assess impact
    Operation op;
    op.type = OperationType::EMERGENCY_RESPONSE;
    op.priority = 20; // High but not emergency
    op.flightId = originalFlightId;
    
    scheduler->submitOperation(op);
}

// ============================================================================
// Threat Level Management
// ============================================================================

void CrisisManager::setThreatLevel(ThreatLevel level) {
    currentThreatLevel = level;
    
    std::string levelStr;
    switch (level) {
        case ThreatLevel::NORMAL: levelStr = "NORMAL"; break;
        case ThreatLevel::ELEVATED: levelStr = "ELEVATED"; break;
        case ThreatLevel::HIGH: levelStr = "HIGH"; break;
        case ThreatLevel::SEVERE: levelStr = "SEVERE"; break;
        case ThreatLevel::CRITICAL: levelStr = "CRITICAL"; break;
    }
    
    dataManager->addEmergencyAlert("Threat level changed to: " + levelStr, EmergencyType::NONE);
    
    if (level == ThreatLevel::CRITICAL) {
        // Complete shutdown
        if (runwayManager) {
            for (int i = 0; i < Config::NUM_RUNWAYS; i++) {
                runwayManager->closeRunwayForWeather(i, 86400); // Close indefinitely
            }
        }
    }
}

ThreatLevel CrisisManager::getThreatLevel() const {
    return currentThreatLevel;
}

double CrisisManager::getScreeningMultiplier() const {
    switch (currentThreatLevel) {
        case ThreatLevel::NORMAL: return 1.0;
        case ThreatLevel::ELEVATED: return 1.3; // 30% slower
        case ThreatLevel::HIGH: return 1.5;     // 50% slower
        case ThreatLevel::SEVERE: return 2.0;   // 100% slower
        case ThreatLevel::CRITICAL: return 999.0; // Effectively stopped
    }
    return 1.0;
}

// ============================================================================
// Random Event Simulation
// ============================================================================

void CrisisManager::simulateRandomEvents() {
    std::uniform_real_distribution<double> eventDist(0, 1);
    
    // Mechanical failure (2% per flight per check)
    if (eventDist(rng) < 0.001) { // Lower for simulation
        // Pick random flight
        std::uniform_int_distribution<int> flightDist(1, 100);
        int flightId = flightDist(rng);
        declareEmergency(flightId, EmergencyType::MECHANICAL_FAILURE,
                        "Mechanical failure on flight " + std::to_string(flightId));
    }
    
    // Medical emergency (1%)
    if (eventDist(rng) < 0.0005) {
        std::uniform_int_distribution<int> flightDist(1, 100);
        int flightId = flightDist(rng);
        declareEmergency(flightId, EmergencyType::MEDICAL_EMERGENCY,
                        "Medical emergency on flight " + std::to_string(flightId));
    }
    
    // Weather events (occasional)
    if (eventDist(rng) < 0.0001) {
        std::uniform_int_distribution<int> weatherDist(0, 4);
        WeatherType type = static_cast<WeatherType>(weatherDist(rng) + 3); // Skip clear/light
        std::uniform_int_distribution<int> durationDist(1800, 7200); // 30 min - 2 hours
        triggerWeatherEvent(type, durationDist(rng));
    }
}

// ============================================================================
// Update
// ============================================================================

void CrisisManager::update(TimeUnit currentTime) {
    // Check for expired weather events
    pthread_mutex_lock(&weatherMutex);
    
    auto it = activeWeatherEvents.begin();
    while (it != activeWeatherEvents.end()) {
        if (currentTime >= it->startTime + it->duration) {
            // Weather event ended
            it = activeWeatherEvents.erase(it);
        } else {
            ++it;
        }
    }
    
    // If no active weather events, restore clear weather
    if (activeWeatherEvents.empty()) {
        currentWeather.type = WeatherType::CLEAR;
        currentWeather.operationMultiplier = 1.0;
        currentWeather.requiresDeicing = false;
        dataManager->updateWeather(currentWeather);
        
        // Reopen runways
        if (runwayManager) {
            for (int i = 0; i < Config::NUM_RUNWAYS; i++) {
                runwayManager->reopenRunway(i);
            }
        }
    }
    
    pthread_mutex_unlock(&weatherMutex);
    
    // Check for auto-resolved crises
    pthread_mutex_lock(&crisisMutex);
    
    for (auto& pair : activeCrises) {
        if (!pair.second.resolved && currentTime >= pair.second.expectedEndTime) {
            // Auto-resolve after expected duration
            pair.second.resolved = true;
            totalCrisesHandled++;
        }
    }
    
    // Clean up old resolved crises
    for (auto it = activeCrises.begin(); it != activeCrises.end(); ) {
        if (it->second.resolved && 
            currentTime > it->second.expectedEndTime + 3600) {
            it = activeCrises.erase(it);
        } else {
            ++it;
        }
    }
    
    pthread_mutex_unlock(&crisisMutex);
}

int CrisisManager::getActiveCrisisCount() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&crisisMutex));
    int count = 0;
    for (const auto& pair : activeCrises) {
        if (!pair.second.resolved) count++;
    }
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&crisisMutex));
    return count;
}

int CrisisManager::getTotalCrisesHandled() const {
    return totalCrisesHandled;
}
