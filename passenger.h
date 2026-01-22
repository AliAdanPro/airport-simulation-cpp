/**
 * Smart Airport Operations Management System
 * passenger.h - Passenger and Baggage Processing
 * 
 * Implements multi-stage passenger pipeline and baggage handling:
 * - Check-in, security, customs, boarding
 * - Connecting passenger tracking
 * - VIP, disabled, and unaccompanied minor handling
 * - Automated baggage sorting with RFID tracking
 */

#ifndef PASSENGER_H
#define PASSENGER_H

#include "types.h"
#include "config.h"
#include "data_manager.h"
#include <vector>
#include <queue>
#include <map>
#include <random>

/**
 * Processing Lane
 * Generic queue-based processing lane (check-in, security, etc.)
 */
class ProcessingLane {
public:
    ProcessingLane(int id, int minTime, int maxTime);
    
    // Add passenger to queue
    void enqueue(Passenger* passenger);
    
    // Process passengers (called each update)
    void process(TimeUnit currentTime);
    
    // Get current queue length
    int getQueueLength() const;
    
    // Get processing time estimate
    TimeUnit getEstimatedWait() const;
    
    // Get processed passengers
    std::vector<Passenger*> getProcessedPassengers();
    
private:
    int laneId;
    int minProcessingTime;
    int maxProcessingTime;
    std::queue<Passenger*> waitingQueue;
    Passenger* currentPassenger = nullptr;
    TimeUnit processingEndTime = 0;
    std::vector<Passenger*> completedPassengers;
    std::mt19937 rng;
};

/**
 * Baggage Sorting Node
 * Node in the conveyor network
 */
struct BaggageSortingNode {
    int nodeId;
    std::string name;
    std::vector<int> connectedNodes;
    std::queue<Baggage*> inputBuffer;
    std::map<int, std::queue<Baggage*>> outputBuffers; // destination -> queue
};

/**
 * Passenger Processing System
 */
class PassengerProcessor {
public:
    PassengerProcessor(DataManager* dm);
    ~PassengerProcessor();
    
    void initialize();
    
    // Process passengers from arriving flight
    void processArrivingPassengers(int flightId, const std::vector<Passenger>& passengers);
    
    // Start departing passenger processing
    void startDepartureProcessing(int flightId, const std::vector<Passenger>& passengers);
    
    // Update all processing (call each tick)
    void update(TimeUnit currentTime);
    
    // Check connections at risk
    std::vector<Passenger*> getConnectionsAtRisk();
    
    // Rebook passenger
    void rebookPassenger(int passengerId, int newFlightId);
    
    // Special handling
    void assignVIPFastTrack(int passengerId);
    void assignDisabledAssistance(int passengerId);
    void assignMinorEscort(int passengerId, int escortId);
    
    // Get statistics
    int getCheckinQueueLength() const;
    int getSecurityQueueLength() const;
    int getCustomsQueueLength() const;
    double getAverageProcessingTime() const;
    
private:
    DataManager* dataManager;
    
    // Check-in counters
    std::vector<std::unique_ptr<ProcessingLane>> checkinCounters;
    
    // Security lanes
    std::vector<std::unique_ptr<ProcessingLane>> securityLanes;
    
    // Customs counters (international only)
    std::vector<std::unique_ptr<ProcessingLane>> customsCounters;
    
    // Passengers in various stages
    std::map<int, Passenger> allPassengers;
    std::vector<Passenger*> atGate;
    std::map<int, std::vector<int>> flightPassengers; // flightId -> passengerIds
    
    // Statistics
    double totalProcessingTime = 0;
    int processedCount = 0;
    
    pthread_mutex_t passengerMutex;
    std::mt19937 rng;
    
    // Helper functions
    TimeUnit calculateMinConnectionTime(int fromGate, int toGate);
    bool isConnectionAtRisk(const Passenger& passenger, TimeUnit currentTime);
};

/**
 * Baggage Handling System
 */
class BaggageHandler {
public:
    BaggageHandler(DataManager* dm);
    ~BaggageHandler();
    
    void initialize();
    
    // Process bags from arriving flight
    void processArrivingBags(int flightId, const std::vector<Baggage>& bags);
    
    // Start loading bags for departing flight
    void startLoadingBags(int flightId);
    
    // Update conveyor network
    void update(TimeUnit currentTime);
    
    // Handle misrouted bags
    std::vector<Baggage*> getMisroutedBags();
    void rerouteBag(int bagId, int correctFlightId);
    
    // Priority handling
    void markPriority(int bagId);
    
    // RFID tracking
    std::string getBagLocation(int bagId);
    
    // Statistics
    int getTotalBagsProcessed() const;
    double getMisrouteRate() const;
    
private:
    DataManager* dataManager;
    
    // Sorting network
    std::vector<BaggageSortingNode> sortingNodes;
    
    // All bags
    std::map<int, Baggage> allBags;
    
    // Flight assignments
    std::map<int, std::vector<int>> flightBags; // flightId -> bagIds
    
    // Statistics
    int totalProcessed = 0;
    int misroutedCount = 0;
    
    pthread_mutex_t baggageMutex;
    std::mt19937 rng;
    
    // Routing
    int determineSortingNode(int destinationFlight);
    void simulateMisrouting(Baggage& bag);
};

#endif // PASSENGER_H
