/**
 * Smart Airport Operations Management System
 * data_manager.cpp - Multi-Level Data Management Implementation
 */

#include "data_manager.h"
#include <algorithm>
#include <thread>
#include <chrono>

DataManager::DataManager() {
    pthread_rwlock_init(&level1Lock, nullptr);
    pthread_rwlock_init(&level2Lock, nullptr);
    pthread_rwlock_init(&level3Lock, nullptr);
    pthread_rwlock_init(&level4Lock, nullptr);
    pthread_mutex_init(&versionMutex, nullptr);
    
    initializeResources();
    
    // Initialize weather to clear
    currentWeather.type = WeatherType::CLEAR;
    currentWeather.windSpeed = 5;
    currentWeather.windDirection = 270;
    currentWeather.visibility = 10000;
    currentWeather.requiresDeicing = false;
    currentWeather.operationMultiplier = 1.0;
}

DataManager::~DataManager() {
    pthread_rwlock_destroy(&level1Lock);
    pthread_rwlock_destroy(&level2Lock);
    pthread_rwlock_destroy(&level3Lock);
    pthread_rwlock_destroy(&level4Lock);
    pthread_mutex_destroy(&versionMutex);
}

void DataManager::initializeResources() {
    pthread_rwlock_wrlock(&level2Lock);
    
    // Initialize fuel trucks
    for (int i = 0; i < Config::NUM_FUEL_TRUCKS; i++) {
        resourceAvailability[ResourceType::FUEL_TRUCK][i] = true;
    }
    
    // Initialize baggage carts
    for (int i = 0; i < Config::NUM_BAGGAGE_CARTS; i++) {
        resourceAvailability[ResourceType::BAGGAGE_CART][i] = true;
    }
    
    // Initialize catering trucks
    for (int i = 0; i < Config::NUM_CATERING_TRUCKS; i++) {
        resourceAvailability[ResourceType::CATERING_TRUCK][i] = true;
    }
    
    // Initialize cleaning crews
    for (int i = 0; i < Config::NUM_CLEANING_CREWS; i++) {
        resourceAvailability[ResourceType::CLEANING_CREW][i] = true;
    }
    
    // Initialize maintenance teams
    for (int i = 0; i < Config::NUM_MAINTENANCE_TEAMS; i++) {
        resourceAvailability[ResourceType::MAINTENANCE_TEAM][i] = true;
    }
    
    // Initialize jetbridges
    for (int i = 0; i < Config::NUM_JETBRIDGES; i++) {
        resourceAvailability[ResourceType::JETBRIDGE][i] = true;
    }
    
    // Initialize de-icing trucks
    for (int i = 0; i < Config::NUM_DEICING_TRUCKS; i++) {
        resourceAvailability[ResourceType::DEICING_TRUCK][i] = true;
    }
    
    pthread_rwlock_unlock(&level2Lock);
}

// ============================================================================
// Level 1: Critical Real-Time Data
// ============================================================================

void DataManager::setRunwayStatus(int runwayId, RunwayStatus status) {
    simulateAccessDelay(DataLevel::CRITICAL_REALTIME);
    pthread_rwlock_wrlock(&level1Lock);
    runwayStatuses[runwayId] = status;
    pthread_rwlock_unlock(&level1Lock);
}

RunwayStatus DataManager::getRunwayStatus(int runwayId) {
    simulateAccessDelay(DataLevel::CRITICAL_REALTIME);
    pthread_rwlock_rdlock(&level1Lock);
    RunwayStatus status = RunwayStatus::AVAILABLE;
    auto it = runwayStatuses.find(runwayId);
    if (it != runwayStatuses.end()) {
        status = it->second;
    }
    pthread_rwlock_unlock(&level1Lock);
    return status;
}

void DataManager::setGateOccupancy(int gateId, bool occupied, int flightId) {
    simulateAccessDelay(DataLevel::CRITICAL_REALTIME);
    pthread_rwlock_wrlock(&level1Lock);
    gateOccupancy[gateId] = {occupied, flightId};
    pthread_rwlock_unlock(&level1Lock);
}

bool DataManager::isGateOccupied(int gateId) {
    simulateAccessDelay(DataLevel::CRITICAL_REALTIME);
    pthread_rwlock_rdlock(&level1Lock);
    bool occupied = false;
    auto it = gateOccupancy.find(gateId);
    if (it != gateOccupancy.end()) {
        occupied = it->second.first;
    }
    pthread_rwlock_unlock(&level1Lock);
    return occupied;
}

int DataManager::getGateFlightId(int gateId) {
    simulateAccessDelay(DataLevel::CRITICAL_REALTIME);
    pthread_rwlock_rdlock(&level1Lock);
    int flightId = -1;
    auto it = gateOccupancy.find(gateId);
    if (it != gateOccupancy.end()) {
        flightId = it->second.second;
    }
    pthread_rwlock_unlock(&level1Lock);
    return flightId;
}

void DataManager::addEmergencyAlert(const std::string& alert, EmergencyType type) {
    simulateAccessDelay(DataLevel::CRITICAL_REALTIME);
    pthread_rwlock_wrlock(&level1Lock);
    emergencyAlerts.push_back({alert, type});
    pthread_rwlock_unlock(&level1Lock);
}

std::vector<std::string> DataManager::getActiveEmergencies() {
    simulateAccessDelay(DataLevel::CRITICAL_REALTIME);
    pthread_rwlock_rdlock(&level1Lock);
    std::vector<std::string> alerts;
    for (const auto& pair : emergencyAlerts) {
        alerts.push_back(pair.first);
    }
    pthread_rwlock_unlock(&level1Lock);
    return alerts;
}

void DataManager::clearEmergency(const std::string& alertId) {
    simulateAccessDelay(DataLevel::CRITICAL_REALTIME);
    pthread_rwlock_wrlock(&level1Lock);
    emergencyAlerts.erase(
        std::remove_if(emergencyAlerts.begin(), emergencyAlerts.end(),
            [&alertId](const auto& pair) { return pair.first == alertId; }),
        emergencyAlerts.end());
    pthread_rwlock_unlock(&level1Lock);
}

// ============================================================================
// Level 2: Active Operations Data
// ============================================================================

void DataManager::updateFlightSchedule(const Flight& flight) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_wrlock(&level2Lock);
    activeFlights[flight.id] = flight;
    pthread_rwlock_unlock(&level2Lock);
}

std::vector<Flight> DataManager::getUpcomingFlights(TimeUnit window) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_rdlock(&level2Lock);
    std::vector<Flight> flights;
    for (const auto& pair : activeFlights) {
        flights.push_back(pair.second);
    }
    pthread_rwlock_unlock(&level2Lock);
    return flights;
}

Flight* DataManager::getFlightById(int flightId) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_rdlock(&level2Lock);
    Flight* flight = nullptr;
    auto it = activeFlights.find(flightId);
    if (it != activeFlights.end()) {
        flight = &(const_cast<Flight&>(it->second));
    }
    pthread_rwlock_unlock(&level2Lock);
    return flight;
}

void DataManager::setResourceAvailable(ResourceType type, int resourceId, bool available) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_wrlock(&level2Lock);
    resourceAvailability[type][resourceId] = available;
    pthread_rwlock_unlock(&level2Lock);
}

bool DataManager::isResourceAvailable(ResourceType type, int resourceId) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_rdlock(&level2Lock);
    bool available = false;
    auto typeIt = resourceAvailability.find(type);
    if (typeIt != resourceAvailability.end()) {
        auto resIt = typeIt->second.find(resourceId);
        if (resIt != typeIt->second.end()) {
            available = resIt->second;
        }
    }
    pthread_rwlock_unlock(&level2Lock);
    return available;
}

int DataManager::getAvailableResourceCount(ResourceType type) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_rdlock(&level2Lock);
    int count = 0;
    auto typeIt = resourceAvailability.find(type);
    if (typeIt != resourceAvailability.end()) {
        for (const auto& pair : typeIt->second) {
            if (pair.second) count++;
        }
    }
    pthread_rwlock_unlock(&level2Lock);
    return count;
}

std::vector<int> DataManager::getAvailableResources(ResourceType type) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_rdlock(&level2Lock);
    std::vector<int> available;
    auto typeIt = resourceAvailability.find(type);
    if (typeIt != resourceAvailability.end()) {
        for (const auto& pair : typeIt->second) {
            if (pair.second) available.push_back(pair.first);
        }
    }
    pthread_rwlock_unlock(&level2Lock);
    return available;
}

void DataManager::assignCrew(int crewId, int flightId) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_wrlock(&level2Lock);
    crewAssignments[crewId] = flightId;
    pthread_rwlock_unlock(&level2Lock);
}

void DataManager::unassignCrew(int crewId) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_wrlock(&level2Lock);
    crewAssignments.erase(crewId);
    pthread_rwlock_unlock(&level2Lock);
}

std::vector<int> DataManager::getFlightCrew(int flightId) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_rdlock(&level2Lock);
    std::vector<int> crew;
    for (const auto& pair : crewAssignments) {
        if (pair.second == flightId) {
            crew.push_back(pair.first);
        }
    }
    pthread_rwlock_unlock(&level2Lock);
    return crew;
}

void DataManager::updateWeather(const WeatherCondition& weather) {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_wrlock(&level2Lock);
    currentWeather = weather;
    pthread_rwlock_unlock(&level2Lock);
}

WeatherCondition DataManager::getCurrentWeather() {
    simulateAccessDelay(DataLevel::ACTIVE_OPERATIONS);
    pthread_rwlock_rdlock(&level2Lock);
    WeatherCondition weather = currentWeather;
    pthread_rwlock_unlock(&level2Lock);
    return weather;
}

// ============================================================================
// Level 3: System Data
// ============================================================================

void DataManager::setDailySchedule(const std::vector<Flight>& flights) {
    simulateAccessDelay(DataLevel::SYSTEM_DATA);
    pthread_rwlock_wrlock(&level3Lock);
    dailySchedule = flights;
    pthread_rwlock_unlock(&level3Lock);
}

std::vector<Flight> DataManager::getDailySchedule() {
    simulateAccessDelay(DataLevel::SYSTEM_DATA);
    pthread_rwlock_rdlock(&level3Lock);
    std::vector<Flight> schedule = dailySchedule;
    pthread_rwlock_unlock(&level3Lock);
    return schedule;
}

void DataManager::updatePassengerManifest(int flightId, const std::vector<Passenger>& passengers) {
    simulateAccessDelay(DataLevel::SYSTEM_DATA);
    pthread_rwlock_wrlock(&level3Lock);
    passengerManifests[flightId] = passengers;
    pthread_rwlock_unlock(&level3Lock);
}

std::vector<Passenger> DataManager::getPassengerManifest(int flightId) {
    simulateAccessDelay(DataLevel::SYSTEM_DATA);
    pthread_rwlock_rdlock(&level3Lock);
    std::vector<Passenger> passengers;
    auto it = passengerManifests.find(flightId);
    if (it != passengerManifests.end()) {
        passengers = it->second;
    }
    pthread_rwlock_unlock(&level3Lock);
    return passengers;
}

void DataManager::addMaintenanceRecord(int aircraftId, const std::string& record) {
    simulateAccessDelay(DataLevel::SYSTEM_DATA);
    pthread_rwlock_wrlock(&level3Lock);
    maintenanceRecords[aircraftId].push_back(record);
    pthread_rwlock_unlock(&level3Lock);
}

std::vector<std::string> DataManager::getMaintenanceRecords(int aircraftId) {
    simulateAccessDelay(DataLevel::SYSTEM_DATA);
    pthread_rwlock_rdlock(&level3Lock);
    std::vector<std::string> records;
    auto it = maintenanceRecords.find(aircraftId);
    if (it != maintenanceRecords.end()) {
        records = it->second;
    }
    pthread_rwlock_unlock(&level3Lock);
    return records;
}

// ============================================================================
// Level 4: Historical Data
// ============================================================================

void DataManager::archiveFlight(const Flight& flight) {
    simulateAccessDelay(DataLevel::HISTORICAL);
    pthread_rwlock_wrlock(&level4Lock);
    flightHistory.push_back(flight);
    // Keep history bounded (last 7 days worth of flights)
    while (flightHistory.size() > 1000) {
        flightHistory.erase(flightHistory.begin());
    }
    pthread_rwlock_unlock(&level4Lock);
}

std::vector<Flight> DataManager::getFlightHistory(int days) {
    simulateAccessDelay(DataLevel::HISTORICAL);
    pthread_rwlock_rdlock(&level4Lock);
    std::vector<Flight> history = flightHistory;
    pthread_rwlock_unlock(&level4Lock);
    return history;
}

void DataManager::updateStatistics(const std::string& key, double value) {
    simulateAccessDelay(DataLevel::HISTORICAL);
    pthread_rwlock_wrlock(&level4Lock);
    statistics[key] = value;
    pthread_rwlock_unlock(&level4Lock);
}

double DataManager::getStatistic(const std::string& key) {
    simulateAccessDelay(DataLevel::HISTORICAL);
    pthread_rwlock_rdlock(&level4Lock);
    double value = 0.0;
    auto it = statistics.find(key);
    if (it != statistics.end()) {
        value = it->second;
    }
    pthread_rwlock_unlock(&level4Lock);
    return value;
}

std::map<std::string, double> DataManager::getAllStatistics() {
    simulateAccessDelay(DataLevel::HISTORICAL);
    pthread_rwlock_rdlock(&level4Lock);
    std::map<std::string, double> stats = statistics;
    pthread_rwlock_unlock(&level4Lock);
    return stats;
}

void DataManager::addAuditLog(const LogEntry& entry) {
    simulateAccessDelay(DataLevel::HISTORICAL);
    pthread_rwlock_wrlock(&level4Lock);
    auditLogs.push_back(entry);
    // Keep logs bounded
    while (auditLogs.size() > 10000) {
        auditLogs.erase(auditLogs.begin());
    }
    pthread_rwlock_unlock(&level4Lock);
}

std::vector<LogEntry> DataManager::getAuditLogs(TimeUnit since) {
    simulateAccessDelay(DataLevel::HISTORICAL);
    pthread_rwlock_rdlock(&level4Lock);
    std::vector<LogEntry> logs;
    for (const auto& log : auditLogs) {
        if (log.timestamp >= since) {
            logs.push_back(log);
        }
    }
    pthread_rwlock_unlock(&level4Lock);
    return logs;
}

// ============================================================================
// Data Consistency and Versioning
// ============================================================================

int DataManager::getVersion(const std::string& key) {
    pthread_mutex_lock(&versionMutex);
    int version = 0;
    auto it = versionedData.find(key);
    if (it != versionedData.end()) {
        version = it->second.version;
    }
    pthread_mutex_unlock(&versionMutex);
    return version;
}

bool DataManager::updateIfVersion(const std::string& key, const std::string& value, int expectedVersion) {
    pthread_mutex_lock(&versionMutex);
    
    auto it = versionedData.find(key);
    if (it != versionedData.end() && it->second.version != expectedVersion) {
        pthread_mutex_unlock(&versionMutex);
        return false; // Version mismatch - conflict
    }
    
    DataEntry entry;
    entry.key = key;
    entry.value = value;
    entry.version = (it != versionedData.end()) ? it->second.version + 1 : 1;
    entry.lastModified = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    entry.dirty = true;
    
    versionedData[key] = entry;
    
    pthread_mutex_unlock(&versionMutex);
    return true;
}

void DataManager::forceUpdate(const std::string& key, const std::string& value) {
    pthread_mutex_lock(&versionMutex);
    
    DataEntry entry;
    entry.key = key;
    entry.value = value;
    entry.version = (versionedData.count(key) > 0) ? versionedData[key].version + 1 : 1;
    entry.lastModified = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    entry.dirty = true;
    
    versionedData[key] = entry;
    
    pthread_mutex_unlock(&versionMutex);
}

// ============================================================================
// Access Time Simulation
// ============================================================================

TimeUnit DataManager::getAccessTime(DataLevel level) {
    switch (level) {
        case DataLevel::CRITICAL_REALTIME: return Config::DATA_LEVEL_1_ACCESS_TIME;
        case DataLevel::ACTIVE_OPERATIONS: return Config::DATA_LEVEL_2_ACCESS_TIME;
        case DataLevel::SYSTEM_DATA: return Config::DATA_LEVEL_3_ACCESS_TIME;
        case DataLevel::HISTORICAL: return Config::DATA_LEVEL_4_ACCESS_TIME;
    }
    return 1;
}

void DataManager::simulateAccessDelay(DataLevel level) {
    // In real-time mode, simulate actual delays
    // Scale down for simulation (1 time unit = 1ms for responsiveness)
    // This preserves the relative difference between levels while keeping UI responsive
    TimeUnit delay = getAccessTime(level);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}
