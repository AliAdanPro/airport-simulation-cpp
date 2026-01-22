/**
 * Smart Airport Operations Management System
 * simulation.cpp - Main Simulation Engine Implementation
 */

#include "simulation.h"
#include <chrono>
#include <thread>
#include <random>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <set>

// ============================================================================
// FinancialTracker Implementation
// ============================================================================

FinancialTracker::FinancialTracker() {
    pthread_mutex_init(&mutex, nullptr);
    
    costs["fuel"] = 0;
    costs["crew"] = 0;
    costs["maintenance"] = 0;
    costs["ground_services"] = 0;
    costs["emergency"] = 0;
    costs["delay"] = 0;
    costs["misc"] = 0;
    
    revenues["landing_fees"] = 0;
    revenues["gate_fees"] = 0;
    revenues["passenger_fees"] = 0;
    revenues["cargo_fees"] = 0;
    revenues["retail"] = 0;
    revenues["parking"] = 0;
}

void FinancialTracker::addFuelCost(double amount) {
    pthread_mutex_lock(&mutex);
    costs["fuel"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addCrewCost(double amount) {
    pthread_mutex_lock(&mutex);
    costs["crew"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addMaintenanceCost(double amount) {
    pthread_mutex_lock(&mutex);
    costs["maintenance"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addGroundServicesCost(double amount) {
    pthread_mutex_lock(&mutex);
    costs["ground_services"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addEmergencyCost(double amount) {
    pthread_mutex_lock(&mutex);
    costs["emergency"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addDelayCost(double amount) {
    pthread_mutex_lock(&mutex);
    costs["delay"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addMiscCost(double amount) {
    pthread_mutex_lock(&mutex);
    costs["misc"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addLandingFee(double amount) {
    pthread_mutex_lock(&mutex);
    revenues["landing_fees"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addGateFee(double amount) {
    pthread_mutex_lock(&mutex);
    revenues["gate_fees"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addPassengerFee(double amount) {
    pthread_mutex_lock(&mutex);
    revenues["passenger_fees"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addCargoFee(double amount) {
    pthread_mutex_lock(&mutex);
    revenues["cargo_fees"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addRetailRevenue(double amount) {
    pthread_mutex_lock(&mutex);
    revenues["retail"] += amount;
    pthread_mutex_unlock(&mutex);
}

void FinancialTracker::addParkingRevenue(double amount) {
    pthread_mutex_lock(&mutex);
    revenues["parking"] += amount;
    pthread_mutex_unlock(&mutex);
}

double FinancialTracker::getTotalCosts() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mutex));
    double total = 0;
    for (const auto& pair : costs) {
        total += pair.second;
    }
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mutex));
    return total;
}

double FinancialTracker::getTotalRevenue() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mutex));
    double total = 0;
    for (const auto& pair : revenues) {
        total += pair.second;
    }
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mutex));
    return total;
}

double FinancialTracker::getProfit() const {
    return getTotalRevenue() - getTotalCosts();
}

std::map<std::string, double> FinancialTracker::getCostBreakdown() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mutex));
    auto result = costs;
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mutex));
    return result;
}

std::map<std::string, double> FinancialTracker::getRevenueBreakdown() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mutex));
    auto result = revenues;
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mutex));
    return result;
}

bool FinancialTracker::isWithinBudget() const {
    return getTotalCosts() <= budget;
}

double FinancialTracker::getBudgetRemaining() const {
    return budget - getTotalCosts();
}

// ============================================================================
// Simulation Implementation
// ============================================================================

Simulation::Simulation() {
    pthread_mutex_init(&statsMutex, nullptr);
}

Simulation::~Simulation() {
    stop();
    pthread_mutex_destroy(&statsMutex);
}

bool Simulation::initialize() {
    // Create core components
    dataManager = std::make_unique<DataManager>();
    scheduler = std::make_unique<Scheduler>();
    memoryManager = std::make_unique<MemoryManager>();
    
    // Create operational components
    runwayManager = std::make_unique<RunwayManager>(dataManager.get());
    gateManager = std::make_unique<GateManager>(dataManager.get());
    taxiwayNetwork = std::make_unique<TaxiwayNetwork>(dataManager.get());
    flightManager = std::make_unique<FlightManager>(dataManager.get(), scheduler.get());
    groundServices = std::make_unique<GroundServiceManager>(dataManager.get(), scheduler.get());
    passengerProcessor = std::make_unique<PassengerProcessor>(dataManager.get());
    baggageHandler = std::make_unique<BaggageHandler>(dataManager.get());
    crewManager = std::make_unique<CrewManager>(dataManager.get());
    crisisManager = std::make_unique<CrisisManager>(dataManager.get(), scheduler.get());
    
    // Create financial tracker
    financialTracker = std::make_unique<FinancialTracker>();
    
    // Wire up dependencies
    flightManager->setRunwayManager(runwayManager.get());
    flightManager->setGateManager(gateManager.get());
    flightManager->setTaxiwayNetwork(taxiwayNetwork.get());
    
    crisisManager->setRunwayManager(runwayManager.get());
    crisisManager->setGateManager(gateManager.get());
    crisisManager->setFlightManager(flightManager.get());
    
    // Initialize all components
    runwayManager->initialize(Config::NUM_RUNWAYS);
    gateManager->initialize();
    taxiwayNetwork->initialize();
    groundServices->initialize();
    passengerProcessor->initialize();
    baggageHandler->initialize();
    crewManager->initialize();
    crisisManager->initialize();
    
    return true;
}

void Simulation::start() {
    if (running) return;
    
    running = true;
    
    // Start component threads
    scheduler->start();
    memoryManager->start();
    
    // Record start time
    startRealTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    simulationTime = 0;
    
    // Generate initial flights
    generateScheduledFlights();
    
    // Start simulation thread
    pthread_create(&simulationThread, nullptr, simulationLoop, this);
}

void Simulation::stop() {
    if (!running.exchange(false)) return;  // Already stopped
    
    // Wait for simulation thread to finish
    pthread_join(simulationThread, nullptr);
    
    // Safely stop other threads with null checks
    if (scheduler) {
        scheduler->stop();
    }
    if (memoryManager) {
        memoryManager->stop();
    }
}


void Simulation::setSpeed(double mult) {
    speedMultiplier = mult;
}

double Simulation::getSpeed() const {
    return speedMultiplier;
}

void Simulation::pause() {
    paused = true;
}

void Simulation::resume() {
    paused = false;
}

TimeUnit Simulation::getSimulationTime() const {
    return simulationTime;
}

SimulationStats Simulation::getStats() const {
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&statsMutex));
    SimulationStats s = stats;
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&statsMutex));
    return s;
}

int Simulation::createFlight(const std::string& flightNumber, AircraftType type,
                              FlightType flightType, TimeUnit arrivalOffset,
                              TimeUnit departureOffset) {
    TimeUnit arrival = simulationTime + arrivalOffset;
    TimeUnit departure = simulationTime + departureOffset;
    
    int flightId = flightManager->createFlight(flightNumber, type, flightType,
                                                arrival, departure);
    
    return flightId;
}

void Simulation::triggerWeatherEvent(WeatherType type, TimeUnit duration) {
    crisisManager->triggerWeatherEvent(type, duration);
}

void Simulation::triggerEmergency(int flightId, EmergencyType type) {
    crisisManager->declareEmergency(flightId, type, "Manual emergency trigger");
}

void* Simulation::simulationLoop(void* arg) {
    Simulation* sim = static_cast<Simulation*>(arg);
    sim->runSimulation();
    return nullptr;
}

void Simulation::runSimulation() {
    TimeUnit lastUpdate = simulationTime;
    auto lastRealTime = std::chrono::steady_clock::now();
    double timeAccumulator = 0.0;  // Accumulator for fractional time
    
    while (running) {
        if (paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            lastRealTime = std::chrono::steady_clock::now();
            continue;
        }
        
        // Calculate elapsed real time
        auto now = std::chrono::steady_clock::now();
        auto realElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastRealTime).count();
        lastRealTime = now;
        
        // Accumulate simulation time based on speed multiplier
        // At 1x: 1 real second = 1 simulation second
        // At 10x: 1 real second = 10 simulation seconds
        timeAccumulator += (realElapsed / 1000.0) * speedMultiplier;
        
        // Only advance simulation when we have at least 1 second accumulated
        if (timeAccumulator >= 1.0) {
            TimeUnit simElapsed = static_cast<TimeUnit>(timeAccumulator);
            timeAccumulator -= simElapsed;  // Keep the fractional part
            
            simulationTime += simElapsed;
            
            // Update all components
            updateFlights(simElapsed);
            updateGroundServices(simElapsed);
            updatePassengers(simElapsed);
            updateCrew(simElapsed);
            updateCrises(simElapsed);
            updateMemory(simElapsed);  // AWSC-PPC page access simulation
            
            // Update statistics every tick for responsive display
            updateStatistics();
            
            // Periodic events (every 60 simulation seconds)
            static TimeUnit lastPeriodicUpdate = 0;
            if (simulationTime - lastPeriodicUpdate >= 60) {
                lastPeriodicUpdate = simulationTime;
                
                // Random events
                crisisManager->simulateRandomEvents();
                groundServices->simulateRandomFailures();
                
                // Generate new flights only if we have less than 8 active
                auto currentActive = flightManager->getAllActiveFlights();
                if (currentActive.size() < 8) {
                    generateRandomFlight();
                }
            }
            
            // Auto-exit conditions:
            // 1. Time limit reached (configurable - currently 300 seconds)
            // 2. All flights have departed and none scheduled
            auto activeFlights = flightManager->getAllActiveFlights();
            auto scheduledFlights = dataManager->getUpcomingFlights(0);
            
            if (simulationTime >= 300) {
                // Time limit reached - exit gracefully
                running = false;
            } else if (simulationTime > 300 && activeFlights.empty() && scheduledFlights.empty()) {
                // All flights complete
                running = false;
            }
        }
        
        // Sleep to maintain ~60 updates per second
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void Simulation::updateFlights(TimeUnit elapsed) {
    // CRITICAL: Activate scheduled flights that have reached their arrival time
    flightManager->activateScheduledFlights(simulationTime);
    
    // Get all active flights
    std::vector<Flight*> activeFlights = flightManager->getAllActiveFlights();
    
    // Submit operations to scheduler to create contention
    static std::hash<std::string> hasher;
    static std::set<std::pair<int, FlightStatus>> pendingOps;  // Track (flightId, status) with pending ops
    
    for (Flight* flight : activeFlights) {
        auto opKey = std::make_pair(flight->id, flight->status);
        
        switch (flight->status) {
            case FlightStatus::APPROACHING: {
                // Only submit if not already pending
                if (pendingOps.find(opKey) != pendingOps.end()) {
                    flightManager->processArrival(flight->id);
                    break;
                }
                pendingOps.insert(opKey);
                
                // Submit landing request to scheduler with varying priorities
                int priority = 10 + (hasher(flight->flightNumber) % 20); // Priority 10-29
                
                Operation landingOp;
                landingOp.type = OperationType::RUNWAY_LANDING;
                landingOp.priority = priority;
                landingOp.flightId = flight->id;
                landingOp.resourceId = -1;
                landingOp.estimatedDuration = 10;
                landingOp.execute = [this, flight]() {
                    flightManager->processArrival(flight->id);
                };
                landingOp.isComplete = [flight]() {
                    return flight->status != FlightStatus::APPROACHING;
                };
                
                scheduler->submitOperation(landingOp);
                flightManager->processArrival(flight->id);
                break;
            }
                
            case FlightStatus::LANDING:
                // Simulate landing time (10 seconds - more visible)
                flight->turnaroundProgress += elapsed;
                if (flight->turnaroundProgress >= 10) {
                    if (flight->assignedRunway >= 0) {
                        runwayManager->releaseLanding(flight->assignedRunway);
                    }
                    flightManager->updateFlightStatus(flight->id, FlightStatus::LANDED);
                    flight->turnaroundProgress = 0;
                }
                break;
                
            case FlightStatus::LANDED:
                // Assign gate - use the gate ID returned by assignGate
                if (flight->assignedGate < 0) {
                    int gate = gateManager->assignGate(
                        flight->id, flight->aircraftType, flight->flightType);
                    if (gate >= 0) {
                        flight->assignedGate = gate;
                        flightManager->updateFlightStatus(flight->id, FlightStatus::TAXIING_IN);
                    }
                    // Note: if no gate available, flight stays in LANDED state
                }
                break;
                
            case FlightStatus::TAXIING_IN:
                // Simulate taxiing time (15 seconds - more visible)
                flight->turnaroundProgress += elapsed;
                if (flight->turnaroundProgress >= 15) {
                    flightManager->updateFlightStatus(flight->id, FlightStatus::AT_GATE);
                    flight->turnaroundProgress = 0;
                    
                    // Start turnaround
                    groundServices->startTurnaround(
                        flight->id, flight->aircraftType, flight->flightType,
                        crisisManager->isDeicingRequired());
                }
                break;
                
            case FlightStatus::AT_GATE:
                // Simulate ground service turnaround (30 seconds - main work time)
                flight->turnaroundProgress += elapsed;
                if (flight->turnaroundProgress >= 30) {
                    flightManager->updateFlightStatus(flight->id, FlightStatus::BOARDING);
                    flight->turnaroundProgress = 0;
                }
                break;
                
            case FlightStatus::BOARDING:
                // Simulate boarding time (20 seconds - passengers need time)
                flight->turnaroundProgress += elapsed;
                if (flight->turnaroundProgress >= 20) {
                    flightManager->updateFlightStatus(flight->id, FlightStatus::PUSHBACK);
                    flight->turnaroundProgress = 0;
                }
                break;
                
            case FlightStatus::PUSHBACK:
                // Simulate pushback time (10 seconds)
                flight->turnaroundProgress += elapsed;
                if (flight->turnaroundProgress >= 10) {
                    flightManager->updateFlightStatus(flight->id, FlightStatus::TAXIING_OUT);
                    flight->turnaroundProgress = 0;
                }
                break;
                
            case FlightStatus::TAXIING_OUT:
                // Simulate taxiing to runway (15 seconds)
                flight->turnaroundProgress += elapsed;
                if (flight->turnaroundProgress >= 15) {
                    flightManager->updateFlightStatus(flight->id, FlightStatus::WAITING_TAKEOFF);
                    flight->turnaroundProgress = 0;
                }
                break;
                
            case FlightStatus::WAITING_TAKEOFF: {
                // Only submit if not already pending
                if (pendingOps.find(opKey) != pendingOps.end()) {
                    flightManager->processDeparture(flight->id);
                    break;
                }
                pendingOps.insert(opKey);
                
                // Submit departure request to scheduler (lower priority than landing)
                int priority = 30 + (hasher(flight->flightNumber) % 25);  // Priority 30-54 (lower than landing 10-29)
                
                Operation takeoffOp;
                takeoffOp.type = OperationType::RUNWAY_TAKEOFF;
                takeoffOp.priority = priority;
                takeoffOp.flightId = flight->id;
                takeoffOp.resourceId = flight->assignedRunway;
                takeoffOp.estimatedDuration = 10;
                takeoffOp.execute = [this, flight]() {
                    flightManager->processDeparture(flight->id);
                };
                takeoffOp.isComplete = [flight]() {
                    return flight->status != FlightStatus::WAITING_TAKEOFF;
                };
                
                scheduler->submitOperation(takeoffOp);
                // Request departure (runway assignment)
                flightManager->processDeparture(flight->id);
                break;
            }
                
            case FlightStatus::DEPARTING:
                // Simulate departure (10 seconds)
                flight->turnaroundProgress += elapsed;
                if (flight->turnaroundProgress >= 10) {
                    if (flight->assignedRunway >= 0) {
                        runwayManager->releaseTakeoff(flight->assignedRunway);
                    }
                    if (flight->assignedGate >= 0) {
                        gateManager->releaseGate(flight->assignedGate);
                    }
                    flightManager->updateFlightStatus(flight->id, FlightStatus::DEPARTED);
                    crewManager->releaseFlightCrew(flight->id);
                    
                    // CRITICAL: Unregister process from memory manager to free pages
                    if (memoryManager) {
                        int processId = flight->id % 64;
                        memoryManager->unregisterProcess(processId);
                    }
                    
                    // Financial
                    financialTracker->addLandingFee(Config::LANDING_FEE_BASE);
                    financialTracker->addGateFee(Config::GATE_FEE_PER_MINUTE * 60);
                    
                    pthread_mutex_lock(&statsMutex);
                    stats.totalFlightsProcessed++;
                    pthread_mutex_unlock(&statsMutex);
                }
                break;
                
            default:
                break;
        }
    }
}

void Simulation::updateGroundServices(TimeUnit elapsed) {
    groundServices->updateServices(simulationTime);
}

void Simulation::updatePassengers(TimeUnit elapsed) {
    passengerProcessor->update(simulationTime);
    baggageHandler->update(simulationTime);
}

void Simulation::updateCrew(TimeUnit elapsed) {
    crewManager->update(elapsed);
}

void Simulation::updateCrises(TimeUnit elapsed) {
    crisisManager->update(simulationTime);
}

void Simulation::updateStatistics() {
    pthread_mutex_lock(&statsMutex);
    
    // Flight statistics
    stats.currentActiveFlights = flightManager->getTotalActiveFlights();
    stats.averageDelay = flightManager->getAverageDelay();
    
    // Resource utilization
    std::vector<Runway> runways = runwayManager->getAllRunways();
    int busyRunways = 0;
    for (const auto& rwy : runways) {
        if (rwy.status != RunwayStatus::AVAILABLE) busyRunways++;
    }
    stats.runwayUtilization = static_cast<double>(busyRunways) / runways.size();
    
    std::vector<Gate> gates = gateManager->getAllGates();
    int occupiedGates = 0;
    for (const auto& gate : gates) {
        if (gate.isOccupied) occupiedGates++;
    }
    stats.gateUtilization = static_cast<double>(occupiedGates) / gates.size();
    
    // Memory statistics
    MemoryMetrics memMetrics = memoryManager->getMetrics();
    stats.memoryUtilization = memMetrics.memoryUtilization;
    
    // Scheduler statistics
    SchedulerMetrics schedStats = scheduler->getMetrics();
    if (schedStats.operationsCompleted + schedStats.operationsPending > 0) {
        stats.schedulerEfficiency = static_cast<double>(schedStats.operationsCompleted) /
            (schedStats.operationsCompleted + schedStats.operationsPending);
    }
    
    // Crisis statistics
    stats.emergenciesHandled = crisisManager->getTotalCrisesHandled();
    
    // Passenger statistics
    stats.averageProcessingTime = passengerProcessor->getAverageProcessingTime();
    
    pthread_mutex_unlock(&statsMutex);
}

void Simulation::generateScheduledFlights() {
    std::random_device rd;
    std::mt19937 rng(rd());
    
    // Only generate common passenger aircraft (B737, A320, B777) - no cargo/private
    std::uniform_int_distribution<int> typeDist(0, 2);  // 0=B737, 1=A320, 2=B777
    AircraftType types[] = {AircraftType::B737, AircraftType::A320, AircraftType::B777};
    
    // Only domestic and international flights (no cargo/private)
    std::uniform_int_distribution<int> flightTypeDist(0, 1);  // 0=DOMESTIC, 1=INTERNATIONAL
    FlightType ftypes[] = {FlightType::DOMESTIC, FlightType::INTERNATIONAL};
    
    // Generate limited initial flights to avoid memory overload
    int numFlights = std::min(Config::INITIAL_FLIGHTS, 15);
    
    for (int i = 0; i < numFlights; i++) {
        std::stringstream ss;
        ss << "FL" << std::setfill('0') << std::setw(4) << nextFlightNumber++;
        
        AircraftType type = types[typeDist(rng)];
        FlightType fType = ftypes[flightTypeDist(rng)];
        
        // Stagger arrivals to create waves of contention
        TimeUnit arrivalOffset;
        if (i < 5) {
            arrivalOffset = i * 3;  // 0, 3, 6, 9, 12 seconds - immediate wave
        } else if (i < 10) {
            arrivalOffset = 15 + (i - 5) * 8;  // 15, 23, 31, 39, 47 - second wave
        } else {
            arrivalOffset = 60 + (i - 10) * 15;  // 60, 75, 90... - third wave
        }
        TimeUnit departureOffset = arrivalOffset + 90; // 90 second turnaround
        
        createFlight(ss.str(), type, fType, arrivalOffset, departureOffset);
    }
}

void Simulation::generateRandomFlight() {
    std::random_device rd;
    std::mt19937 rng(rd());
    
    std::stringstream ss;
    ss << "FL" << std::setfill('0') << std::setw(4) << nextFlightNumber++;
    
    // Only generate common passenger aircraft (like generateScheduledFlights)
    std::uniform_int_distribution<int> typeDist(0, 2);  // 0=B737, 1=A320, 2=B777
    AircraftType types[] = {AircraftType::B737, AircraftType::A320, AircraftType::B777};
    
    // Only domestic and international flights
    std::uniform_int_distribution<int> flightTypeDist(0, 1);  // 0=DOMESTIC, 1=INTERNATIONAL
    FlightType ftypes[] = {FlightType::DOMESTIC, FlightType::INTERNATIONAL};
    
    AircraftType type = types[typeDist(rng)];
    FlightType fType = ftypes[flightTypeDist(rng)];
    
    createFlight(ss.str(), type, fType, 30, 120);  // Arrive in 30s, depart in 120s
}

// AWSC-PPC Memory Management Integration
// This function simulates memory page accesses as flights progress through their lifecycle
void Simulation::updateMemory(TimeUnit elapsed) {
    if (!memoryManager) return;
    
    static TimeUnit lastMemoryUpdate = 0;
    static std::mt19937 rng(std::random_device{}());
    
    // Update memory every simulation tick for visible metrics
    if (simulationTime - lastMemoryUpdate < 1) return;
    lastMemoryUpdate = simulationTime;
    
    // IMPORTANT: Don't allocate new pages when memory is under pressure
    // This prevents the 100% utilization freeze
    double currentUtil = memoryManager->getUtilization();
    if (currentUtil > 0.75) {
        // Memory is too full - skip this cycle to let pages get freed
        return;
    }
    
    auto flights = flightManager->getAllActiveFlights();
    
    for (auto* flight : flights) {
        // Use flight->id for consistent process tracking (matches unregisterProcess on departure)
        int processId = flight->id % 64;  // Max 64 processes
        int pageBase = flight->id % 32;
        
        // Simulate different page accesses based on flight status
        // Only access ONE page per flight per tick to reduce memory pressure
        switch (flight->status) {
            case FlightStatus::APPROACHING:
            case FlightStatus::LANDING:
                // Radar/tracking data pages (pages 0-15)
                memoryManager->accessPage(processId, pageBase % 16);
                break;
                
            case FlightStatus::LANDED:
            case FlightStatus::TAXIING_IN:
                // Ground control data pages (pages 16-31)
                memoryManager->accessPage(processId, 16 + (pageBase % 16));
                break;
                
            case FlightStatus::AT_GATE:
                // Passenger manifest, cargo, gate data pages (pages 32-47)
                memoryManager->accessPage(processId, 32 + (pageBase % 16));
                break;
                
            case FlightStatus::BOARDING:
            case FlightStatus::PUSHBACK:
                // Boarding/departure data pages (pages 48-63)
                memoryManager->accessPage(processId, 48 + (pageBase % 16));
                break;
                
            case FlightStatus::TAXIING_OUT:
            case FlightStatus::DEPARTING:
                // Flight plan/departure data pages (pages 64-79)
                memoryManager->accessPage(processId, 64 + (pageBase % 16));
                break;
                
            default:
                break;
        }
    }
    
    // Background prefetch simulation - occasionally access related pages
    if (simulationTime % 5 == 0 && !flights.empty()) {
        std::uniform_int_distribution<int> pageDist(0, 127);
        std::uniform_int_distribution<int> procDist(0, 63);
        
        // Simulate predictive prefetching by accessing nearby pages
        for (int i = 0; i < 3; i++) {
            memoryManager->accessPage(procDist(rng), pageDist(rng));
        }
    }
}
