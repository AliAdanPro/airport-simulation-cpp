/**
 * Smart Airport Operations Management System
 * crisis_manager.h - Crisis and Event Management
 * 
 * Implements dynamic crisis management:
 * - Weather events (5+ types)
 * - Operational emergencies
 * - Mass disruption handling
 * - Threat level management
 */

#ifndef CRISIS_MANAGER_H
#define CRISIS_MANAGER_H

#include "types.h"
#include "config.h"
#include "data_manager.h"
#include "scheduler.h"
#include <vector>
#include <map>
#include <random>

// Forward declarations
class RunwayManager;
class GateManager;
class FlightManager;

/**
 * Active Crisis
 */
struct ActiveCrisis {
    int id;
    EmergencyType type;
    std::string description;
    TimeUnit startTime;
    TimeUnit expectedEndTime;
    int affectedFlightId;       // -1 if global
    bool resolved;
    int priority;               // 0 = highest
};

/**
 * Weather Event
 */
struct WeatherEvent {
    WeatherType type;
    TimeUnit startTime;
    TimeUnit duration;
    double operationalImpact;   // Slowdown multiplier
    std::string description;
};

/**
 * Crisis Manager
 * Coordinates crisis response across all airport systems
 */
class CrisisManager {
public:
    CrisisManager(DataManager* dm, Scheduler* sched);
    ~CrisisManager();
    
    void setRunwayManager(RunwayManager* rm) { runwayManager = rm; }
    void setGateManager(GateManager* gm) { gateManager = gm; }
    void setFlightManager(FlightManager* fm) { flightManager = fm; }
    
    void initialize();
    
    // ========================================================================
    // Weather Events
    // ========================================================================
    
    // Trigger weather event
    void triggerWeatherEvent(WeatherType type, TimeUnit duration);
    
    // Get current weather impact
    double getCurrentWeatherImpact() const;
    
    // Check if de-icing required
    bool isDeicingRequired() const;
    
    // ========================================================================
    // Operational Emergencies
    // ========================================================================
    
    // Declare emergency for flight
    int declareEmergency(int flightId, EmergencyType type, const std::string& description);
    
    // Resolve emergency
    void resolveEmergency(int crisisId);
    
    // Get active emergencies
    std::vector<ActiveCrisis> getActiveEmergencies() const;
    
    // ========================================================================
    // Mass Disruptions
    // ========================================================================
    
    // Trigger mass cancellation (airline system failure)
    void triggerMassCancellation(int numFlights, const std::string& reason);
    
    // Handle cascading delays
    void handleCascadingDelays(int originalFlightId, int delayMinutes);
    
    // ========================================================================
    // Threat Level Management
    // ========================================================================
    
    // Set threat level (1-5)
    void setThreatLevel(ThreatLevel level);
    
    // Get current threat level
    ThreatLevel getThreatLevel() const;
    
    // Get screening time multiplier
    double getScreeningMultiplier() const;
    
    // ========================================================================
    // Random Event Simulation
    // ========================================================================
    
    // Simulate random events (call periodically)
    void simulateRandomEvents();
    
    // ========================================================================
    // Update and Status
    // ========================================================================
    
    // Update crisis states (call each tick)
    void update(TimeUnit currentTime);
    
    // Get crisis statistics
    int getActiveCrisisCount() const;
    int getTotalCrisesHandled() const;
    
private:
    DataManager* dataManager;
    Scheduler* scheduler;
    RunwayManager* runwayManager = nullptr;
    GateManager* gateManager = nullptr;
    FlightManager* flightManager = nullptr;
    
    // Active crises
    std::map<int, ActiveCrisis> activeCrises;
    int nextCrisisId = 1;
    pthread_mutex_t crisisMutex;
    
    // Weather state
    WeatherCondition currentWeather;
    std::vector<WeatherEvent> activeWeatherEvents;
    pthread_mutex_t weatherMutex;
    
    // Threat level
    ThreatLevel currentThreatLevel = ThreatLevel::NORMAL;
    
    // Statistics
    int totalCrisesHandled = 0;
    
    // Random generator
    std::mt19937 rng;
    
    // Handle specific weather types
    void handleThunderstorm(TimeUnit duration);
    void handleFog(TimeUnit duration);
    void handleSnowIce(TimeUnit duration);
    void handleHighWind(int windSpeed);
    void handleTornadoWarning();
    
    // Handle specific emergencies
    void handleMechanicalFailure(int flightId);
    void handleMedicalEmergency(int flightId);
    void handleSecurityIncident(int flightId);
    void handleFuelShortage();
    
    // Priority landing for emergencies
    void priorityLanding(int flightId);
};

#endif // CRISIS_MANAGER_H
