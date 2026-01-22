/**
 * Smart Airport Operations Management System
 * types.h - Core data types, enums, and constants
 * 
 * This file defines all the fundamental data structures used throughout
 * the airport simulation system.
 */

#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <cmath>
#include <thread>

#include <pthread.h>
#include <unistd.h>

// ============================================================================
// LOGGING MACROS 
// ============================================================================
// Ensure we don't conflict with system macros
#ifdef ERROR
#undef ERROR
#endif

// ============================================================================
// TIME AND SIMULATION CONSTANTS
// ============================================================================

using TimeUnit = long long;  // Time in simulation time units (1 unit = 1 second real-time)
using SimClock = std::chrono::steady_clock;
using SimTime = std::chrono::time_point<SimClock>;

// ============================================================================
// AIRCRAFT TYPES AND SPECIFICATIONS
// ============================================================================

enum class AircraftType {
    // Passenger Aircraft
    A380,       // Super heavy, international long-haul
    B777,       // Heavy, international long-haul
    B737,       // Medium, domestic/short-haul
    A320,       // Medium, domestic/short-haul
    
    // Cargo Aircraft
    B747F,      // Cargo, 24/7 operations
    B777F,      // Cargo, 24/7 operations
    
    // Private Jets
    G650,       // VIP, minimal ground time
    FALCON_7X,  // VIP, minimal ground time
    
    // Emergency Aircraft
    MEDICAL_EVAC,   // Medical evacuation
    DIVERSION,      // Diverted aircraft
    FUEL_EMERGENCY  // Fuel emergency
};

enum class WakeTurbulenceCategory {
    SUPER,      // A380 only - requires largest separation
    HEAVY,      // B777, B747F, B777F
    MEDIUM,     // B737, A320
    LIGHT       // Private jets
};

struct AircraftSpecs {
    std::string code;           // Short code (e.g., "A380")
    std::string name;           // Full name
    WakeTurbulenceCategory wakeCategory;
    int passengerCapacity;
    int cargoCapacityKg;
    int fuelCapacityGallons;
    int crewRequired;           // Minimum cabin crew
    int cockpitCrew;            // Pilots required
    double serviceTimeMultiplier; // Base multiplier for ground services
    int requiredGateSize;       // 1=small, 2=medium, 3=large, 4=super (A380)
    int minTurnaroundMinutes;   // Minimum turnaround time
    bool isEmergency;
    bool isCargo;
    bool isPrivate;
};

// ============================================================================
// FLIGHT STATUS AND TYPES
// ============================================================================

enum class FlightStatus {
    SCHEDULED,          // Not yet active
    APPROACHING,        // In approach queue
    LANDING,            // On runway landing
    LANDED,             // Landed, waiting for gate
    TAXIING_IN,         // Taxiing from runway to gate
    AT_GATE,            // Parked at gate, being serviced
    BOARDING,           // Passengers boarding
    PUSHBACK,           // Pushing back from gate
    TAXIING_OUT,        // Taxiing to runway
    WAITING_TAKEOFF,    // In takeoff queue
    DEPARTING,          // On runway taking off
    DEPARTED,           // Left airspace
    CANCELLED,          // Flight cancelled
    DIVERTED,           // Diverted to another airport
    GO_AROUND           // Aborted landing
};

enum class FlightType {
    DOMESTIC,
    INTERNATIONAL,
    CARGO,
    PRIVATE,
    EMERGENCY
};

// ============================================================================
// GATE TYPES AND SPECIFICATIONS
// ============================================================================

enum class GateType {
    INTERNATIONAL_LARGE,    // A380 capable, customs facilities
    INTERNATIONAL_MEDIUM,   // B777/B747 capable, customs facilities
    DOMESTIC_LARGE,         // B777 capable
    DOMESTIC_MEDIUM,        // B737/A320
    CARGO,                  // Cargo aircraft only
    PRIVATE                 // Private jets
};

struct Gate {
    int id;
    std::string name;
    GateType type;
    int sizeCapacity;       // Max aircraft size (1-4)
    bool hasCustoms;
    bool isOccupied;
    int currentFlightId;
    TimeUnit availableAt;
    int terminalId;
    double walkingDistanceToSecurity; // meters
};

// ============================================================================
// RUNWAY TYPES AND STATUS
// ============================================================================

enum class RunwayStatus {
    AVAILABLE,
    OCCUPIED_LANDING,
    OCCUPIED_TAKEOFF,
    OCCUPIED_CROSSING,
    CLOSED_WEATHER,
    CLOSED_MAINTENANCE,
    CLOSED_EMERGENCY
};

struct Runway {
    int id;
    std::string name;
    int heading;            // 0-360 degrees
    int length;             // meters
    bool canHandleHeavy;
    bool canHandleSuper;    // A380 capable
    RunwayStatus status;
    int currentFlightId;
    TimeUnit busyUntil;
};

// ============================================================================
// GROUND SERVICES
// ============================================================================

enum class ServiceType {
    PASSENGER_DEBOARDING,
    BAGGAGE_UNLOADING,
    FIRE_SAFETY_CHECK,
    REFUELING,
    CLEANING,
    CATERING,
    MAINTENANCE_CHECK,
    BAGGAGE_LOADING,
    PASSENGER_BOARDING,
    PUSHBACK,
    DEICING           // Winter only
};

enum class ResourceType {
    FUEL_TRUCK,
    BAGGAGE_CART,
    CATERING_TRUCK,
    CLEANING_CREW,
    MAINTENANCE_TEAM,
    JETBRIDGE,
    DEICING_TRUCK
};

struct ServiceTask {
    int id;
    int flightId;
    ServiceType type;
    TimeUnit estimatedDuration;
    TimeUnit actualDuration;
    TimeUnit startTime;
    TimeUnit endTime;
    bool completed;
    bool inProgress;
    std::vector<ServiceType> dependencies;
    std::vector<ResourceType> requiredResources;
    double progressPercent;
};

// ============================================================================
// PASSENGER AND BAGGAGE
// ============================================================================

enum class PassengerType {
    REGULAR,
    VIP,                // Fast-track processing
    DISABLED,           // Requires assistance
    UNACCOMPANIED_MINOR, // Requires escort
    CONNECTING,         // Tight connection
    CREW_MEMBER
};

enum class PassengerStatus {
    ARRIVED,
    CHECK_IN_QUEUE,
    CHECKING_IN,
    SECURITY_QUEUE,
    SECURITY_SCREENING,
    SECONDARY_SCREENING,
    CUSTOMS_QUEUE,
    CUSTOMS_PROCESSING,
    IN_TERMINAL,
    AT_GATE,
    BOARDING,
    BOARDED,
    MISSED_FLIGHT,
    REBOOKED
};

struct Passenger {
    int id;
    std::string name;
    PassengerType type;
    PassengerStatus status;
    int departingFlightId;
    int arrivingFlightId;
    int gateNumber;
    TimeUnit connectionDeadline;
    bool isConnecting;
    bool hasCheckedBaggage;
    int baggageCount;
    TimeUnit processingStartTime;
};

enum class BaggageStatus {
    CHECKED_IN,
    IN_SORTING,
    ON_CONVEYOR,
    BEING_LOADED,
    LOADED,
    MISROUTED,
    PRIORITY,
    RETRIEVED,
    TRANSFER
};

struct Baggage {
    int id;
    int passengerId;
    int flightId;
    BaggageStatus status;
    bool isPriority;        // Tight connection
    bool isOversized;
    bool hasRFIDTracking;
    std::string currentLocation;
    int destinationFlight;  // For transfers
};

// ============================================================================
// CREW MANAGEMENT
// ============================================================================

enum class CrewType {
    PILOT_CAPTAIN,
    PILOT_FIRST_OFFICER,
    CABIN_CREW,
    GROUND_LOADER,
    GROUND_CLEANER,
    GROUND_REFUELER,
    GROUND_MECHANIC,
    ATC_CONTROLLER,
    GATE_AGENT
};

enum class CrewStatus {
    AVAILABLE,
    ON_DUTY,
    ON_BREAK,
    FATIGUED,
    OFF_DUTY,
    STANDBY
};

struct CrewMember {
    int id;
    std::string name;
    CrewType type;
    CrewStatus status;
    double fatigueLevel;        // 0.0 to 1.0
    double skillLevel;          // 0.8 to 1.2 (affects service efficiency)
    TimeUnit dutyStartTime;
    TimeUnit totalDutyHours;
    TimeUnit lastBreakTime;
    std::vector<AircraftType> certifications;
    int currentAssignment;
    TimeUnit availableAt;
};

// ============================================================================
// WEATHER AND CRISIS
// ============================================================================

enum class WeatherType {
    CLEAR,
    LIGHT_RAIN,
    HEAVY_RAIN,
    THUNDERSTORM,
    FOG,
    SNOW,
    ICE,
    HIGH_WIND,
    TORNADO_WARNING
};

enum class EmergencyType {
    NONE,
    MECHANICAL_FAILURE,
    MEDICAL_EMERGENCY,
    SECURITY_INCIDENT,
    FUEL_SHORTAGE,
    BIRD_STRIKE,
    HYDRAULIC_FAILURE,
    SUSPICIOUS_PACKAGE,
    UNRULY_PASSENGER,
    CYBER_THREAT
};

enum class ThreatLevel {
    NORMAL = 1,         // Standard operations
    ELEVATED = 2,       // Enhanced screening (+30% time)
    HIGH = 3,           // All bags screened (+50% time)
    SEVERE = 4,         // Only critical flights
    CRITICAL = 5        // Complete shutdown
};

struct WeatherCondition {
    WeatherType type;
    int windSpeed;          // knots
    int windDirection;      // degrees
    int visibility;         // meters
    bool requiresDeicing;
    double operationMultiplier; // Slowdown factor
    int estimatedDuration;      // minutes
};

// ============================================================================
// SCHEDULING ALGORITHM (HMFQ-PPRA) STRUCTURES
// ============================================================================

enum class SchedulerQueue {
    EMERGENCY = 0,      // Priority 0-10, unlimited quantum
    CRITICAL = 1,       // Priority 11-30, 200 time units
    HIGH = 2,           // Priority 31-50, 150 time units
    NORMAL = 3,         // Priority 51-75, 100 time units
    LOW = 4             // Priority 76-100, 50 time units
};

enum class OperationType {
    RUNWAY_LANDING,
    RUNWAY_TAKEOFF,
    GATE_ASSIGNMENT,
    GROUND_SERVICE,
    PASSENGER_PROCESSING,
    BAGGAGE_HANDLING,
    CREW_ASSIGNMENT,
    EMERGENCY_RESPONSE,
    MAINTENANCE,
    TAXIWAY_MOVEMENT
};

struct Operation {
    int id;
    OperationType type;
    int priority;               // 0-100 (lower = higher priority)
    SchedulerQueue currentQueue;
    TimeUnit arrivalTime;
    TimeUnit waitTime;
    TimeUnit quantumUsed;
    TimeUnit totalQuantumUsed;
    int quantumExpirations;
    bool isPreempted;
    bool hasGuaranteedService;
    int preemptionCount;
    
    // Predictive Impact Score components
    double delayPropagationFactor;
    double connectionRiskFactor;
    double resourceUtilizationImpact;
    double weatherRiskFactor;
    double fuelCriticalityFactor;
    double predictedImpactScore;
    
    // Priority inheritance
    int originalPriority;
    int inheritedPriority;
    bool hasPriorityInheritance;
    
    // Operation-specific data
    int flightId;
    int resourceId;
    std::function<void()> execute;
    std::function<bool()> isComplete;
    
    // Historical stats for adaptive learning
    TimeUnit estimatedDuration;
    TimeUnit actualDuration;
};

// ============================================================================
// PAGE REPLACEMENT ALGORITHM (AWSC-PPC) STRUCTURES
// ============================================================================

enum class ProcessPhase {
    INITIALIZATION,
    COMPUTATION,
    IO,
    TERMINATION
};

enum class AccessPattern {
    SEQUENTIAL,
    RANDOM,
    STRIDED,
    LOOP
};

struct PageFrame {
    int pageId;
    int processId;
    bool referenceBit;          // R
    bool modifiedBit;           // M
    int accessFrequency;        // F (16-bit counter with decay)
    TimeUnit lastAccessTime;    // T
    AccessPattern accessPattern;
    bool workingSetMember;      // W
    bool prefetchCandidate;     // P
    double compressionRatio;
    bool lockBit;               // L (pinned pages)
    double predictionScore;     // Future access probability
    bool isCompressed;
    std::vector<char> compressedData;
};

struct ProcessMemoryState {
    int processId;
    ProcessPhase phase;
    int workingSetWindow;       // Δ
    std::vector<int> referenceHistory;
    int faultCount;
    int accessCount;
    double faultRate;
    std::map<int, std::map<int, int>> transitionMatrix; // Markov chain
    int lastAccessedPage;
};

// ============================================================================
// DATA MANAGEMENT LEVELS
// ============================================================================

enum class DataLevel {
    CRITICAL_REALTIME = 1,  // 1 time unit access
    ACTIVE_OPERATIONS = 2,  // 5 time units access
    SYSTEM_DATA = 3,        // 20 time units access
    HISTORICAL = 4          // 100 time units access
};

// ============================================================================
// FINANCIAL TRACKING
// ============================================================================

struct FinancialRecord {
    TimeUnit timestamp;
    std::string category;
    double amount;
    bool isRevenue;
    std::string description;
};

struct Budget {
    double totalBudget;
    double spent;
    double revenue;
    std::vector<FinancialRecord> records;
};

// ============================================================================
// PERFORMANCE METRICS
// ============================================================================

struct PerformanceMetrics {
    // Real-time metrics
    int activeFlightsArriving;
    int activeFlightsDeparting;
    int activeFlightsTaxiing;
    int activeFlightsAtGate;
    double runwayUtilization;
    double gateOccupancy;
    double avgWaitTime;
    int currentDelays;
    std::map<ResourceType, int> resourceAvailability;
    int activeEmergencies;
    
    // Historical statistics
    int totalFlightsHandled;
    double avgTurnaroundTime;
    double onTimePerformance;
    int passengerThroughput;
    double resourceEfficiency;
    double totalCosts;
    double totalRevenue;
    double systemUptime;
    
    // Bottleneck analysis
    std::string mostConstrainedResource;
    int mostDelayedOperationType;
    TimeUnit peakCongestionTime;
};

// ============================================================================
// TAXIWAY GRAPH NODE
// ============================================================================

struct TaxiwayNode {
    int id;
    std::string name;
    int x, y;               // Position coordinates
    std::vector<int> connectedNodes;
    bool isIntersection;
    bool isRunwayCrossing;
    bool isGateArea;
    int gateId;             // -1 if not a gate area
    int runwayId;           // -1 if not a runway crossing
    bool isOccupied;
    int occupyingFlightId;
};

// ============================================================================
// FLIGHT STRUCTURE
// ============================================================================

struct Flight {
    int id;
    std::string flightNumber;
    AircraftType aircraftType;
    FlightType flightType;
    FlightStatus status;
    
    // Schedule
    TimeUnit scheduledArrival;
    TimeUnit actualArrival;
    TimeUnit scheduledDeparture;
    TimeUnit estimatedDeparture;
    TimeUnit actualDeparture;
    
    // Assignments
    int assignedGate;
    int assignedRunway;
    std::vector<int> taxiPath;
    int currentTaxiwayNode;
    
    // Passengers and cargo
    int totalPassengers;
    int connectingPassengers;
    int baggageCount;
    int cargoWeight;
    
    // Crew
    std::vector<int> assignedCrew;
    
    // Services
    std::vector<ServiceTask> services;
    double turnaroundProgress;
    
    // Fuel
    int fuelOnBoard;        // gallons
    int fuelRequired;       // for departure
    bool hasFuelEmergency;
    
    // Emergency flags
    bool isEmergency;
    EmergencyType emergencyType;
    int priorityOverride;   // -1 if none
    
    // Delay tracking
    int delayMinutes;
    std::string delayReason;
};

// ============================================================================
// LOGGING AND DEBUGGING
// ============================================================================

enum class LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
};

struct LogEntry {
    TimeUnit timestamp;
    LogLevel level;
    std::string subsystem;
    std::string message;
    int relatedFlightId;
    int relatedOperationId;
};

#endif // TYPES_H
