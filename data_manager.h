/**
 * Smart Airport Operations Management System
 * data_manager.h - Multi-Level Data Management System
 * 
 * Implements 4-level hierarchical data storage with different access times:
 * Level 1: Critical Real-Time Data (1 time unit)
 * Level 2: Active Operations Data (5 time units)
 * Level 3: System Data (20 time units)
 * Level 4: Historical Data (100 time units)
 */

#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "types.h"
#include "config.h"
#include <map>
#include <vector>
#include <string>
#include <atomic>

// Data entry with version control
struct DataEntry {
    std::string key;
    std::string value;
    int version;
    TimeUnit lastModified;
    DataLevel level;
    bool dirty;
};

/**
 * Multi-Level Data Manager
 * 
 * Provides tiered data access with different latencies
 * and concurrency control for data consistency
 */
class DataManager {
public:
    DataManager();
    ~DataManager();
    
    // ========================================================================
    // Level 1: Critical Real-Time Data (1 time unit access)
    // ========================================================================
    
    // Runway status
    void setRunwayStatus(int runwayId, RunwayStatus status);
    RunwayStatus getRunwayStatus(int runwayId);
    
    // Gate occupancy
    void setGateOccupancy(int gateId, bool occupied, int flightId = -1);
    bool isGateOccupied(int gateId);
    int getGateFlightId(int gateId);
    
    // Emergency alerts
    void addEmergencyAlert(const std::string& alert, EmergencyType type);
    std::vector<std::string> getActiveEmergencies();
    void clearEmergency(const std::string& alertId);
    
    // ========================================================================
    // Level 2: Active Operations Data (5 time units access)
    // ========================================================================
    
    // Flight schedules (next 2 hours)
    void updateFlightSchedule(const Flight& flight);
    std::vector<Flight> getUpcomingFlights(TimeUnit window = 7200); // 2 hours
    Flight* getFlightById(int flightId);
    
    // Resource availability
    void setResourceAvailable(ResourceType type, int resourceId, bool available);
    bool isResourceAvailable(ResourceType type, int resourceId);
    int getAvailableResourceCount(ResourceType type);
    std::vector<int> getAvailableResources(ResourceType type);
    
    // Crew assignments
    void assignCrew(int crewId, int flightId);
    void unassignCrew(int crewId);
    std::vector<int> getFlightCrew(int flightId);
    
    // Weather data
    void updateWeather(const WeatherCondition& weather);
    WeatherCondition getCurrentWeather();
    
    // ========================================================================
    // Level 3: System Data (20 time units access)
    // ========================================================================
    
    // Full daily schedule
    void setDailySchedule(const std::vector<Flight>& flights);
    std::vector<Flight> getDailySchedule();
    
    // Passenger manifests
    void updatePassengerManifest(int flightId, const std::vector<Passenger>& passengers);
    std::vector<Passenger> getPassengerManifest(int flightId);
    
    // Maintenance records
    void addMaintenanceRecord(int aircraftId, const std::string& record);
    std::vector<std::string> getMaintenanceRecords(int aircraftId);
    
    // ========================================================================
    // Level 4: Historical Data (100 time units access)
    // ========================================================================
    
    // Past flight records
    void archiveFlight(const Flight& flight);
    std::vector<Flight> getFlightHistory(int days = 7);
    
    // Statistics
    void updateStatistics(const std::string& key, double value);
    double getStatistic(const std::string& key);
    std::map<std::string, double> getAllStatistics();
    
    // Audit logs
    void addAuditLog(const LogEntry& entry);
    std::vector<LogEntry> getAuditLogs(TimeUnit since);
    
    // ========================================================================
    // Data Consistency and Versioning
    // ========================================================================
    
    // Get data version
    int getVersion(const std::string& key);
    
    // Optimistic locking - update only if version matches
    bool updateIfVersion(const std::string& key, const std::string& value, int expectedVersion);
    
    // Force update (for critical data)
    void forceUpdate(const std::string& key, const std::string& value);
    
    // ========================================================================
    // Access Time Simulation
    // ========================================================================
    
    // Get access time for data level
    TimeUnit getAccessTime(DataLevel level);
    
    // Simulate access delay
    void simulateAccessDelay(DataLevel level);
    
private:
    // Level 1 data structures
    std::map<int, RunwayStatus> runwayStatuses;
    std::map<int, std::pair<bool, int>> gateOccupancy; // (occupied, flightId)
    std::vector<std::pair<std::string, EmergencyType>> emergencyAlerts;
    pthread_rwlock_t level1Lock;
    
    // Level 2 data structures
    std::map<int, Flight> activeFlights;
    std::map<ResourceType, std::map<int, bool>> resourceAvailability;
    std::map<int, int> crewAssignments; // crewId -> flightId
    WeatherCondition currentWeather;
    pthread_rwlock_t level2Lock;
    
    // Level 3 data structures
    std::vector<Flight> dailySchedule;
    std::map<int, std::vector<Passenger>> passengerManifests;
    std::map<int, std::vector<std::string>> maintenanceRecords;
    pthread_rwlock_t level3Lock;
    
    // Level 4 data structures
    std::vector<Flight> flightHistory;
    std::map<std::string, double> statistics;
    std::vector<LogEntry> auditLogs;
    pthread_rwlock_t level4Lock;
    
    // Version control
    std::map<std::string, DataEntry> versionedData;
    pthread_mutex_t versionMutex;
    
    // Initialize resource pools
    void initializeResources();
};

#endif // DATA_MANAGER_H
