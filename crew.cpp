/**
 * Smart Airport Operations Management System
 * crew.cpp - Crew Management Implementation
 */

#include "crew.h"
#include <algorithm>
#include <chrono>
#include <random>

// ============================================================================
// CrewPool Implementation
// ============================================================================

CrewPool::CrewPool(CrewType type, int count, DataManager* dm)
    : type(type), dataManager(dm) {
    pthread_mutex_init(&mutex, nullptr);
    
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> skillDist(Config::CREW_SKILL_MIN, Config::CREW_SKILL_MAX);
    
    for (int i = 0; i < count; i++) {
        CrewMember member;
        member.id = i;
        member.name = "Crew_" + std::to_string(static_cast<int>(type)) + "_" + std::to_string(i);
        member.type = type;
        member.status = CrewStatus::AVAILABLE;
        member.fatigueLevel = 0.0;
        member.skillLevel = skillDist(rng);
        member.dutyStartTime = 0;
        member.totalDutyHours = 0;
        member.lastBreakTime = 0;
        member.currentAssignment = -1;
        
        // Add certifications for pilots
        if (type == CrewType::PILOT_CAPTAIN || type == CrewType::PILOT_FIRST_OFFICER) {
            member.certifications.push_back(AircraftType::A320);
            member.certifications.push_back(AircraftType::B737);
            if (i < count / 2) {
                member.certifications.push_back(AircraftType::B777);
                member.certifications.push_back(AircraftType::A380);
            }
        }
        
        crewMembers[i] = member;
        lruOrder.push_back(i);
    }
}

int CrewPool::assignCrew(int flightId, AircraftType requiredCert) {
    pthread_mutex_lock(&mutex);
    
    TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Find available crew using LRU (least recently used for fairness)
    for (auto it = lruOrder.begin(); it != lruOrder.end(); ++it) {
        int crewId = *it;
        CrewMember& member = crewMembers[crewId];
        
        if (member.status != CrewStatus::AVAILABLE && 
            member.status != CrewStatus::STANDBY) continue;
        
        // Check fatigue
        if (member.fatigueLevel >= Config::FATIGUE_CRITICAL_LEVEL) continue;
        
        // Check certification if needed
        if (type == CrewType::PILOT_CAPTAIN || type == CrewType::PILOT_FIRST_OFFICER) {
            bool hasCert = false;
            for (AircraftType cert : member.certifications) {
                if (cert == requiredCert) {
                    hasCert = true;
                    break;
                }
            }
            if (!hasCert) continue;
        }
        
        // Assign
        member.status = CrewStatus::ON_DUTY;
        member.currentAssignment = flightId;
        member.dutyStartTime = currentTime;
        
        // Move to back of LRU
        lruOrder.erase(it);
        lruOrder.push_back(crewId);
        
        dataManager->assignCrew(crewId, flightId);
        
        pthread_mutex_unlock(&mutex);
        return crewId;
    }
    
    pthread_mutex_unlock(&mutex);
    return -1;
}

void CrewPool::releaseCrew(int crewId) {
    pthread_mutex_lock(&mutex);
    
    auto it = crewMembers.find(crewId);
    if (it != crewMembers.end()) {
        CrewMember& member = it->second;
        
        TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // Update duty hours
        if (member.dutyStartTime > 0) {
            member.totalDutyHours += (currentTime - member.dutyStartTime);
        }
        
        member.status = CrewStatus::AVAILABLE;
        member.currentAssignment = -1;
        
        dataManager->unassignCrew(crewId);
    }
    
    pthread_mutex_unlock(&mutex);
}

void CrewPool::startBreak(int crewId) {
    pthread_mutex_lock(&mutex);
    
    auto it = crewMembers.find(crewId);
    if (it != crewMembers.end()) {
        it->second.status = CrewStatus::ON_BREAK;
        it->second.lastBreakTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    
    pthread_mutex_unlock(&mutex);
}

void CrewPool::endBreak(int crewId) {
    pthread_mutex_lock(&mutex);
    
    auto it = crewMembers.find(crewId);
    if (it != crewMembers.end()) {
        it->second.status = CrewStatus::AVAILABLE;
        
        // Reduce fatigue during break
        TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        TimeUnit breakDuration = currentTime - it->second.lastBreakTime;
        double hoursRested = breakDuration / 3600.0;
        
        it->second.fatigueLevel = std::max(0.0, 
            it->second.fatigueLevel - hoursRested * Config::FATIGUE_DECREMENT_PER_REST_HOUR);
    }
    
    pthread_mutex_unlock(&mutex);
}

std::vector<int> CrewPool::getAvailableCrew() const {
    std::vector<int> available;
    
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mutex));
    for (const auto& pair : crewMembers) {
        if (pair.second.status == CrewStatus::AVAILABLE) {
            available.push_back(pair.first);
        }
    }
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mutex));
    
    return available;
}

std::vector<int> CrewPool::getStandbyCrew() const {
    std::vector<int> standby;
    
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mutex));
    for (const auto& pair : crewMembers) {
        if (pair.second.status == CrewStatus::STANDBY) {
            standby.push_back(pair.first);
        }
    }
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mutex));
    
    return standby;
}

void CrewPool::updateFatigue(TimeUnit elapsed) {
    pthread_mutex_lock(&mutex);
    
    double hoursElapsed = elapsed / 3600.0;
    
    for (auto& pair : crewMembers) {
        CrewMember& member = pair.second;
        
        if (member.status == CrewStatus::ON_DUTY) {
            // Increase fatigue while on duty
            member.fatigueLevel = std::min(1.0,
                member.fatigueLevel + hoursElapsed * Config::FATIGUE_INCREMENT_PER_HOUR);
            
            // Check for critical fatigue
            if (member.fatigueLevel >= Config::FATIGUE_CRITICAL_LEVEL) {
                member.status = CrewStatus::FATIGUED;
                // Would trigger replacement
            } else if (member.fatigueLevel >= Config::FATIGUE_WARNING_LEVEL) {
                // Warning level - might need break soon
            }
        } else if (member.status == CrewStatus::ON_BREAK || 
                   member.status == CrewStatus::OFF_DUTY) {
            // Decrease fatigue while resting
            member.fatigueLevel = std::max(0.0,
                member.fatigueLevel - hoursElapsed * Config::FATIGUE_DECREMENT_PER_REST_HOUR);
        }
    }
    
    pthread_mutex_unlock(&mutex);
}

bool CrewPool::isFatigued(int crewId) const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mutex));
    
    bool fatigued = false;
    auto it = crewMembers.find(crewId);
    if (it != crewMembers.end()) {
        fatigued = it->second.fatigueLevel >= Config::FATIGUE_WARNING_LEVEL;
    }
    
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mutex));
    return fatigued;
}

CrewMember* CrewPool::getCrew(int crewId) {
    pthread_mutex_lock(&mutex);
    CrewMember* member = nullptr;
    auto it = crewMembers.find(crewId);
    if (it != crewMembers.end()) {
        member = &it->second;
    }
    pthread_mutex_unlock(&mutex);
    return member;
}

void CrewPool::touchLRU(int crewId) {
    auto it = std::find(lruOrder.begin(), lruOrder.end(), crewId);
    if (it != lruOrder.end()) {
        lruOrder.erase(it);
        lruOrder.push_back(crewId);
    }
}

// ============================================================================
// CrewManager Implementation
// ============================================================================

CrewManager::CrewManager(DataManager* dm) : dataManager(dm) {
    pthread_mutex_init(&managerMutex, nullptr);
}

CrewManager::~CrewManager() {
    pthread_mutex_destroy(&managerMutex);
}

void CrewManager::initialize() {
    // Create crew pools
    crewPools[CrewType::PILOT_CAPTAIN] = 
        std::make_unique<CrewPool>(CrewType::PILOT_CAPTAIN, 20, dataManager);
    crewPools[CrewType::PILOT_FIRST_OFFICER] = 
        std::make_unique<CrewPool>(CrewType::PILOT_FIRST_OFFICER, 25, dataManager);
    crewPools[CrewType::CABIN_CREW] = 
        std::make_unique<CrewPool>(CrewType::CABIN_CREW, 100, dataManager);
    crewPools[CrewType::GROUND_LOADER] = 
        std::make_unique<CrewPool>(CrewType::GROUND_LOADER, 30, dataManager);
    crewPools[CrewType::GROUND_CLEANER] = 
        std::make_unique<CrewPool>(CrewType::GROUND_CLEANER, 20, dataManager);
    crewPools[CrewType::GROUND_REFUELER] = 
        std::make_unique<CrewPool>(CrewType::GROUND_REFUELER, 16, dataManager);
    crewPools[CrewType::GROUND_MECHANIC] = 
        std::make_unique<CrewPool>(CrewType::GROUND_MECHANIC, 12, dataManager);
    crewPools[CrewType::ATC_CONTROLLER] = 
        std::make_unique<CrewPool>(CrewType::ATC_CONTROLLER, 15, dataManager);
    crewPools[CrewType::GATE_AGENT] = 
        std::make_unique<CrewPool>(CrewType::GATE_AGENT, 40, dataManager);
}

std::vector<int> CrewManager::assignFlightCrew(int flightId, AircraftType aircraftType,
                                                bool isInternational) {
    pthread_mutex_lock(&managerMutex);
    
    std::vector<int> assigned;
    
    // Assign cockpit crew
    int cockpitNeeded = getRequiredCockpitCrew(aircraftType);
    
    int captain = crewPools[CrewType::PILOT_CAPTAIN]->assignCrew(flightId, aircraftType);
    if (captain >= 0) assigned.push_back(captain);
    
    for (int i = 1; i < cockpitNeeded; i++) {
        int fo = crewPools[CrewType::PILOT_FIRST_OFFICER]->assignCrew(flightId, aircraftType);
        if (fo >= 0) assigned.push_back(fo);
    }
    
    // Assign cabin crew
    int cabinNeeded = getRequiredCabinCrew(aircraftType);
    for (int i = 0; i < cabinNeeded; i++) {
        int fa = crewPools[CrewType::CABIN_CREW]->assignCrew(flightId);
        if (fa >= 0) assigned.push_back(fa);
    }
    
    flightCrewAssignments[flightId] = assigned;
    
    pthread_mutex_unlock(&managerMutex);
    return assigned;
}

void CrewManager::releaseFlightCrew(int flightId) {
    pthread_mutex_lock(&managerMutex);
    
    auto it = flightCrewAssignments.find(flightId);
    if (it != flightCrewAssignments.end()) {
        for (int crewId : it->second) {
            // Find which pool this crew belongs to
            for (auto& poolPair : crewPools) {
                CrewMember* member = poolPair.second->getCrew(crewId);
                if (member && member->currentAssignment == flightId) {
                    poolPair.second->releaseCrew(crewId);
                    break;
                }
            }
        }
        flightCrewAssignments.erase(it);
    }
    
    pthread_mutex_unlock(&managerMutex);
}

void CrewManager::update(TimeUnit elapsedSeconds) {
    pthread_mutex_lock(&managerMutex);
    
    for (auto& pair : crewPools) {
        pair.second->updateFatigue(elapsedSeconds);
    }
    
    pthread_mutex_unlock(&managerMutex);
}

bool CrewManager::handleCrewShortage(CrewType type, int flightId) {
    pthread_mutex_lock(&managerMutex);
    
    auto poolIt = crewPools.find(type);
    if (poolIt == crewPools.end()) {
        pthread_mutex_unlock(&managerMutex);
        return false;
    }
    
    // Try to get standby crew
    std::vector<int> standby = poolIt->second->getStandbyCrew();
    if (!standby.empty()) {
        int crewId = poolIt->second->assignCrew(flightId);
        if (crewId >= 0) {
            flightCrewAssignments[flightId].push_back(crewId);
            pthread_mutex_unlock(&managerMutex);
            return true;
        }
    }
    
    // No standby available - delay operations
    dataManager->addEmergencyAlert(
        "Crew shortage: " + std::to_string(static_cast<int>(type)) + " for flight " + std::to_string(flightId),
        EmergencyType::NONE);
    
    pthread_mutex_unlock(&managerMutex);
    return false;
}

int CrewManager::getAvailableCount(CrewType type) const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&managerMutex));
    
    int count = 0;
    auto it = crewPools.find(type);
    if (it != crewPools.end()) {
        count = it->second->getAvailableCrew().size();
    }
    
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&managerMutex));
    return count;
}

int CrewManager::getOnDutyCount(CrewType type) const {
    // Would need to track this in CrewPool
    return 0;
}

int CrewManager::getFatiguedCount(CrewType type) const {
    // Would iterate through pool
    return 0;
}

int CrewManager::getReplacementCrew(CrewType type, AircraftType certRequired) {
    pthread_mutex_lock(&managerMutex);
    
    int replacement = -1;
    auto poolIt = crewPools.find(type);
    if (poolIt != crewPools.end()) {
        replacement = poolIt->second->assignCrew(-1, certRequired); // -1 = standby
    }
    
    pthread_mutex_unlock(&managerMutex);
    return replacement;
}

int CrewManager::getRequiredCabinCrew(AircraftType type) {
    auto it = Config::AIRCRAFT_SPECS.find(type);
    if (it != Config::AIRCRAFT_SPECS.end()) {
        return it->second.crewRequired;
    }
    return 6;
}

int CrewManager::getRequiredCockpitCrew(AircraftType type) {
    auto it = Config::AIRCRAFT_SPECS.find(type);
    if (it != Config::AIRCRAFT_SPECS.end()) {
        return it->second.cockpitCrew;
    }
    return 2;
}
