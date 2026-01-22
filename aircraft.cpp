/**
 * Smart Airport Operations Management System
 * aircraft.cpp - Aircraft, Runway, Gate, and Taxiway Implementation
 */

#include "aircraft.h"
#include "scheduler.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <limits>
#include <sstream>
#include <iostream>

// ============================================================================
// RunwayManager Implementation
// ============================================================================

RunwayManager::RunwayManager(DataManager* dm) : dataManager(dm) {
    pthread_mutex_init(&runwayMutex, nullptr);
}

RunwayManager::~RunwayManager() {
    pthread_mutex_destroy(&runwayMutex);
}

void RunwayManager::initialize(int numRunways) {
    pthread_mutex_lock(&runwayMutex);
    
    runways.clear();
    for (int i = 0; i < numRunways; i++) {
        Runway rwy;
        rwy.id = i;
        rwy.name = "RWY" + std::to_string(i * 9 + 9) + "L/R";
        rwy.heading = (i * 90) % 360;
        rwy.length = 3500 + (i * 500); // 3500-4500m
        rwy.canHandleHeavy = true;
        rwy.canHandleSuper = (i == 0); // Only first runway for A380
        rwy.status = RunwayStatus::AVAILABLE;
        rwy.currentFlightId = -1;
        rwy.busyUntil = 0;
        runways.push_back(rwy);
        
        dataManager->setRunwayStatus(i, RunwayStatus::AVAILABLE);
    }
    
    pthread_mutex_unlock(&runwayMutex);
}

int RunwayManager::requestLanding(int flightId, WakeTurbulenceCategory wakeCategory) {
    pthread_mutex_lock(&runwayMutex);
    
    TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Find suitable runway
    int selectedRunway = -1;
    TimeUnit earliestAvailable = std::numeric_limits<TimeUnit>::max();
    
    for (auto& runway : runways) {
        if (runway.status != RunwayStatus::AVAILABLE) continue;
        
        // Check if runway can handle this category
        if (wakeCategory == WakeTurbulenceCategory::SUPER && !runway.canHandleSuper) continue;
        if (wakeCategory == WakeTurbulenceCategory::HEAVY && !runway.canHandleHeavy) continue;
        
        // Check wake turbulence separation
        TimeUnit requiredSep = 0;
        auto lastIt = lastLandingCategory.find(runway.id);
        if (lastIt != lastLandingCategory.end()) {
            TimeUnit timeSinceLast = currentTime - lastIt->second.second;
            requiredSep = getRequiredSeparation(lastIt->second.first, wakeCategory);
            if (timeSinceLast < requiredSep) {
                TimeUnit availableAt = lastIt->second.second + requiredSep;
                if (availableAt < earliestAvailable) {
                    earliestAvailable = availableAt;
                    selectedRunway = runway.id;
                }
                continue;
            }
        }
        
        // Runway is immediately available
        if (runway.busyUntil <= currentTime) {
            selectedRunway = runway.id;
            break;
        }
    }
    
    if (selectedRunway != -1) {
        runways[selectedRunway].status = RunwayStatus::OCCUPIED_LANDING;
        runways[selectedRunway].currentFlightId = flightId;
        runways[selectedRunway].busyUntil = currentTime + 120; // 2 min for landing
        
        lastLandingCategory[selectedRunway] = {wakeCategory, currentTime};
        
        dataManager->setRunwayStatus(selectedRunway, RunwayStatus::OCCUPIED_LANDING);
    }
    
    pthread_mutex_unlock(&runwayMutex);
    return selectedRunway;
}

int RunwayManager::requestTakeoff(int flightId, WakeTurbulenceCategory wakeCategory) {
    pthread_mutex_lock(&runwayMutex);
    
    TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    int selectedRunway = -1;
    
    for (auto& runway : runways) {
        if (runway.status != RunwayStatus::AVAILABLE) continue;
        
        if (wakeCategory == WakeTurbulenceCategory::SUPER && !runway.canHandleSuper) continue;
        if (wakeCategory == WakeTurbulenceCategory::HEAVY && !runway.canHandleHeavy) continue;
        
        if (runway.busyUntil <= currentTime) {
            selectedRunway = runway.id;
            break;
        }
    }
    
    if (selectedRunway != -1) {
        runways[selectedRunway].status = RunwayStatus::OCCUPIED_TAKEOFF;
        runways[selectedRunway].currentFlightId = flightId;
        runways[selectedRunway].busyUntil = currentTime + 90; // 1.5 min for takeoff
        
        dataManager->setRunwayStatus(selectedRunway, RunwayStatus::OCCUPIED_TAKEOFF);
    }
    
    pthread_mutex_unlock(&runwayMutex);
    return selectedRunway;
}

void RunwayManager::releaseLanding(int runwayId) {
    pthread_mutex_lock(&runwayMutex);
    
    if (runwayId >= 0 && runwayId < static_cast<int>(runways.size())) {
        runways[runwayId].status = RunwayStatus::AVAILABLE;
        runways[runwayId].currentFlightId = -1;
        dataManager->setRunwayStatus(runwayId, RunwayStatus::AVAILABLE);
    }
    
    pthread_mutex_unlock(&runwayMutex);
}

void RunwayManager::releaseTakeoff(int runwayId) {
    pthread_mutex_lock(&runwayMutex);
    
    if (runwayId >= 0 && runwayId < static_cast<int>(runways.size())) {
        runways[runwayId].status = RunwayStatus::AVAILABLE;
        runways[runwayId].currentFlightId = -1;
        dataManager->setRunwayStatus(runwayId, RunwayStatus::AVAILABLE);
    }
    
    pthread_mutex_unlock(&runwayMutex);
}

std::vector<Runway> RunwayManager::getAllRunways() {
    pthread_mutex_lock(&runwayMutex);
    std::vector<Runway> result = runways;
    pthread_mutex_unlock(&runwayMutex);
    return result;
}

Runway* RunwayManager::getRunway(int runwayId) {
    pthread_mutex_lock(&runwayMutex);
    Runway* rwy = nullptr;
    if (runwayId >= 0 && runwayId < static_cast<int>(runways.size())) {
        rwy = &runways[runwayId];
    }
    pthread_mutex_unlock(&runwayMutex);
    return rwy;
}

bool RunwayManager::isRunwayAvailable(int runwayId) {
    pthread_mutex_lock(&runwayMutex);
    bool available = false;
    if (runwayId >= 0 && runwayId < static_cast<int>(runways.size())) {
        available = runways[runwayId].status == RunwayStatus::AVAILABLE;
    }
    pthread_mutex_unlock(&runwayMutex);
    return available;
}

void RunwayManager::handleWindChange(int windDirection, int windSpeed) {
    pthread_mutex_lock(&runwayMutex);
    
    // Select optimal runway configuration based on wind
    int optimalRunway = selectOptimalRunway(windDirection);
    
    // In high winds, close runways that are too crosswind
    for (auto& runway : runways) {
        int crosswindAngle = std::abs(windDirection - runway.heading);
        if (crosswindAngle > 180) crosswindAngle = 360 - crosswindAngle;
        
        // Close if crosswind component > 35 knots
        double crosswindComponent = windSpeed * std::sin(crosswindAngle * 3.14159 / 180);
        if (crosswindComponent > Config::HIGH_WIND_THRESHOLD) {
            runway.status = RunwayStatus::CLOSED_WEATHER;
            dataManager->setRunwayStatus(runway.id, RunwayStatus::CLOSED_WEATHER);
        }
    }
    
    pthread_mutex_unlock(&runwayMutex);
}

void RunwayManager::closeRunwayForWeather(int runwayId, TimeUnit duration) {
    pthread_mutex_lock(&runwayMutex);
    
    if (runwayId >= 0 && runwayId < static_cast<int>(runways.size())) {
        runways[runwayId].status = RunwayStatus::CLOSED_WEATHER;
        TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        runways[runwayId].busyUntil = currentTime + duration;
        dataManager->setRunwayStatus(runwayId, RunwayStatus::CLOSED_WEATHER);
    }
    
    pthread_mutex_unlock(&runwayMutex);
}

void RunwayManager::reopenRunway(int runwayId) {
    pthread_mutex_lock(&runwayMutex);
    
    if (runwayId >= 0 && runwayId < static_cast<int>(runways.size())) {
        runways[runwayId].status = RunwayStatus::AVAILABLE;
        runways[runwayId].busyUntil = 0;
        dataManager->setRunwayStatus(runwayId, RunwayStatus::AVAILABLE);
    }
    
    pthread_mutex_unlock(&runwayMutex);
}

void RunwayManager::initiateGoAround(int flightId, int runwayId) {
    pthread_mutex_lock(&runwayMutex);
    
    if (runwayId >= 0 && runwayId < static_cast<int>(runways.size())) {
        runways[runwayId].status = RunwayStatus::AVAILABLE;
        runways[runwayId].currentFlightId = -1;
        dataManager->setRunwayStatus(runwayId, RunwayStatus::AVAILABLE);
    }
    
    pthread_mutex_unlock(&runwayMutex);
    
    // Add flight back to approach queue
    dataManager->addEmergencyAlert("Go-around: Flight " + std::to_string(flightId), 
                                    EmergencyType::NONE);
}

TimeUnit RunwayManager::getRequiredSeparation(WakeTurbulenceCategory leading,
                                               WakeTurbulenceCategory following) {
    auto key = std::make_pair(leading, following);
    auto it = Config::WAKE_SEPARATION.find(key);
    if (it != Config::WAKE_SEPARATION.end()) {
        return it->second;
    }
    return 90; // Default 90 seconds
}

int RunwayManager::selectOptimalRunway(int windDirection) {
    int bestRunway = 0;
    int minCrosswind = 360;
    
    for (const auto& runway : runways) {
        int crosswindAngle = std::abs(windDirection - runway.heading);
        if (crosswindAngle > 180) crosswindAngle = 360 - crosswindAngle;
        
        if (crosswindAngle < minCrosswind) {
            minCrosswind = crosswindAngle;
            bestRunway = runway.id;
        }
    }
    
    return bestRunway;
}

// ============================================================================
// GateManager Implementation
// ============================================================================

GateManager::GateManager(DataManager* dm) : dataManager(dm) {
    pthread_mutex_init(&gateMutex, nullptr);
}

GateManager::~GateManager() {
    pthread_mutex_destroy(&gateMutex);
}

void GateManager::initialize() {
    pthread_mutex_lock(&gateMutex);
    
    gates.clear();
    int gateId = 0;
    
    // International large gates (A380 capable)
    for (int i = 0; i < Config::NUM_GATES_INTERNATIONAL_LARGE; i++) {
        Gate gate;
        gate.id = gateId++;
        gate.name = "A" + std::to_string(i + 1);
        gate.type = GateType::INTERNATIONAL_LARGE;
        gate.sizeCapacity = 4;
        gate.hasCustoms = true;
        gate.isOccupied = false;
        gate.currentFlightId = -1;
        gate.terminalId = 1;
        gate.walkingDistanceToSecurity = 200 + i * 50;
        gates.push_back(gate);
        dataManager->setGateOccupancy(gate.id, false);
    }
    
    // International medium gates
    for (int i = 0; i < Config::NUM_GATES_INTERNATIONAL_MEDIUM; i++) {
        Gate gate;
        gate.id = gateId++;
        gate.name = "B" + std::to_string(i + 1);
        gate.type = GateType::INTERNATIONAL_MEDIUM;
        gate.sizeCapacity = 3;
        gate.hasCustoms = true;
        gate.isOccupied = false;
        gate.currentFlightId = -1;
        gate.terminalId = 1;
        gate.walkingDistanceToSecurity = 300 + i * 40;
        gates.push_back(gate);
        dataManager->setGateOccupancy(gate.id, false);
    }
    
    // Domestic large gates
    for (int i = 0; i < Config::NUM_GATES_DOMESTIC_LARGE; i++) {
        Gate gate;
        gate.id = gateId++;
        gate.name = "C" + std::to_string(i + 1);
        gate.type = GateType::DOMESTIC_LARGE;
        gate.sizeCapacity = 3;
        gate.hasCustoms = false;
        gate.isOccupied = false;
        gate.currentFlightId = -1;
        gate.terminalId = 2;
        gate.walkingDistanceToSecurity = 150 + i * 30;
        gates.push_back(gate);
        dataManager->setGateOccupancy(gate.id, false);
    }
    
    // Domestic medium gates
    for (int i = 0; i < Config::NUM_GATES_DOMESTIC_MEDIUM; i++) {
        Gate gate;
        gate.id = gateId++;
        gate.name = "D" + std::to_string(i + 1);
        gate.type = GateType::DOMESTIC_MEDIUM;
        gate.sizeCapacity = 2;
        gate.hasCustoms = false;
        gate.isOccupied = false;
        gate.currentFlightId = -1;
        gate.terminalId = 2;
        gate.walkingDistanceToSecurity = 100 + i * 25;
        gates.push_back(gate);
        dataManager->setGateOccupancy(gate.id, false);
    }
    
    // Cargo gates
    for (int i = 0; i < Config::NUM_GATES_CARGO; i++) {
        Gate gate;
        gate.id = gateId++;
        gate.name = "F" + std::to_string(i + 1);
        gate.type = GateType::CARGO;
        gate.sizeCapacity = 3;
        gate.hasCustoms = true;
        gate.isOccupied = false;
        gate.currentFlightId = -1;
        gate.terminalId = 3;
        gate.walkingDistanceToSecurity = 0; // No passengers
        gates.push_back(gate);
        dataManager->setGateOccupancy(gate.id, false);
    }
    
    // Private gates
    for (int i = 0; i < Config::NUM_GATES_PRIVATE; i++) {
        Gate gate;
        gate.id = gateId++;
        gate.name = "G" + std::to_string(i + 1);
        gate.type = GateType::PRIVATE;
        gate.sizeCapacity = 1;
        gate.hasCustoms = true;
        gate.isOccupied = false;
        gate.currentFlightId = -1;
        gate.terminalId = 4;
        gate.walkingDistanceToSecurity = 50;
        gates.push_back(gate);
        dataManager->setGateOccupancy(gate.id, false);
    }
    
    pthread_mutex_unlock(&gateMutex);
}

int GateManager::assignGate(int flightId, AircraftType aircraftType, FlightType flightType) {
    pthread_mutex_lock(&gateMutex);
    
    int selectedGate = -1;
    int requiredSize = getRequiredGateSize(aircraftType);
    
    for (auto& gate : gates) {
        if (gate.isOccupied) continue;
        if (!isGateCompatible(gate, aircraftType, flightType)) continue;
        
        selectedGate = gate.id;
        gate.isOccupied = true;
        gate.currentFlightId = flightId;
        dataManager->setGateOccupancy(gate.id, true, flightId);
        break;
    }
    
    pthread_mutex_unlock(&gateMutex);
    return selectedGate;
}

void GateManager::releaseGate(int gateId) {
    pthread_mutex_lock(&gateMutex);
    
    for (auto& gate : gates) {
        if (gate.id == gateId) {
            gate.isOccupied = false;
            gate.currentFlightId = -1;
            dataManager->setGateOccupancy(gate.id, false);
            break;
        }
    }
    
    pthread_mutex_unlock(&gateMutex);
}

std::vector<Gate> GateManager::getAllGates() {
    pthread_mutex_lock(&gateMutex);
    std::vector<Gate> result = gates;
    pthread_mutex_unlock(&gateMutex);
    return result;
}

Gate* GateManager::getGate(int gateId) {
    pthread_mutex_lock(&gateMutex);
    Gate* result = nullptr;
    for (auto& gate : gates) {
        if (gate.id == gateId) {
            result = &gate;
            break;
        }
    }
    pthread_mutex_unlock(&gateMutex);
    return result;
}

bool GateManager::isGateAvailable(int gateId) {
    pthread_mutex_lock(&gateMutex);
    bool available = false;
    for (const auto& gate : gates) {
        if (gate.id == gateId) {
            available = !gate.isOccupied;
            break;
        }
    }
    pthread_mutex_unlock(&gateMutex);
    return available;
}

int GateManager::findOptimalGate(int flightId, AircraftType aircraftType,
                                  FlightType flightType, int connectingPassengers) {
    pthread_mutex_lock(&gateMutex);
    
    int bestGate = -1;
    double bestScore = -1;
    
    for (auto& gate : gates) {
        if (gate.isOccupied) continue;
        if (!isGateCompatible(gate, aircraftType, flightType)) continue;
        
        // Score based on walking distance (lower is better for connections)
        double score = 1000 - gate.walkingDistanceToSecurity;
        if (connectingPassengers > 0) {
            score += (1000 - gate.walkingDistanceToSecurity) * 0.5;
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestGate = gate.id;
        }
    }
    
    pthread_mutex_unlock(&gateMutex);
    return bestGate;
}

double GateManager::calculateWalkingDistance(int fromGate, int toGate) {
    double distance = 0;
    
    pthread_mutex_lock(&gateMutex);
    for (const auto& gate : gates) {
        if (gate.id == fromGate) {
            distance = gate.walkingDistanceToSecurity;
        }
        if (gate.id == toGate) {
            distance += gate.walkingDistanceToSecurity;
        }
    }
    pthread_mutex_unlock(&gateMutex);
    
    return distance;
}

bool GateManager::isGateCompatible(const Gate& gate, AircraftType aircraftType, 
                                    FlightType flightType) {
    int requiredSize = getRequiredGateSize(aircraftType);
    
    // Check size - this is the primary constraint
    if (gate.sizeCapacity < requiredSize) return false;
    
    // Cargo aircraft must use cargo gates
    if (aircraftType == AircraftType::B747F || aircraftType == AircraftType::B777F) {
        return gate.type == GateType::CARGO;
    }
    
    // Private jets should use private gates
    if (aircraftType == AircraftType::G650 || aircraftType == AircraftType::FALCON_7X) {
        return gate.type == GateType::PRIVATE;
    }
    
    // Cargo flights use cargo gates
    if (flightType == FlightType::CARGO) {
        return gate.type == GateType::CARGO;
    }
    
    // Private flights use private gates
    if (flightType == FlightType::PRIVATE) {
        return gate.type == GateType::PRIVATE;
    }
    
    // For regular passenger aircraft: any passenger gate is fine
    // International flights prefer gates with customs, but can use domestic if needed
    // (Large aircraft take priority - they need the space)
    if (gate.type == GateType::CARGO || gate.type == GateType::PRIVATE) {
        return false;  // Regular passenger flights can't use cargo/private
    }
    
    return true;  // Any passenger gate will work
}

int GateManager::getRequiredGateSize(AircraftType type) {
    auto it = Config::AIRCRAFT_SPECS.find(type);
    if (it != Config::AIRCRAFT_SPECS.end()) {
        return it->second.requiredGateSize;
    }
    return 2; // Default medium
}

// ============================================================================
// TaxiwayNetwork Implementation
// ============================================================================

TaxiwayNetwork::TaxiwayNetwork(DataManager* dm) : dataManager(dm) {
    pthread_mutex_init(&taxiwayMutex, nullptr);
}

TaxiwayNetwork::~TaxiwayNetwork() {
    pthread_mutex_destroy(&taxiwayMutex);
}

void TaxiwayNetwork::initialize() {
    pthread_mutex_lock(&taxiwayMutex);
    
    nodes.clear();
    
    // Create taxiway nodes (simplified grid layout)
    int nodeId = 0;
    
    // Runway nodes (3 runways x 2 ends)
    for (int r = 0; r < Config::NUM_RUNWAYS; r++) {
        TaxiwayNode arrivalNode;
        arrivalNode.id = nodeId++;
        arrivalNode.name = "RWY" + std::to_string(r) + "_ARR";
        arrivalNode.x = 0;
        arrivalNode.y = r * 100;
        arrivalNode.isRunwayCrossing = true;
        arrivalNode.runwayId = r;
        arrivalNode.isGateArea = false;
        arrivalNode.gateId = -1;
        arrivalNode.isOccupied = false;
        nodes.push_back(arrivalNode);
        
        TaxiwayNode departureNode;
        departureNode.id = nodeId++;
        departureNode.name = "RWY" + std::to_string(r) + "_DEP";
        departureNode.x = 500;
        departureNode.y = r * 100;
        departureNode.isRunwayCrossing = true;
        departureNode.runwayId = r;
        departureNode.isGateArea = false;
        departureNode.gateId = -1;
        departureNode.isOccupied = false;
        nodes.push_back(departureNode);
    }
    
    // Main taxiway spine
    for (int i = 0; i < 10; i++) {
        TaxiwayNode spineNode;
        spineNode.id = nodeId++;
        spineNode.name = "ALPHA" + std::to_string(i);
        spineNode.x = 50 + i * 50;
        spineNode.y = 50;
        spineNode.isIntersection = true;
        spineNode.isRunwayCrossing = false;
        spineNode.runwayId = -1;
        spineNode.isGateArea = false;
        spineNode.gateId = -1;
        spineNode.isOccupied = false;
        nodes.push_back(spineNode);
    }
    
    // Gate area nodes
    for (int g = 0; g < Config::TOTAL_GATES && g < 30; g++) {
        TaxiwayNode gateNode;
        gateNode.id = nodeId++;
        gateNode.name = "GATE" + std::to_string(g);
        gateNode.x = 100 + (g % 10) * 40;
        gateNode.y = 150 + (g / 10) * 50;
        gateNode.isIntersection = false;
        gateNode.isRunwayCrossing = false;
        gateNode.runwayId = -1;
        gateNode.isGateArea = true;
        gateNode.gateId = g;
        gateNode.isOccupied = false;
        nodes.push_back(gateNode);
    }
    
    // Build connections
    for (auto& node : nodes) {
        node.connectedNodes.clear();
    }
    
    // Connect runway nodes to spine
    for (int r = 0; r < Config::NUM_RUNWAYS; r++) {
        int arrivalNode = r * 2;
        int departureNode = r * 2 + 1;
        int spineStart = Config::NUM_RUNWAYS * 2;
        
        nodes[arrivalNode].connectedNodes.push_back(spineStart + r);
        nodes[spineStart + r].connectedNodes.push_back(arrivalNode);
        
        nodes[departureNode].connectedNodes.push_back(spineStart + 9 - r);
        nodes[spineStart + 9 - r].connectedNodes.push_back(departureNode);
    }
    
    // Connect spine nodes
    int spineStart = Config::NUM_RUNWAYS * 2;
    for (int i = 0; i < 9; i++) {
        nodes[spineStart + i].connectedNodes.push_back(spineStart + i + 1);
        nodes[spineStart + i + 1].connectedNodes.push_back(spineStart + i);
    }
    
    // Connect gates to spine
    int gateStart = Config::NUM_RUNWAYS * 2 + 10;
    for (size_t g = 0; g < nodes.size() - gateStart; g++) {
        int spineNode = spineStart + (g % 10);
        nodes[gateStart + g].connectedNodes.push_back(spineNode);
        nodes[spineNode].connectedNodes.push_back(gateStart + g);
    }
    
    buildAdjacencyMatrix();
    
    pthread_mutex_unlock(&taxiwayMutex);
}

std::vector<int> TaxiwayNetwork::findPath(int fromNodeId, int toNodeId) {
    pthread_mutex_lock(&taxiwayMutex);
    
    std::vector<int> path;
    
    if (fromNodeId < 0 || fromNodeId >= static_cast<int>(nodes.size()) ||
        toNodeId < 0 || toNodeId >= static_cast<int>(nodes.size())) {
        pthread_mutex_unlock(&taxiwayMutex);
        return path;
    }
    
    // A* pathfinding
    std::vector<double> gScore(nodes.size(), std::numeric_limits<double>::max());
    std::vector<double> fScore(nodes.size(), std::numeric_limits<double>::max());
    std::vector<int> cameFrom(nodes.size(), -1);
    std::set<int> closedSet;
    
    auto compare = [&fScore](int a, int b) { return fScore[a] > fScore[b]; };
    std::priority_queue<int, std::vector<int>, decltype(compare)> openSet(compare);
    
    gScore[fromNodeId] = 0;
    fScore[fromNodeId] = heuristic(fromNodeId, toNodeId);
    openSet.push(fromNodeId);
    
    while (!openSet.empty()) {
        int current = openSet.top();
        openSet.pop();
        
        if (current == toNodeId) {
            // Reconstruct path
            while (current != -1) {
                path.push_back(current);
                current = cameFrom[current];
            }
            std::reverse(path.begin(), path.end());
            break;
        }
        
        if (closedSet.count(current)) continue;
        closedSet.insert(current);
        
        for (int neighbor : nodes[current].connectedNodes) {
            if (closedSet.count(neighbor)) continue;
            if (nodes[neighbor].isOccupied && neighbor != toNodeId) continue;
            
            double tentativeG = gScore[current] + adjacencyMatrix[current][neighbor];
            
            if (tentativeG < gScore[neighbor]) {
                cameFrom[neighbor] = current;
                gScore[neighbor] = tentativeG;
                fScore[neighbor] = gScore[neighbor] + heuristic(neighbor, toNodeId);
                openSet.push(neighbor);
            }
        }
    }
    
    pthread_mutex_unlock(&taxiwayMutex);
    return path;
}

std::vector<int> TaxiwayNetwork::findAlternatePath(int fromNodeId, int toNodeId,
                                                    const std::set<int>& blockedNodes) {
    pthread_mutex_lock(&taxiwayMutex);
    
    // Temporarily mark blocked nodes
    std::vector<bool> originalOccupied(nodes.size());
    for (int blocked : blockedNodes) {
        if (blocked >= 0 && blocked < static_cast<int>(nodes.size())) {
            originalOccupied[blocked] = nodes[blocked].isOccupied;
            nodes[blocked].isOccupied = true;
        }
    }
    
    pthread_mutex_unlock(&taxiwayMutex);
    
    std::vector<int> path = findPath(fromNodeId, toNodeId);
    
    pthread_mutex_lock(&taxiwayMutex);
    
    // Restore original state
    for (int blocked : blockedNodes) {
        if (blocked >= 0 && blocked < static_cast<int>(nodes.size())) {
            nodes[blocked].isOccupied = originalOccupied[blocked];
        }
    }
    
    pthread_mutex_unlock(&taxiwayMutex);
    return path;
}

bool TaxiwayNetwork::requestNode(int nodeId, int flightId) {
    pthread_mutex_lock(&taxiwayMutex);
    
    if (nodeId < 0 || nodeId >= static_cast<int>(nodes.size())) {
        pthread_mutex_unlock(&taxiwayMutex);
        return false;
    }
    
    if (nodes[nodeId].isOccupied) {
        pthread_mutex_unlock(&taxiwayMutex);
        return false;
    }
    
    nodes[nodeId].isOccupied = true;
    nodes[nodeId].occupyingFlightId = flightId;
    
    pthread_mutex_unlock(&taxiwayMutex);
    return true;
}

void TaxiwayNetwork::releaseNode(int nodeId) {
    pthread_mutex_lock(&taxiwayMutex);
    
    if (nodeId >= 0 && nodeId < static_cast<int>(nodes.size())) {
        nodes[nodeId].isOccupied = false;
        nodes[nodeId].occupyingFlightId = -1;
    }
    
    pthread_mutex_unlock(&taxiwayMutex);
}

bool TaxiwayNetwork::isNodeOccupied(int nodeId) {
    pthread_mutex_lock(&taxiwayMutex);
    bool occupied = false;
    if (nodeId >= 0 && nodeId < static_cast<int>(nodes.size())) {
        occupied = nodes[nodeId].isOccupied;
    }
    pthread_mutex_unlock(&taxiwayMutex);
    return occupied;
}

bool TaxiwayNetwork::detectGridlock() {
    pthread_mutex_lock(&taxiwayMutex);
    
    // Simple gridlock detection: check for cycles in occupied nodes
    // If all neighbors of an occupied node are also occupied, potential gridlock
    int stuckCount = 0;
    
    for (const auto& node : nodes) {
        if (!node.isOccupied) continue;
        
        bool allNeighborsBlocked = true;
        for (int neighbor : node.connectedNodes) {
            if (!nodes[neighbor].isOccupied) {
                allNeighborsBlocked = false;
                break;
            }
        }
        
        if (allNeighborsBlocked) stuckCount++;
    }
    
    pthread_mutex_unlock(&taxiwayMutex);
    return stuckCount >= 3; // Gridlock if 3+ aircraft can't move
}

void TaxiwayNetwork::resolveGridlock() {
    pthread_mutex_lock(&taxiwayMutex);
    
    // Simple resolution: find a node to clear
    for (auto& node : nodes) {
        if (node.isOccupied && node.isIntersection) {
            // Priority to clear intersections
            // In real system, would coordinate with ground control
            break;
        }
    }
    
    pthread_mutex_unlock(&taxiwayMutex);
}

bool TaxiwayNetwork::requestPushback(int gateId, int flightId) {
    int gateNode = getGateNode(gateId);
    if (gateNode == -1) return false;
    
    pthread_mutex_lock(&taxiwayMutex);
    
    // Check if pushback area is clear
    bool canPushback = true;
    for (int neighbor : nodes[gateNode].connectedNodes) {
        if (nodes[neighbor].isOccupied) {
            canPushback = false;
            break;
        }
    }
    
    if (canPushback) {
        // Reserve the first taxiway node for pushback
        if (!nodes[gateNode].connectedNodes.empty()) {
            int pushbackNode = nodes[gateNode].connectedNodes[0];
            nodes[pushbackNode].isOccupied = true;
            nodes[pushbackNode].occupyingFlightId = flightId;
        }
    }
    
    pthread_mutex_unlock(&taxiwayMutex);
    return canPushback;
}

void TaxiwayNetwork::completePushback(int gateId, int flightId) {
    int gateNode = getGateNode(gateId);
    if (gateNode == -1) return;
    
    releaseNode(gateNode);
}

std::vector<TaxiwayNode> TaxiwayNetwork::getAllNodes() {
    pthread_mutex_lock(&taxiwayMutex);
    std::vector<TaxiwayNode> result = nodes;
    pthread_mutex_unlock(&taxiwayMutex);
    return result;
}

TaxiwayNode* TaxiwayNetwork::getNode(int nodeId) {
    pthread_mutex_lock(&taxiwayMutex);
    TaxiwayNode* node = nullptr;
    if (nodeId >= 0 && nodeId < static_cast<int>(nodes.size())) {
        node = &nodes[nodeId];
    }
    pthread_mutex_unlock(&taxiwayMutex);
    return node;
}

int TaxiwayNetwork::getRunwayNode(int runwayId, bool departure) {
    return runwayId * 2 + (departure ? 1 : 0);
}

int TaxiwayNetwork::getGateNode(int gateId) {
    int gateStart = Config::NUM_RUNWAYS * 2 + 10;
    if (gateId >= 0 && gateId < Config::TOTAL_GATES) {
        return gateStart + gateId;
    }
    return -1;
}

double TaxiwayNetwork::heuristic(int fromNode, int toNode) {
    // Euclidean distance
    double dx = nodes[toNode].x - nodes[fromNode].x;
    double dy = nodes[toNode].y - nodes[fromNode].y;
    return std::sqrt(dx * dx + dy * dy);
}

void TaxiwayNetwork::buildAdjacencyMatrix() {
    int n = nodes.size();
    adjacencyMatrix.resize(n, std::vector<double>(n, std::numeric_limits<double>::max()));
    
    for (const auto& node : nodes) {
        adjacencyMatrix[node.id][node.id] = 0;
        
        for (int neighbor : node.connectedNodes) {
            double dist = heuristic(node.id, neighbor);
            adjacencyMatrix[node.id][neighbor] = dist;
            adjacencyMatrix[neighbor][node.id] = dist;
        }
    }
}

// ============================================================================
// FlightManager Implementation
// ============================================================================

FlightManager::FlightManager(DataManager* dm, Scheduler* sched)
    : dataManager(dm), scheduler(sched) {
    pthread_rwlock_init(&flightLock, nullptr);
}

FlightManager::~FlightManager() {
    pthread_rwlock_destroy(&flightLock);
}

int FlightManager::createFlight(const std::string& flightNumber, AircraftType type,
                                 FlightType flightType, TimeUnit scheduledArrival,
                                 TimeUnit scheduledDeparture) {
    int id = nextFlightId++;
    
    Flight flight;
    flight.id = id;
    flight.flightNumber = flightNumber;
    flight.aircraftType = type;
    flight.flightType = flightType;
    flight.status = FlightStatus::SCHEDULED;
    flight.scheduledArrival = scheduledArrival;
    flight.scheduledDeparture = scheduledDeparture;
    flight.assignedGate = -1;
    flight.assignedRunway = -1;
    flight.delayMinutes = 0;
    flight.isEmergency = false;
    flight.emergencyType = EmergencyType::NONE;
    flight.priorityOverride = -1;
    flight.turnaroundProgress = 0;
    
    // Get specs
    auto specs = getAircraftSpecs(type);
    flight.totalPassengers = specs.passengerCapacity;
    flight.fuelRequired = specs.fuelCapacityGallons / 2;
    
    pthread_rwlock_wrlock(&flightLock);
    flights[id] = flight;
    pthread_rwlock_unlock(&flightLock);
    
    dataManager->updateFlightSchedule(flight);
    
    return id;
}

void FlightManager::processArrival(int flightId) {
    pthread_rwlock_wrlock(&flightLock);
    auto it = flights.find(flightId);
    if (it != flights.end()) {
        Flight& flight = it->second;
        
        auto specs = getAircraftSpecs(flight.aircraftType);
        
        // Request runway
        if (runwayManager) {
            int runway = runwayManager->requestLanding(flightId, specs.wakeCategory);
            if (runway >= 0) {
                flight.assignedRunway = runway;
                flight.status = FlightStatus::LANDING;
            }
        }
        
        pthread_rwlock_unlock(&flightLock);
        dataManager->updateFlightSchedule(flight);
    } else {
        pthread_rwlock_unlock(&flightLock);
    }
}

void FlightManager::processDeparture(int flightId) {
    pthread_rwlock_wrlock(&flightLock);
    auto it = flights.find(flightId);
    if (it != flights.end()) {
        Flight& flight = it->second;
        
        auto specs = getAircraftSpecs(flight.aircraftType);
        
        // Request runway
        if (runwayManager) {
            int runway = runwayManager->requestTakeoff(flightId, specs.wakeCategory);
            if (runway >= 0) {
                flight.assignedRunway = runway;
                flight.status = FlightStatus::DEPARTING;
            }
        }
        
        pthread_rwlock_unlock(&flightLock);
        dataManager->updateFlightSchedule(flight);
    } else {
        pthread_rwlock_unlock(&flightLock);
    }
}

void FlightManager::updateFlightStatus(int flightId, FlightStatus newStatus) {
    pthread_rwlock_wrlock(&flightLock);
    auto it = flights.find(flightId);
    if (it != flights.end()) {
        it->second.status = newStatus;
        
        // Handle status transitions
        switch (newStatus) {
            case FlightStatus::DEPARTED:
                // Archive flight
                dataManager->archiveFlight(it->second);
                break;
            default:
                break;
        }
        
        dataManager->updateFlightSchedule(it->second);
    }
    pthread_rwlock_unlock(&flightLock);
}

void FlightManager::activateScheduledFlights(TimeUnit currentTime) {
    pthread_rwlock_wrlock(&flightLock);
    for (auto& pair : flights) {
        Flight& flight = pair.second;
        // Transition from SCHEDULED to APPROACHING when arrival time is reached
        if (flight.status == FlightStatus::SCHEDULED && currentTime >= flight.scheduledArrival) {
            flight.status = FlightStatus::APPROACHING;
            flight.actualArrival = currentTime;
        }
    }
    pthread_rwlock_unlock(&flightLock);
}

Flight* FlightManager::getFlight(int flightId) {
    pthread_rwlock_rdlock(&flightLock);
    Flight* flight = nullptr;
    auto it = flights.find(flightId);
    if (it != flights.end()) {
        flight = &it->second;
    }
    pthread_rwlock_unlock(&flightLock);
    return flight;
}

std::vector<Flight*> FlightManager::getFlightsByStatus(FlightStatus status) {
    std::vector<Flight*> result;
    pthread_rwlock_rdlock(&flightLock);
    for (auto& pair : flights) {
        if (pair.second.status == status) {
            result.push_back(&pair.second);
        }
    }
    pthread_rwlock_unlock(&flightLock);
    return result;
}

std::vector<Flight*> FlightManager::getAllActiveFlights() {
    std::vector<Flight*> result;
    pthread_rwlock_rdlock(&flightLock);
    for (auto& pair : flights) {
        if (pair.second.status != FlightStatus::DEPARTED &&
            pair.second.status != FlightStatus::CANCELLED &&
            pair.second.status != FlightStatus::SCHEDULED) {
            result.push_back(&pair.second);
        }
    }
    pthread_rwlock_unlock(&flightLock);
    return result;
}

void FlightManager::declareEmergency(int flightId, EmergencyType type) {
    pthread_rwlock_wrlock(&flightLock);
    auto it = flights.find(flightId);
    if (it != flights.end()) {
        it->second.isEmergency = true;
        it->second.emergencyType = type;
        it->second.priorityOverride = 0; // Highest priority
        
        dataManager->addEmergencyAlert("Emergency: Flight " + it->second.flightNumber, type);
    }
    pthread_rwlock_unlock(&flightLock);
}

void FlightManager::clearEmergency(int flightId) {
    pthread_rwlock_wrlock(&flightLock);
    auto it = flights.find(flightId);
    if (it != flights.end()) {
        it->second.isEmergency = false;
        it->second.emergencyType = EmergencyType::NONE;
        it->second.priorityOverride = -1;
    }
    pthread_rwlock_unlock(&flightLock);
}

void FlightManager::addDelay(int flightId, int delayMinutes, const std::string& reason) {
    pthread_rwlock_wrlock(&flightLock);
    auto it = flights.find(flightId);
    if (it != flights.end()) {
        it->second.delayMinutes += delayMinutes;
        it->second.delayReason = reason;
        it->second.estimatedDeparture = it->second.scheduledDeparture + (delayMinutes * 60);
    }
    pthread_rwlock_unlock(&flightLock);
}

int FlightManager::getTotalActiveFlights() {
    return getAllActiveFlights().size();
}

int FlightManager::getFlightsInStatus(FlightStatus status) {
    return getFlightsByStatus(status).size();
}

double FlightManager::getAverageDelay() {
    double totalDelay = 0;
    int count = 0;
    
    pthread_rwlock_rdlock(&flightLock);
    for (const auto& pair : flights) {
        if (pair.second.delayMinutes > 0) {
            totalDelay += pair.second.delayMinutes;
            count++;
        }
    }
    pthread_rwlock_unlock(&flightLock);
    
    return count > 0 ? totalDelay / count : 0;
}

int FlightManager::calculateFlightPriority(const Flight& flight) {
    if (flight.priorityOverride >= 0) return flight.priorityOverride;
    
    int priority = 50; // Base priority
    
    // Emergency
    if (flight.isEmergency) return 0;
    
    // Delays increase priority
    if (flight.delayMinutes > 120) priority -= 20;
    else if (flight.delayMinutes > 60) priority -= 10;
    else if (flight.delayMinutes > 30) priority -= 5;
    
    // International flights slightly higher priority
    if (flight.flightType == FlightType::INTERNATIONAL) priority -= 5;
    
    // Large aircraft get priority (more passengers affected)
    auto specs = getAircraftSpecs(flight.aircraftType);
    if (specs.passengerCapacity > 300) priority -= 10;
    
    return std::max(0, std::min(100, priority));
}

AircraftSpecs FlightManager::getAircraftSpecs(AircraftType type) {
    auto it = Config::AIRCRAFT_SPECS.find(type);
    if (it != Config::AIRCRAFT_SPECS.end()) {
        return it->second;
    }
    // Return default specs
    return {"UNK", "Unknown", WakeTurbulenceCategory::MEDIUM, 150, 15000, 5000, 6, 2, 1.0, 2, 60, false, false, false};
}
