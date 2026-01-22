/**
 * Smart Airport Operations Management System
 * config.h - Configuration parameters and constants
 * 
 * This file contains all configurable parameters for the simulation.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

namespace Config {

// ============================================================================
// SIMULATION SETTINGS
// ============================================================================

constexpr bool REAL_TIME_MODE = true;       // 1 sim second = 1 real second
constexpr TimeUnit SIMULATION_DURATION = 86400; // 24 hours in seconds
constexpr TimeUnit DASHBOARD_UPDATE_INTERVAL = 30; // 30 seconds
constexpr TimeUnit LOG_FLUSH_INTERVAL = 60;

// ============================================================================
// AIRPORT INFRASTRUCTURE
// ============================================================================

constexpr int NUM_RUNWAYS = 3;
constexpr int NUM_GATES_INTERNATIONAL_LARGE = 4;
constexpr int NUM_GATES_INTERNATIONAL_MEDIUM = 8;
constexpr int NUM_GATES_DOMESTIC_LARGE = 6;
constexpr int NUM_GATES_DOMESTIC_MEDIUM = 12;
constexpr int NUM_GATES_CARGO = 4;
constexpr int NUM_GATES_PRIVATE = 6;
constexpr int TOTAL_GATES = 40;

// Taxiway network size
constexpr int TAXIWAY_NODES = 50;
constexpr int TAXIWAY_EDGES = 80;

// ============================================================================
// GROUND RESOURCES
// ============================================================================

constexpr int NUM_FUEL_TRUCKS = 8;
constexpr int NUM_BAGGAGE_CARTS = 12;
constexpr int NUM_CATERING_TRUCKS = 6;
constexpr int NUM_CLEANING_CREWS = 4;
constexpr int NUM_MAINTENANCE_TEAMS = 3;
constexpr int NUM_JETBRIDGES = 10;
constexpr int NUM_DEICING_TRUCKS = 2;

// Service time ranges (in seconds) - DEMO TIMES for fast turnaround
constexpr int REFUEL_TIME_MIN = 2;       // Demo: 2 seconds
constexpr int REFUEL_TIME_MAX = 4;       // Demo: 4 seconds
constexpr int CLEANING_TIME_MIN = 2;     // Demo: 2 seconds
constexpr int CLEANING_TIME_MAX = 4;     // Demo: 4 seconds
constexpr int CATERING_TIME_MIN = 2;     // Demo: 2 seconds
constexpr int CATERING_TIME_MAX = 4;     // Demo: 4 seconds
constexpr int DEICING_TIME_MIN = 2;      // Demo: 2 seconds
constexpr int DEICING_TIME_MAX = 4;      // Demo: 4 seconds

// Service dependencies - additional time for international (30%)
constexpr double INTERNATIONAL_TIME_MULTIPLIER = 1.3;

// Equipment failure probability
constexpr double EQUIPMENT_FAILURE_PROBABILITY = 0.005;  // 0.5%

// Crew skill variation range
constexpr double CREW_SKILL_MIN = 0.8;
constexpr double CREW_SKILL_MAX = 1.2;

// ============================================================================
// PASSENGER PROCESSING
// ============================================================================

constexpr int NUM_CHECKIN_COUNTERS = 10;
constexpr int NUM_SECURITY_LANES = 5;
constexpr int CHECKIN_TIME_MIN = 120;   // 2 min
constexpr int CHECKIN_TIME_MAX = 300;   // 5 min
constexpr int SECURITY_TIME_MIN = 180;  // 3 min
constexpr int SECURITY_TIME_MAX = 480;  // 8 min
constexpr int CUSTOMS_TIME_MIN = 120;   // 2 min
constexpr int CUSTOMS_TIME_MAX = 600;   // 10 min
constexpr int BOARDING_TIME_MIN = 2;    // Demo: 2 seconds
constexpr int BOARDING_TIME_MAX = 4;    // Demo: 4 seconds

// Secondary screening probability
constexpr double SECONDARY_SCREENING_PROBABILITY = 0.05;

// Connection thresholds
constexpr int TIGHT_CONNECTION_THRESHOLD = 3600; // 60 min in seconds
constexpr int MIN_CONNECTION_TIME = 2400;        // 40 min

// VIP fast-track speed multiplier
constexpr double VIP_SPEED_MULTIPLIER = 0.5; // 50% faster

// ============================================================================
// BAGGAGE HANDLING
// ============================================================================

constexpr double BAGGAGE_MISROUTE_PROBABILITY = 0.05;
constexpr int BAGGAGE_CONVEYOR_SPEED = 2; // meters per second
constexpr int BAGGAGE_SORTING_TIME = 30;  // seconds per bag

// ============================================================================
// CREW CONSTRAINTS
// ============================================================================

// Maximum duty hours (in seconds)
constexpr int PILOT_MAX_DUTY_HOURS = 28800;     // 8 hours
constexpr int CABIN_CREW_MAX_DUTY_HOURS = 43200; // 12 hours
constexpr int GROUND_CREW_MAX_DUTY_HOURS = 28800; // 8 hours
constexpr int ATC_SHIFT_DURATION = 7200;         // 2 hours

// Break requirements (in seconds)
constexpr int MIN_REST_BETWEEN_DUTIES = 28800; // 8 hours
constexpr int BREAK_INTERVAL = 7200;           // 2 hours
constexpr int BREAK_DURATION = 1800;           // 30 min

// Fatigue thresholds
constexpr double FATIGUE_WARNING_LEVEL = 0.7;
constexpr double FATIGUE_CRITICAL_LEVEL = 0.9;
constexpr double FATIGUE_INCREMENT_PER_HOUR = 0.1;
constexpr double FATIGUE_DECREMENT_PER_REST_HOUR = 0.2;

// ============================================================================
// WEATHER AND CRISIS
// ============================================================================

// Weather impact on operations
constexpr double THUNDERSTORM_RUNWAY_CLOSE_DURATION_MIN = 1800; // 30 min
constexpr double THUNDERSTORM_RUNWAY_CLOSE_DURATION_MAX = 3600; // 60 min
constexpr double FOG_LANDING_RATE_REDUCTION = 0.5;
constexpr double HIGH_WIND_THRESHOLD = 35; // knots

// Emergency probabilities per flight
constexpr double MECHANICAL_FAILURE_PROBABILITY = 0.02;
constexpr double MEDICAL_EMERGENCY_PROBABILITY = 0.01;

// ============================================================================
// FINANCIAL PARAMETERS
// ============================================================================

constexpr double FUEL_COST_PER_GALLON = 5.00;
constexpr double CREW_OVERTIME_COST_PER_HOUR = 100.00;
constexpr double DELAY_COMPENSATION_MIN = 200.00;
constexpr double DELAY_COMPENSATION_MAX = 800.00;
constexpr double GATE_RENTAL_PER_HOUR = 500.00;
constexpr double MAINTENANCE_COST_PER_HOUR = 150.00;
constexpr double DEICING_COST_LARGE_AIRCRAFT = 10000.00;

// Initial flights to generate
constexpr int INITIAL_FLIGHTS = 20;

constexpr double LANDING_FEE_BASE = 500.00;
constexpr double GATE_FEE_PER_MINUTE = 8.00; // $480/hr
constexpr double DAILY_BUDGET = 500000.00;

// ============================================================================
// HMFQ-PPRA SCHEDULING ALGORITHM PARAMETERS
// ============================================================================

// Queue quantum values (in time units / seconds)
constexpr TimeUnit QUEUE_0_QUANTUM = -1;    // Unlimited (emergency)
constexpr TimeUnit QUEUE_1_QUANTUM = 200;   // Critical
constexpr TimeUnit QUEUE_2_QUANTUM = 150;   // High
constexpr TimeUnit QUEUE_3_QUANTUM = 100;   // Normal
constexpr TimeUnit QUEUE_4_QUANTUM = 50;    // Low

// Priority ranges
constexpr int QUEUE_0_PRIORITY_MAX = 10;
constexpr int QUEUE_1_PRIORITY_MAX = 30;
constexpr int QUEUE_2_PRIORITY_MAX = 50;
constexpr int QUEUE_3_PRIORITY_MAX = 75;
constexpr int QUEUE_4_PRIORITY_MAX = 100;

// Demotion thresholds (quantum expirations before demotion)
constexpr int QUEUE_1_DEMOTION_THRESHOLD = 2;
constexpr int QUEUE_2_DEMOTION_THRESHOLD = 3;
constexpr int QUEUE_3_DEMOTION_THRESHOLD = 4;

// Aging time constants (in seconds) - reduced for faster aging
constexpr double QUEUE_1_TIME_CONSTANT = 40;   // was 480s (8 min)
constexpr double QUEUE_2_TIME_CONSTANT = 30;   // was 300s (5 min)
constexpr double QUEUE_3_TIME_CONSTANT = 20;   // was 180s (3 min)
constexpr double QUEUE_4_TIME_CONSTANT = 10;   // was 120s (2 min)

// Base aging rate
constexpr double BASE_AGE_RATE = 1.0;
constexpr double AGE_WEIGHT = 3.0;  // was 0.5 - increased for visible promotions

// Predictive Impact Score weights (must sum to 1.0)
constexpr double PIS_ALPHA = 0.25; // Delay propagation
constexpr double PIS_BETA = 0.20;  // Connection risk
constexpr double PIS_GAMMA = 0.20; // Resource utilization
constexpr double PIS_DELTA = 0.15; // Weather risk
constexpr double PIS_EPSILON = 0.20; // Fuel criticality

// Dynamic quantum adjustment
constexpr double LOAD_FACTOR_MIN = 0.4;
constexpr double LOAD_FACTOR_MAX = 1.0;
constexpr double OPERATION_FACTOR_SIMPLE = 0.7;
constexpr double OPERATION_FACTOR_MEDIUM = 1.0;
constexpr double OPERATION_FACTOR_COMPLEX = 1.3;

// Preemption cost-benefit threshold
constexpr double PREEMPTION_THRESHOLD = 1.5;

// Starvation prevention thresholds (in seconds) - reduced for simulation
constexpr TimeUnit QUEUE_4_MAX_WAIT = 60;   // 1 min (was 15 min)
constexpr TimeUnit QUEUE_3_MAX_WAIT = 90;   // 1.5 min (was 20 min)
constexpr TimeUnit QUEUE_2_MAX_WAIT = 120;  // 2 min (was 30 min)

// Max wait before mandatory processing (except Queue 0 dominance)
constexpr TimeUnit ABSOLUTE_MAX_WAIT = 120; // 2 min (was 30 min)

// Aging promotion threshold - promote when priority drops below this
constexpr int AGING_PROMOTION_PRIORITY_THRESHOLD = 55;

// Adaptive learning weight
constexpr double LEARNING_OLD_WEIGHT = 0.7;
constexpr double LEARNING_NEW_WEIGHT = 0.3;

// ============================================================================
// AWSC-PPC PAGE REPLACEMENT PARAMETERS
// ============================================================================

// Working set base window (in time units)
constexpr int WORKING_SET_BASE_WINDOW = 15;

// Phase multipliers
constexpr double PHASE_MULTIPLIER_INIT = 1.5;
constexpr double PHASE_MULTIPLIER_COMPUTE = 1.0;
constexpr double PHASE_MULTIPLIER_IO = 0.7;
constexpr double PHASE_MULTIPLIER_TERMINATE = 0.5;

// Load multipliers
constexpr double LOAD_MULT_LIGHT = 1.3;     // <40% memory
constexpr double LOAD_MULT_MEDIUM = 1.0;    // 40-70%
constexpr double LOAD_MULT_HEAVY = 0.8;     // 70-90%
constexpr double LOAD_MULT_CRITICAL = 0.6;  // >90%

// Fault rate multipliers
constexpr double FAULT_MULT_LOW = 0.9;      // <5%
constexpr double FAULT_MULT_NORMAL = 1.0;   // 5-15%
constexpr double FAULT_MULT_HIGH = 1.2;     // >15%

// Victim score weights (must sum to 1.0)
constexpr double W_FREQ = 0.3;
constexpr double W_REC = 0.3;
constexpr double W_PAT = 0.2;
constexpr double W_PRED = 0.2;

// Pattern component values
constexpr double PATTERN_SEQUENTIAL = 0.5;
constexpr double PATTERN_RANDOM = 1.0;
constexpr double PATTERN_STRIDED = 0.7;

// Dirty page penalty
constexpr double DIRTY_PENALTY = 2.0;

// Compression threshold and benefit
constexpr double COMPRESSION_SIZE_THRESHOLD = 0.6;
constexpr double COMPRESSION_BENEFIT = -0.8;
constexpr double COMPRESSION_AGE_THRESHOLD = 100; // time units

// Process priority multipliers for victim score
constexpr double PRIORITY_MULT_HIGH = 2.0;
constexpr double PRIORITY_MULT_MEDIUM = 1.0;
constexpr double PRIORITY_MULT_LOW = 0.5;

// Frequency decay
constexpr double FREQUENCY_DECAY_FACTOR = 0.9;
constexpr int FREQUENCY_DECAY_INTERVAL = 1000; // time units

// Thrashing threshold
constexpr double THRASHING_THRESHOLD = 0.25;

// Prefetch settings
constexpr int MAX_PREFETCH_COUNT = 5;
constexpr double PREFETCH_MEMORY_THRESHOLD = 0.20; // 20% available

// Prediction confidence threshold
constexpr double PREDICTION_CONFIDENCE_THRESHOLD = 0.6;

// TLB settings
constexpr int TLB_SIZE = 64;

// Memory size (number of page frames)
constexpr int TOTAL_PAGE_FRAMES = 256;
constexpr int PAGE_SIZE = 4096; // bytes

// ============================================================================
// DATA MANAGER ACCESS TIMES
// ============================================================================

constexpr TimeUnit DATA_LEVEL_1_ACCESS_TIME = 1;    // Critical real-time
constexpr TimeUnit DATA_LEVEL_2_ACCESS_TIME = 5;    // Active operations
constexpr TimeUnit DATA_LEVEL_3_ACCESS_TIME = 20;   // System data
constexpr TimeUnit DATA_LEVEL_4_ACCESS_TIME = 100;  // Historical

// ============================================================================
// AIRCRAFT SPECIFICATIONS
// ============================================================================

inline const std::map<AircraftType, AircraftSpecs> AIRCRAFT_SPECS = {
    {AircraftType::A380, {"A380", "Airbus A380", WakeTurbulenceCategory::SUPER, 
        555, 50000, 85000, 22, 2, 3.0, 3, 120, false, false, false}},  // Gate size 3 (was 4)
    {AircraftType::B777, {"B777", "Boeing 777", WakeTurbulenceCategory::HEAVY,
        396, 40000, 47890, 12, 2, 2.0, 3, 90, false, false, false}},
    {AircraftType::B737, {"B737", "Boeing 737", WakeTurbulenceCategory::MEDIUM,
        189, 20000, 6875, 6, 2, 1.0, 2, 45, false, false, false}},
    {AircraftType::A320, {"A320", "Airbus A320", WakeTurbulenceCategory::MEDIUM,
        180, 18000, 6400, 5, 2, 1.0, 2, 45, false, false, false}},
    {AircraftType::B747F, {"B747F", "Boeing 747 Freighter", WakeTurbulenceCategory::HEAVY,
        0, 120000, 57285, 3, 2, 2.5, 3, 90, false, true, false}},
    {AircraftType::B777F, {"B777F", "Boeing 777 Freighter", WakeTurbulenceCategory::HEAVY,
        0, 102000, 47890, 3, 2, 2.2, 3, 75, false, true, false}},
    {AircraftType::G650, {"G650", "Gulfstream G650", WakeTurbulenceCategory::LIGHT,
        19, 2000, 6120, 2, 2, 0.5, 1, 30, false, false, true}},
    {AircraftType::FALCON_7X, {"FALCON7X", "Dassault Falcon 7X", WakeTurbulenceCategory::LIGHT,
        16, 1500, 4700, 2, 2, 0.5, 1, 25, false, false, true}},
    {AircraftType::MEDICAL_EVAC, {"MEDEVAC", "Medical Evacuation", WakeTurbulenceCategory::MEDIUM,
        10, 500, 3000, 4, 2, 0.3, 1, 15, true, false, false}},
    {AircraftType::DIVERSION, {"DIVERT", "Diverted Aircraft", WakeTurbulenceCategory::MEDIUM,
        150, 15000, 5000, 6, 2, 1.0, 2, 60, true, false, false}},
    {AircraftType::FUEL_EMERGENCY, {"FUELEMG", "Fuel Emergency", WakeTurbulenceCategory::MEDIUM,
        150, 15000, 3000, 6, 2, 0.3, 2, 20, true, false, false}}
};

// Wake turbulence separation (in seconds)
inline const std::map<std::pair<WakeTurbulenceCategory, WakeTurbulenceCategory>, int> WAKE_SEPARATION = {
    {{WakeTurbulenceCategory::SUPER, WakeTurbulenceCategory::SUPER}, 180},
    {{WakeTurbulenceCategory::SUPER, WakeTurbulenceCategory::HEAVY}, 240},
    {{WakeTurbulenceCategory::SUPER, WakeTurbulenceCategory::MEDIUM}, 300},
    {{WakeTurbulenceCategory::SUPER, WakeTurbulenceCategory::LIGHT}, 360},
    {{WakeTurbulenceCategory::HEAVY, WakeTurbulenceCategory::SUPER}, 120},
    {{WakeTurbulenceCategory::HEAVY, WakeTurbulenceCategory::HEAVY}, 120},
    {{WakeTurbulenceCategory::HEAVY, WakeTurbulenceCategory::MEDIUM}, 180},
    {{WakeTurbulenceCategory::HEAVY, WakeTurbulenceCategory::LIGHT}, 240},
    {{WakeTurbulenceCategory::MEDIUM, WakeTurbulenceCategory::SUPER}, 90},
    {{WakeTurbulenceCategory::MEDIUM, WakeTurbulenceCategory::HEAVY}, 90},
    {{WakeTurbulenceCategory::MEDIUM, WakeTurbulenceCategory::MEDIUM}, 90},
    {{WakeTurbulenceCategory::MEDIUM, WakeTurbulenceCategory::LIGHT}, 120},
    {{WakeTurbulenceCategory::LIGHT, WakeTurbulenceCategory::SUPER}, 60},
    {{WakeTurbulenceCategory::LIGHT, WakeTurbulenceCategory::HEAVY}, 60},
    {{WakeTurbulenceCategory::LIGHT, WakeTurbulenceCategory::MEDIUM}, 60},
    {{WakeTurbulenceCategory::LIGHT, WakeTurbulenceCategory::LIGHT}, 60}
};

// ============================================================================
// SCALABILITY LIMITS
// ============================================================================

constexpr int MAX_CONCURRENT_FLIGHTS = 100;
constexpr int MAX_PASSENGERS_PER_HOUR = 5000;
constexpr int MAX_OPERATIONS_IN_QUEUE = 500;

} // namespace Config

#endif // CONFIG_H
