/**
 * Smart Airport Operations Management System
 * crew.h - Crew Management with Fatigue Simulation
 * 
 * Implements crew management for all 5 crew types with:
 * - Duty hour limits and rest requirements
 * - Skill qualifications
 * - Fatigue simulation
 * - LRU-based crew replacement
 */

#ifndef CREW_H
#define CREW_H

#include "types.h"
#include "config.h"
#include "data_manager.h"
#include <vector>
#include <map>
#include <queue>

/**
 * Crew Pool
 * Manages crew members of a specific type
 */
class CrewPool {
public:
    CrewPool(CrewType type, int count, DataManager* dm);
    
    // Assign crew to duty
    int assignCrew(int flightId, AircraftType requiredCert = AircraftType::A320);
    
    // Release crew from duty
    void releaseCrew(int crewId);
    
    // Start break for crew member
    void startBreak(int crewId);
    
    // End break
    void endBreak(int crewId);
    
    // Get available crew
    std::vector<int> getAvailableCrew() const;
    
    // Get standby crew
    std::vector<int> getStandbyCrew() const;
    
    // Update fatigue for all crew
    void updateFatigue(TimeUnit elapsed);
    
    // Check if crew is fatigued
    bool isFatigued(int crewId) const;
    
    // Get crew member
    CrewMember* getCrew(int crewId);
    
private:
    CrewType type;
    DataManager* dataManager;
    std::map<int, CrewMember> crewMembers;
    std::deque<int> lruOrder;  // For LRU-based replacement
    mutable pthread_mutex_t mutex;
    
    // Add certification
    void addCertification(int crewId, AircraftType aircraft);
    
    // Update LRU
    void touchLRU(int crewId);
};

/**
 * Crew Manager
 * Central crew coordination
 */
class CrewManager {
public:
    CrewManager(DataManager* dm);
    ~CrewManager();
    
    void initialize();
    
    // Assign crew for flight
    std::vector<int> assignFlightCrew(int flightId, AircraftType aircraftType,
                                       bool isInternational);
    
    // Release flight crew
    void releaseFlightCrew(int flightId);
    
    // Update all crew (call periodically)
    void update(TimeUnit elapsedSeconds);
    
    // Handle crew shortage
    bool handleCrewShortage(CrewType type, int flightId);
    
    // Get crew statistics
    int getAvailableCount(CrewType type) const;
    int getOnDutyCount(CrewType type) const;
    int getFatiguedCount(CrewType type) const;
    
    // Get crew for replacement
    int getReplacementCrew(CrewType type, AircraftType certRequired);
    
private:
    DataManager* dataManager;
    
    // Crew pools by type
    std::map<CrewType, std::unique_ptr<CrewPool>> crewPools;
    
    // Flight assignments
    std::map<int, std::vector<int>> flightCrewAssignments;
    
    pthread_mutex_t managerMutex;
    
    // Get required crew for aircraft
    int getRequiredCabinCrew(AircraftType type);
    int getRequiredCockpitCrew(AircraftType type);
};

#endif // CREW_H
