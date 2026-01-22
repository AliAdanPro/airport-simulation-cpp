/**
 * Smart Airport Operations Management System
 * simulation.h - Main Simulation Engine
 * 
 * Central coordination of all airport operations:
 * - Real-time simulation loop (1 second = 1 second)
 * - Event-driven updates
 * - Component coordination
 * - Statistics collection
 */

#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"
#include "config.h"
#include "data_manager.h"
#include "scheduler.h"
#include "memory_manager.h"
#include "aircraft.h"
#include "ground_services.h"
#include "passenger.h"
#include "crew.h"
#include "crisis_manager.h"
#include <memory>
#include <atomic>

// Forward declaration
class Display;

/**
 * Financial Tracker
 * Tracks all costs and revenues
 */
class FinancialTracker {
public:
    FinancialTracker();
    
    // Add costs
    void addFuelCost(double amount);
    void addCrewCost(double amount);
    void addMaintenanceCost(double amount);
    void addGroundServicesCost(double amount);
    void addEmergencyCost(double amount);
    void addDelayCost(double amount);
    void addMiscCost(double amount);
    
    // Add revenues
    void addLandingFee(double amount);
    void addGateFee(double amount);
    void addPassengerFee(double amount);
    void addCargoFee(double amount);
    void addRetailRevenue(double amount);
    void addParkingRevenue(double amount);
    
    // Get totals
    double getTotalCosts() const;
    double getTotalRevenue() const;
    double getProfit() const;
    
    // Get breakdown
    std::map<std::string, double> getCostBreakdown() const;
    std::map<std::string, double> getRevenueBreakdown() const;
    
    // Check budget
    bool isWithinBudget() const;
    double getBudgetRemaining() const;
    
private:
    std::map<std::string, double> costs;
    std::map<std::string, double> revenues;
    double budget = Config::DAILY_BUDGET;
    mutable pthread_mutex_t mutex;
};

/**
 * Simulation Statistics
 */
struct SimulationStats {
    // Flight stats
    int totalFlightsProcessed = 0;
    int currentActiveFlights = 0;
    int delayedFlights = 0;
    int cancelledFlights = 0;
    double averageDelay = 0.0;
    
    // On-time performance
    double onTimeArrivalRate = 0.0;
    double onTimeDepartureRate = 0.0;
    
    // Resource utilization
    double runwayUtilization = 0.0;
    double gateUtilization = 0.0;
    double resourceUtilization = 0.0;
    
    // Passenger stats
    int totalPassengersProcessed = 0;
    int missedConnections = 0;
    double averageProcessingTime = 0.0;
    
    // Baggage stats
    int bagsProcessed = 0;
    int bagsMisrouted = 0;
    
    // Crisis stats
    int emergenciesHandled = 0;
    int weatherEvents = 0;
    
    // System performance
    double schedulerEfficiency = 0.0;
    double memoryUtilization = 0.0;
    double cpuLoad = 0.0;
};

/**
 * Main Simulation Engine
 */
class Simulation {
public:
    Simulation();
    ~Simulation();
    
    // Initialize all components
    bool initialize();
    
    // Run simulation
    void start();
    void stop();
    void pause();
    void resume();
    
    // Simulation control
    void setSpeed(double speedMultiplier);
    double getSpeed() const;
    TimeUnit getSimulationTime() const;
    bool isRunning() const { return running.load(); }
    
    // Get statistics
    SimulationStats getStats() const;
    
    // Get component references
    DataManager* getDataManager() { return dataManager.get(); }
    Scheduler* getScheduler() { return scheduler.get(); }
    FlightManager* getFlightManager() { return flightManager.get(); }
    FinancialTracker* getFinancialTracker() { return financialTracker.get(); }
    RunwayManager* getRunwayManager() { return runwayManager.get(); }
    GateManager* getGateManager() { return gateManager.get(); }
    MemoryManager* getMemoryManager() { return memoryManager.get(); }
    
    // Manual flight creation
    int createFlight(const std::string& flightNumber, AircraftType type,
                     FlightType flightType, TimeUnit arrivalOffset,
                     TimeUnit departureOffset);
    
    // Trigger events
    void triggerWeatherEvent(WeatherType type, TimeUnit duration);
    void triggerEmergency(int flightId, EmergencyType type);
    
    // Set display
    void setDisplay(Display* disp) { display = disp; }
    
private:
    // Core components
    std::unique_ptr<DataManager> dataManager;
    std::unique_ptr<Scheduler> scheduler;
    std::unique_ptr<MemoryManager> memoryManager;
    
    // Operational components
    std::unique_ptr<RunwayManager> runwayManager;
    std::unique_ptr<GateManager> gateManager;
    std::unique_ptr<TaxiwayNetwork> taxiwayNetwork;
    std::unique_ptr<FlightManager> flightManager;
    std::unique_ptr<GroundServiceManager> groundServices;
    std::unique_ptr<PassengerProcessor> passengerProcessor;
    std::unique_ptr<BaggageHandler> baggageHandler;
    std::unique_ptr<CrewManager> crewManager;
    std::unique_ptr<CrisisManager> crisisManager;
    
    // Financial
    std::unique_ptr<FinancialTracker> financialTracker;
    
    // Display
    Display* display = nullptr;
    
    // Simulation state
    std::atomic<bool> running{false};
    std::atomic<bool> paused{false};
    double speedMultiplier = 1.0;
    TimeUnit simulationTime = 0;
    TimeUnit startRealTime = 0;
    
    // Main loop thread
    pthread_t simulationThread;
    static void* simulationLoop(void* arg);
    void runSimulation();
    
    // Update functions
    void updateFlights(TimeUnit elapsed);
    void updateGroundServices(TimeUnit elapsed);
    void updatePassengers(TimeUnit elapsed);
    void updateCrew(TimeUnit elapsed);
    void updateCrises(TimeUnit elapsed);
    void updateStatistics();
    void updateMemory(TimeUnit elapsed);  // AWSC-PPC integration
    
    // Flight generation
    void generateScheduledFlights();
    void generateRandomFlight();
    int nextFlightNumber = 1;
    
    // Statistics
    SimulationStats stats;
    pthread_mutex_t statsMutex;
};

#endif // SIMULATION_H
