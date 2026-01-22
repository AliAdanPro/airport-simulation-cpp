/**
 * Smart Airport Operations Management System
 * passenger.cpp - Passenger and Baggage Processing Implementation
 */

#include "passenger.h"
#include <algorithm>
#include <chrono>

// ============================================================================
// ProcessingLane Implementation
// ============================================================================

ProcessingLane::ProcessingLane(int id, int minTime, int maxTime)
    : laneId(id), minProcessingTime(minTime), maxProcessingTime(maxTime),
      rng(std::random_device{}()) {}

void ProcessingLane::enqueue(Passenger* passenger) {
    waitingQueue.push(passenger);
}

void ProcessingLane::process(TimeUnit currentTime) {
    // Check if current passenger is done
    if (currentPassenger && currentTime >= processingEndTime) {
        currentPassenger->status = static_cast<PassengerStatus>(
            static_cast<int>(currentPassenger->status) + 1);
        completedPassengers.push_back(currentPassenger);
        currentPassenger = nullptr;
    }
    
    // Start processing next passenger if lane is free
    if (!currentPassenger && !waitingQueue.empty()) {
        currentPassenger = waitingQueue.front();
        waitingQueue.pop();
        
        // Calculate processing time
        std::uniform_int_distribution<int> timeDist(minProcessingTime, maxProcessingTime);
        int processTime = timeDist(rng);
        
        // Apply VIP speedup
        if (currentPassenger->type == PassengerType::VIP) {
            processTime = static_cast<int>(processTime * Config::VIP_SPEED_MULTIPLIER);
        }
        
        processingEndTime = currentTime + processTime;
        currentPassenger->processingStartTime = currentTime;
    }
}

int ProcessingLane::getQueueLength() const {
    return waitingQueue.size() + (currentPassenger ? 1 : 0);
}

TimeUnit ProcessingLane::getEstimatedWait() const {
    int avgTime = (minProcessingTime + maxProcessingTime) / 2;
    return getQueueLength() * avgTime;
}

std::vector<Passenger*> ProcessingLane::getProcessedPassengers() {
    std::vector<Passenger*> result = completedPassengers;
    completedPassengers.clear();
    return result;
}

// ============================================================================
// PassengerProcessor Implementation
// ============================================================================

PassengerProcessor::PassengerProcessor(DataManager* dm)
    : dataManager(dm), rng(std::random_device{}()) {
    pthread_mutex_init(&passengerMutex, nullptr);
}

PassengerProcessor::~PassengerProcessor() {
    pthread_mutex_destroy(&passengerMutex);
}

void PassengerProcessor::initialize() {
    // Create check-in counters
    for (int i = 0; i < Config::NUM_CHECKIN_COUNTERS; i++) {
        checkinCounters.push_back(std::make_unique<ProcessingLane>(
            i, Config::CHECKIN_TIME_MIN, Config::CHECKIN_TIME_MAX));
    }
    
    // Create security lanes
    for (int i = 0; i < Config::NUM_SECURITY_LANES; i++) {
        securityLanes.push_back(std::make_unique<ProcessingLane>(
            i, Config::SECURITY_TIME_MIN, Config::SECURITY_TIME_MAX));
    }
    
    // Create customs counters
    for (int i = 0; i < 5; i++) {
        customsCounters.push_back(std::make_unique<ProcessingLane>(
            i, Config::CUSTOMS_TIME_MIN, Config::CUSTOMS_TIME_MAX));
    }
}

void PassengerProcessor::processArrivingPassengers(int flightId, 
                                                    const std::vector<Passenger>& passengers) {
    pthread_mutex_lock(&passengerMutex);
    
    for (const auto& pax : passengers) {
        allPassengers[pax.id] = pax;
        allPassengers[pax.id].status = PassengerStatus::ARRIVED;
        allPassengers[pax.id].arrivingFlightId = flightId;
        flightPassengers[flightId].push_back(pax.id);
    }
    
    pthread_mutex_unlock(&passengerMutex);
}

void PassengerProcessor::startDepartureProcessing(int flightId,
                                                   const std::vector<Passenger>& passengers) {
    pthread_mutex_lock(&passengerMutex);
    
    for (const auto& pax : passengers) {
        allPassengers[pax.id] = pax;
        allPassengers[pax.id].status = PassengerStatus::CHECK_IN_QUEUE;
        allPassengers[pax.id].departingFlightId = flightId;
        flightPassengers[flightId].push_back(pax.id);
        
        // Find shortest check-in queue
        int shortestQueue = 0;
        int minLength = checkinCounters[0]->getQueueLength();
        for (int i = 1; i < static_cast<int>(checkinCounters.size()); i++) {
            if (checkinCounters[i]->getQueueLength() < minLength) {
                minLength = checkinCounters[i]->getQueueLength();
                shortestQueue = i;
            }
        }
        
        checkinCounters[shortestQueue]->enqueue(&allPassengers[pax.id]);
    }
    
    pthread_mutex_unlock(&passengerMutex);
}

void PassengerProcessor::update(TimeUnit currentTime) {
    pthread_mutex_lock(&passengerMutex);
    
    // Process all check-in counters
    for (auto& counter : checkinCounters) {
        counter->process(currentTime);
        
        // Move completed passengers to security
        for (Passenger* pax : counter->getProcessedPassengers()) {
            pax->status = PassengerStatus::SECURITY_QUEUE;
            
            // Find shortest security lane
            int shortestLane = 0;
            int minLength = securityLanes[0]->getQueueLength();
            for (int i = 1; i < static_cast<int>(securityLanes.size()); i++) {
                if (securityLanes[i]->getQueueLength() < minLength) {
                    minLength = securityLanes[i]->getQueueLength();
                    shortestLane = i;
                }
            }
            
            securityLanes[shortestLane]->enqueue(pax);
            
            // Track processing time
            totalProcessingTime += (currentTime - pax->processingStartTime);
            processedCount++;
        }
    }
    
    // Process security lanes
    for (auto& lane : securityLanes) {
        lane->process(currentTime);
        
        // Move completed passengers appropriately
        for (Passenger* pax : lane->getProcessedPassengers()) {
            // Random secondary screening
            std::uniform_real_distribution<double> dist(0, 1);
            if (dist(rng) < Config::SECONDARY_SCREENING_PROBABILITY) {
                pax->status = PassengerStatus::SECONDARY_SCREENING;
                // Add delay for secondary screening
            } else {
                // Check if needs customs (international)
                // Simplified: assume all go to gate
                pax->status = PassengerStatus::IN_TERMINAL;
            }
        }
    }
    
    // Check for connection risks
    for (auto& pair : allPassengers) {
        if (pair.second.isConnecting && 
            pair.second.status == PassengerStatus::IN_TERMINAL) {
            if (isConnectionAtRisk(pair.second, currentTime)) {
                // Alert
                dataManager->addEmergencyAlert(
                    "Connection at risk: Passenger " + std::to_string(pair.first),
                    EmergencyType::NONE);
            }
        }
    }
    
    pthread_mutex_unlock(&passengerMutex);
}

std::vector<Passenger*> PassengerProcessor::getConnectionsAtRisk() {
    std::vector<Passenger*> atRisk;
    
    pthread_mutex_lock(&passengerMutex);
    
    TimeUnit currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    for (auto& pair : allPassengers) {
        if (pair.second.isConnecting && isConnectionAtRisk(pair.second, currentTime)) {
            atRisk.push_back(&pair.second);
        }
    }
    
    pthread_mutex_unlock(&passengerMutex);
    return atRisk;
}

void PassengerProcessor::rebookPassenger(int passengerId, int newFlightId) {
    pthread_mutex_lock(&passengerMutex);
    
    auto it = allPassengers.find(passengerId);
    if (it != allPassengers.end()) {
        int oldFlight = it->second.departingFlightId;
        it->second.departingFlightId = newFlightId;
        it->second.status = PassengerStatus::REBOOKED;
        
        // Update flight passenger lists
        auto& oldList = flightPassengers[oldFlight];
        oldList.erase(std::remove(oldList.begin(), oldList.end(), passengerId), oldList.end());
        flightPassengers[newFlightId].push_back(passengerId);
    }
    
    pthread_mutex_unlock(&passengerMutex);
}

void PassengerProcessor::assignVIPFastTrack(int passengerId) {
    pthread_mutex_lock(&passengerMutex);
    auto it = allPassengers.find(passengerId);
    if (it != allPassengers.end()) {
        it->second.type = PassengerType::VIP;
    }
    pthread_mutex_unlock(&passengerMutex);
}

void PassengerProcessor::assignDisabledAssistance(int passengerId) {
    pthread_mutex_lock(&passengerMutex);
    auto it = allPassengers.find(passengerId);
    if (it != allPassengers.end()) {
        it->second.type = PassengerType::DISABLED;
    }
    pthread_mutex_unlock(&passengerMutex);
}

void PassengerProcessor::assignMinorEscort(int passengerId, int escortId) {
    pthread_mutex_lock(&passengerMutex);
    auto it = allPassengers.find(passengerId);
    if (it != allPassengers.end()) {
        it->second.type = PassengerType::UNACCOMPANIED_MINOR;
    }
    pthread_mutex_unlock(&passengerMutex);
}

int PassengerProcessor::getCheckinQueueLength() const {
    int total = 0;
    for (const auto& counter : checkinCounters) {
        total += counter->getQueueLength();
    }
    return total;
}

int PassengerProcessor::getSecurityQueueLength() const {
    int total = 0;
    for (const auto& lane : securityLanes) {
        total += lane->getQueueLength();
    }
    return total;
}

int PassengerProcessor::getCustomsQueueLength() const {
    int total = 0;
    for (const auto& counter : customsCounters) {
        total += counter->getQueueLength();
    }
    return total;
}

double PassengerProcessor::getAverageProcessingTime() const {
    return processedCount > 0 ? totalProcessingTime / processedCount : 0;
}

TimeUnit PassengerProcessor::calculateMinConnectionTime(int fromGate, int toGate) {
    // Simplified: assume 5 min per 100m
    double distance = 300; // Default 300m
    return static_cast<TimeUnit>(distance / 100 * 300); // 5 min per 100m
}

bool PassengerProcessor::isConnectionAtRisk(const Passenger& passenger, TimeUnit currentTime) {
    TimeUnit timeToConnection = passenger.connectionDeadline - currentTime;
    TimeUnit requiredTime = Config::MIN_CONNECTION_TIME;
    
    // Add processing time if still in queue
    if (passenger.status == PassengerStatus::SECURITY_QUEUE) {
        requiredTime += Config::SECURITY_TIME_MAX;
    }
    
    return timeToConnection < requiredTime;
}

// ============================================================================
// BaggageHandler Implementation
// ============================================================================

BaggageHandler::BaggageHandler(DataManager* dm)
    : dataManager(dm), rng(std::random_device{}()) {
    pthread_mutex_init(&baggageMutex, nullptr);
}

BaggageHandler::~BaggageHandler() {
    pthread_mutex_destroy(&baggageMutex);
}

void BaggageHandler::initialize() {
    // Create sorting network nodes
    for (int i = 0; i < 10; i++) {
        BaggageSortingNode node;
        node.nodeId = i;
        node.name = "SORT" + std::to_string(i);
        
        // Connect to adjacent nodes
        if (i > 0) node.connectedNodes.push_back(i - 1);
        if (i < 9) node.connectedNodes.push_back(i + 1);
        
        sortingNodes.push_back(node);
    }
}

void BaggageHandler::processArrivingBags(int flightId, const std::vector<Baggage>& bags) {
    pthread_mutex_lock(&baggageMutex);
    
    for (const auto& bag : bags) {
        allBags[bag.id] = bag;
        allBags[bag.id].status = BaggageStatus::IN_SORTING;
        flightBags[flightId].push_back(bag.id);
        
        // Check for transfer
        if (bag.destinationFlight > 0 && bag.destinationFlight != flightId) {
            allBags[bag.id].status = BaggageStatus::TRANSFER;
            // Mark as priority if tight connection
            if (bag.isPriority) {
                allBags[bag.id].isPriority = true;
            }
        }
        
        // Simulate misrouting
        simulateMisrouting(allBags[bag.id]);
        
        totalProcessed++;
    }
    
    pthread_mutex_unlock(&baggageMutex);
}

void BaggageHandler::startLoadingBags(int flightId) {
    pthread_mutex_lock(&baggageMutex);
    
    auto it = flightBags.find(flightId);
    if (it != flightBags.end()) {
        for (int bagId : it->second) {
            auto bagIt = allBags.find(bagId);
            if (bagIt != allBags.end()) {
                bagIt->second.status = BaggageStatus::BEING_LOADED;
            }
        }
    }
    
    pthread_mutex_unlock(&baggageMutex);
}

void BaggageHandler::update(TimeUnit currentTime) {
    pthread_mutex_lock(&baggageMutex);
    
    // Process bags through sorting nodes
    for (auto& node : sortingNodes) {
        while (!node.inputBuffer.empty()) {
            Baggage* bag = node.inputBuffer.front();
            node.inputBuffer.pop();
            
            // Determine destination node
            int destNode = determineSortingNode(bag->flightId);
            
            if (destNode == node.nodeId) {
                // Arrived at destination
                bag->status = BaggageStatus::BEING_LOADED;
            } else {
                // Route to next node
                if (!node.connectedNodes.empty()) {
                    int nextNode = node.connectedNodes[0];
                    if (nextNode < static_cast<int>(sortingNodes.size())) {
                        sortingNodes[nextNode].inputBuffer.push(bag);
                    }
                }
            }
            
            bag->currentLocation = "Node " + std::to_string(node.nodeId);
        }
    }
    
    pthread_mutex_unlock(&baggageMutex);
}

std::vector<Baggage*> BaggageHandler::getMisroutedBags() {
    std::vector<Baggage*> misrouted;
    
    pthread_mutex_lock(&baggageMutex);
    for (auto& pair : allBags) {
        if (pair.second.status == BaggageStatus::MISROUTED) {
            misrouted.push_back(&pair.second);
        }
    }
    pthread_mutex_unlock(&baggageMutex);
    
    return misrouted;
}

void BaggageHandler::rerouteBag(int bagId, int correctFlightId) {
    pthread_mutex_lock(&baggageMutex);
    
    auto it = allBags.find(bagId);
    if (it != allBags.end()) {
        it->second.flightId = correctFlightId;
        it->second.status = BaggageStatus::IN_SORTING;
    }
    
    pthread_mutex_unlock(&baggageMutex);
}

void BaggageHandler::markPriority(int bagId) {
    pthread_mutex_lock(&baggageMutex);
    
    auto it = allBags.find(bagId);
    if (it != allBags.end()) {
        it->second.isPriority = true;
        it->second.status = BaggageStatus::PRIORITY;
    }
    
    pthread_mutex_unlock(&baggageMutex);
}

std::string BaggageHandler::getBagLocation(int bagId) {
    pthread_mutex_lock(&baggageMutex);
    
    std::string location = "Unknown";
    auto it = allBags.find(bagId);
    if (it != allBags.end()) {
        location = it->second.currentLocation;
    }
    
    pthread_mutex_unlock(&baggageMutex);
    return location;
}

int BaggageHandler::getTotalBagsProcessed() const {
    return totalProcessed;
}

double BaggageHandler::getMisrouteRate() const {
    return totalProcessed > 0 ? 
           static_cast<double>(misroutedCount) / totalProcessed : 0;
}

int BaggageHandler::determineSortingNode(int destinationFlight) {
    // Simple hash-based routing
    return destinationFlight % sortingNodes.size();
}

void BaggageHandler::simulateMisrouting(Baggage& bag) {
    std::uniform_real_distribution<double> dist(0, 1);
    if (dist(rng) < Config::BAGGAGE_MISROUTE_PROBABILITY) {
        bag.status = BaggageStatus::MISROUTED;
        misroutedCount++;
    }
}
