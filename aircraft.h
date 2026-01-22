/**
 * Smart Airport Operations Management System
 * aircraft.h - Aircraft, Runway, Gate, and Taxiway Management
 * 
 * Consolidated module handling:
 * - Aircraft types and specifications
 * - Multi-runway air traffic management
 * - Gate assignment with constraints
 * - Graph-based taxiway network
 */

#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include "types.h"
#include "config.h"
#include "data_manager.h"
#include <vector>
#include <map>
#include <queue>
#include <set>

// Forward declarations
class Scheduler;

/**
 * Runway Manager
 * 
 * Handles multi-runway operations including:
 * - Wake turbulence separation
 * - Landing/takeoff coordination
 * - Go-around procedures
 * - Wind-based runway selection
 */
class RunwayManager {
public:
    RunwayManager(DataManager* dm);
    ~RunwayManager();
    
    void initialize(int numRunways);
    
    // Runway operations
    int requestLanding(int flightId, WakeTurbulenceCategory wakeCategory);
    int requestTakeoff(int flightId, WakeTurbulenceCategory wakeCategory);
    void releaseLanding(int runwayId);
    void releaseTakeoff(int runwayId);
    
    // Status
    std::vector<Runway> getAllRunways();
    Runway* getRunway(int runwayId);
    bool isRunwayAvailable(int runwayId);
    
    // Weather effects
    void handleWindChange(int windDirection, int windSpeed);
    void closeRunwayForWeather(int runwayId, TimeUnit duration);
    void reopenRunway(int runwayId);
    
    // Go-around
    void initiateGoAround(int flightId, int runwayId);
    
    // Wake turbulence separation
    TimeUnit getRequiredSeparation(WakeTurbulenceCategory leading, 
                                    WakeTurbulenceCategory following);
    
private:
    DataManager* dataManager;
    std::vector<Runway> runways;
    pthread_mutex_t runwayMutex;
    
    // Last aircraft category per runway (for wake separation)
    std::map<int, std::pair<WakeTurbulenceCategory, TimeUnit>> lastLandingCategory;
    
    // Approach queues per runway
    std::map<int, std::queue<int>> approachQueues;
    
    // Calculate optimal runway based on wind
    int selectOptimalRunway(int windDirection);
};

/**
 * Gate Manager
 * 
 * Handles gate assignment with constraints:
 * - International vs domestic
 * - Aircraft size compatibility
 * - Walking distance optimization
 */
class GateManager {
public:
    GateManager(DataManager* dm);
    ~GateManager();
    
    void initialize();
    
    // Gate assignment
    int assignGate(int flightId, AircraftType aircraftType, FlightType flightType);
    void releaseGate(int gateId);
    
    // Status
    std::vector<Gate> getAllGates();
    Gate* getGate(int gateId);
    bool isGateAvailable(int gateId);
    
    // Optimization
    int findOptimalGate(int flightId, AircraftType aircraftType, 
                        FlightType flightType, int connectingPassengers);
    
    // Walking distance
    double calculateWalkingDistance(int fromGate, int toGate);
    
private:
    DataManager* dataManager;
    std::vector<Gate> gates;
    pthread_mutex_t gateMutex;
    
    // Check if gate is compatible with aircraft
    bool isGateCompatible(const Gate& gate, AircraftType aircraftType, FlightType flightType);
    
    // Get required gate size for aircraft
    int getRequiredGateSize(AircraftType type);
};

/**
 * Taxiway Network
 * 
 * Graph-based taxiway management:
 * - Pathfinding with A*
 * - Gridlock detection and resolution
 * - Alternative routing
 */
class TaxiwayNetwork {
public:
    TaxiwayNetwork(DataManager* dm);
    ~TaxiwayNetwork();
    
    void initialize();
    
    // Path operations
    std::vector<int> findPath(int fromNodeId, int toNodeId);
    std::vector<int> findAlternatePath(int fromNodeId, int toNodeId, 
                                       const std::set<int>& blockedNodes);
    
    // Movement
    bool requestNode(int nodeId, int flightId);
    void releaseNode(int nodeId);
    bool isNodeOccupied(int nodeId);
    
    // Gridlock detection
    bool detectGridlock();
    void resolveGridlock();
    
    // Pushback coordination
    bool requestPushback(int gateId, int flightId);
    void completePushback(int gateId, int flightId);
    
    // Get nodes
    std::vector<TaxiwayNode> getAllNodes();
    TaxiwayNode* getNode(int nodeId);
    
    // Get runway/gate locations
    int getRunwayNode(int runwayId, bool departure);
    int getGateNode(int gateId);
    
private:
    DataManager* dataManager;
    std::vector<TaxiwayNode> nodes;
    std::vector<std::vector<double>> adjacencyMatrix;
    pthread_mutex_t taxiwayMutex;
    
    // A* pathfinding helper
    double heuristic(int fromNode, int toNode);
    
    // Build adjacency matrix
    void buildAdjacencyMatrix();
};

/**
 * Flight Manager
 * 
 * Central coordination of flight operations
 */
class FlightManager {
public:
    FlightManager(DataManager* dm, Scheduler* sched);
    ~FlightManager();
    
    void setRunwayManager(RunwayManager* rm) { runwayManager = rm; }
    void setGateManager(GateManager* gm) { gateManager = gm; }
    void setTaxiwayNetwork(TaxiwayNetwork* tn) { taxiwayNetwork = tn; }
    
    // Flight lifecycle
    int createFlight(const std::string& flightNumber, AircraftType type, 
                     FlightType flightType, TimeUnit scheduledArrival,
                     TimeUnit scheduledDeparture);
    
    void processArrival(int flightId);
    void processDeparture(int flightId);
    void updateFlightStatus(int flightId, FlightStatus newStatus);
    void activateScheduledFlights(TimeUnit currentTime);
    
    // Flight queries
    Flight* getFlight(int flightId);
    std::vector<Flight*> getFlightsByStatus(FlightStatus status);
    std::vector<Flight*> getAllActiveFlights();
    
    // Emergency handling
    void declareEmergency(int flightId, EmergencyType type);
    void clearEmergency(int flightId);
    
    // Delay management
    void addDelay(int flightId, int delayMinutes, const std::string& reason);
    
    // Statistics
    int getTotalActiveFlights();
    int getFlightsInStatus(FlightStatus status);
    double getAverageDelay();
    
private:
    DataManager* dataManager;
    Scheduler* scheduler;
    RunwayManager* runwayManager = nullptr;
    GateManager* gateManager = nullptr;
    TaxiwayNetwork* taxiwayNetwork = nullptr;
    
    std::map<int, Flight> flights;
    std::atomic<int> nextFlightId{1};
    pthread_rwlock_t flightLock;
    
    // Calculate priority for flight
    int calculateFlightPriority(const Flight& flight);
    
    // Get aircraft specs
    AircraftSpecs getAircraftSpecs(AircraftType type);
};

#endif // AIRCRAFT_H
