/**
 * Smart Airport Operations Management System
 * display.cpp - Text-Based Display Implementation
 */

#include "display.h"
#include <iomanip>
#include <algorithm>

Display::Display(Simulation* sim) : simulation(sim) {}

std::string Display::renderDashboard() {
    std::stringstream ss;
    
    ss << renderHeader();
    ss << doubleLine(displayWidth) << "\n";
    ss << renderFlightBoard();
    ss << horizontalLine(displayWidth) << "\n";
    ss << renderRunwayStatus();
    ss << renderGateStatus();
    ss << horizontalLine(displayWidth) << "\n";
    ss << renderResourceStatus();
    ss << horizontalLine(displayWidth) << "\n";
    ss << renderCrisisAlerts();
    ss << horizontalLine(displayWidth) << "\n";
    ss << renderStatistics();
    ss << horizontalLine(displayWidth) << "\n";
    ss << renderFinancials();
    ss << horizontalLine(displayWidth) << "\n";
    ss << renderSchedulerMetrics();
    ss << renderMemoryMetrics();
    ss << doubleLine(displayWidth) << "\n";
    
    return ss.str();
}

std::string Display::renderHeader() {
    std::stringstream ss;
    
    TimeUnit simTime = simulation->getSimulationTime();
    int hours = (simTime / 3600) % 24;
    int minutes = (simTime / 60) % 60;
    int seconds = simTime % 60;
    
    ss << doubleLine(displayWidth) << "\n";
    ss << centerString("SMART AIRPORT OPERATIONS MANAGEMENT SYSTEM", displayWidth) << "\n";
    ss << centerString("HMFQ-PPRA + AWSC-PPC Implementation", displayWidth) << "\n";
    ss << doubleLine(displayWidth) << "\n";
    
    std::stringstream time;
    time << "Simulation Time: " << std::setfill('0') << std::setw(2) << hours << ":"
         << std::setw(2) << minutes << ":" << std::setw(2) << seconds
         << " | Speed: " << simulation->getSpeed() << "x";
    
    ss << centerString(time.str(), displayWidth) << "\n";
    
    return ss.str();
}

std::string Display::renderFlightBoard() {
    std::stringstream ss;
    
    ss << "\n" << centerString("[ FLIGHT INFORMATION DISPLAY ]", displayWidth) << "\n\n";
    
    // Header row
    ss << "| " << padString("Flight", 8)
       << "| " << padString("Type", 6)
       << "| " << padString("Status", 18)
       << "| " << padString("Gate", 6)
       << "| " << padString("Runway", 8)
       << "| " << padString("Delay", 8)
       << "| " << padString("Progress", 10) << "|\n";
    ss << horizontalLine(displayWidth) << "\n";
    
    // Get active flights
    FlightManager* fm = simulation->getFlightManager();
    if (fm) {
        std::vector<Flight*> flights = fm->getAllActiveFlights();
        
        int displayed = 0;
        for (Flight* flight : flights) {
            if (displayed >= 15) {
                ss << "| " << padString("... and " + std::to_string(flights.size() - 15) + " more", displayWidth - 4) << "|\n";
                break;
            }
            
            // Calculate progress based on flight status
            double progress = 0;
            switch (flight->status) {
                case FlightStatus::SCHEDULED: progress = 0; break;
                case FlightStatus::APPROACHING: progress = 10; break;
                case FlightStatus::LANDING: progress = 25; break;
                case FlightStatus::LANDED: progress = 35; break;
                case FlightStatus::TAXIING_IN: progress = 45; break;
                case FlightStatus::AT_GATE: progress = 55; break;
                case FlightStatus::PUSHBACK: progress = 75; break;
                case FlightStatus::TAXIING_OUT: progress = 85; break;
                case FlightStatus::DEPARTING: progress = 95; break;
                case FlightStatus::DEPARTED: progress = 100; break;
                default: progress = 0; break;
            }
            
            ss << "| " << padString(flight->flightNumber, 8)
               << "| " << padString(aircraftTypeToString(flight->aircraftType), 6)
               << "| " << padString(flightStatusToString(flight->status), 18)
               << "| " << padString(flight->assignedGate >= 0 ? std::to_string(flight->assignedGate) : "-", 6)
               << "| " << padString(flight->assignedRunway >= 0 ? std::to_string(flight->assignedRunway) : "-", 8)
               << "| " << padString(flight->delayMinutes > 0 ? std::to_string(flight->delayMinutes) + "m" : "-", 8)
               << "| " << padString(formatPercent(progress), 10) << "|\n";
            
            displayed++;
        }
        
        if (flights.empty()) {
            ss << "| " << padString("No active flights", displayWidth - 4) << "|\n";
        }
    }
    
    ss << "\n";
    return ss.str();
}

std::string Display::renderRunwayStatus() {
    std::stringstream ss;
    
    ss << "\n" << centerString("[ RUNWAY STATUS ]", displayWidth) << "\n\n";
    
    DataManager* dm = simulation->getDataManager();
    if (dm) {
        for (int i = 0; i < Config::NUM_RUNWAYS; i++) {
            RunwayStatus status = dm->getRunwayStatus(i);
            ss << "  Runway " << i << ": " << runwayStatusToString(status) << "\n";
        }
    }
    
    ss << "\n";
    return ss.str();
}

std::string Display::renderGateStatus() {
    std::stringstream ss;
    
    ss << centerString("[ GATE UTILIZATION ]", displayWidth) << "\n\n";
    
    SimulationStats stats = simulation->getStats();
    
    // Visual bar
    int barWidth = 50;
    int filled = static_cast<int>(stats.gateUtilization * barWidth);
    
    ss << "  [";
    for (int i = 0; i < barWidth; i++) {
        ss << (i < filled ? "#" : " ");
    }
    ss << "] " << formatPercent(stats.gateUtilization * 100) << "\n\n";
    
    return ss.str();
}

std::string Display::renderResourceStatus() {
    std::stringstream ss;
    
    ss << "\n" << centerString("[ GROUND RESOURCES ]", displayWidth) << "\n\n";
    
    DataManager* dm = simulation->getDataManager();
    if (dm) {
        ss << "  Fuel Trucks:      " << dm->getAvailableResourceCount(ResourceType::FUEL_TRUCK) 
           << "/" << Config::NUM_FUEL_TRUCKS << " available\n";
        ss << "  Baggage Carts:    " << dm->getAvailableResourceCount(ResourceType::BAGGAGE_CART) 
           << "/" << Config::NUM_BAGGAGE_CARTS << " available\n";
        ss << "  Catering Trucks:  " << dm->getAvailableResourceCount(ResourceType::CATERING_TRUCK) 
           << "/" << Config::NUM_CATERING_TRUCKS << " available\n";
        ss << "  Cleaning Crews:   " << dm->getAvailableResourceCount(ResourceType::CLEANING_CREW) 
           << "/" << Config::NUM_CLEANING_CREWS << " available\n";
        ss << "  Maintenance:      " << dm->getAvailableResourceCount(ResourceType::MAINTENANCE_TEAM) 
           << "/" << Config::NUM_MAINTENANCE_TEAMS << " available\n";
        ss << "  Jetbridges:       " << dm->getAvailableResourceCount(ResourceType::JETBRIDGE) 
           << "/" << Config::NUM_JETBRIDGES << " available\n";
        ss << "  De-icing Trucks:  " << dm->getAvailableResourceCount(ResourceType::DEICING_TRUCK) 
           << "/" << Config::NUM_DEICING_TRUCKS << " available\n";
    }
    
    ss << "\n";
    return ss.str();
}

std::string Display::renderCrisisAlerts() {
    std::stringstream ss;
    
    ss << "\n" << centerString("[ ALERTS & WEATHER ]", displayWidth) << "\n\n";
    
    DataManager* dm = simulation->getDataManager();
    if (dm) {
        WeatherCondition weather = dm->getCurrentWeather();
        ss << "  Current Weather: " << weatherTypeToString(weather.type) << "\n";
        ss << "  Wind: " << weather.windSpeed << " knots from " << weather.windDirection << " deg\n";
        ss << "  Visibility: " << weather.visibility << " m\n";
        ss << "  De-icing Required: " << (weather.requiresDeicing ? "YES" : "No") << "\n";
        ss << "  Operations: " << formatPercent(weather.operationMultiplier * 100) << " capacity\n\n";
        
        std::vector<std::string> emergencies = dm->getActiveEmergencies();
        if (!emergencies.empty()) {
            ss << "  ACTIVE ALERTS:\n";
            for (const auto& alert : emergencies) {
                ss << "    [!] " << alert << "\n";
            }
        } else {
            ss << "  No active alerts\n";
        }
    }
    
    ss << "\n";
    return ss.str();
}

std::string Display::renderStatistics() {
    std::stringstream ss;
    
    SimulationStats stats = simulation->getStats();
    
    ss << "\n" << centerString("[ OPERATIONAL STATISTICS ]", displayWidth) << "\n\n";
    
    ss << "  Flights Processed:    " << stats.totalFlightsProcessed << "\n";
    ss << "  Active Flights:       " << stats.currentActiveFlights << "\n";
    ss << "  Average Delay:        " << stats.averageDelay << " minutes\n";
    ss << "  Runway Utilization:   " << formatPercent(stats.runwayUtilization * 100) << "\n";
    ss << "  Gate Utilization:     " << formatPercent(stats.gateUtilization * 100) << "\n";
    ss << "  Emergencies Handled:  " << stats.emergenciesHandled << "\n";
    
    ss << "\n";
    return ss.str();
}

std::string Display::renderFinancials() {
    std::stringstream ss;
    
    FinancialTracker* ft = simulation->getFinancialTracker();
    if (!ft) return "";
    
    ss << "\n" << centerString("[ FINANCIAL SUMMARY ]", displayWidth) << "\n\n";
    
    double revenue = ft->getTotalRevenue();
    double costs = ft->getTotalCosts();
    double profit = ft->getProfit();
    
    ss << "  Total Revenue:   " << formatCurrency(revenue) << "\n";
    ss << "  Total Costs:     " << formatCurrency(costs) << "\n";
    ss << "  Net Profit:      " << formatCurrency(profit) 
       << (profit >= 0 ? " (PROFIT)" : " (LOSS)") << "\n";
    ss << "  Budget Status:   " << (ft->isWithinBudget() ? "Within Budget" : "OVER BUDGET") << "\n";
    ss << "  Budget Remain:   " << formatCurrency(ft->getBudgetRemaining()) << "\n";
    
    ss << "\n";
    return ss.str();
}

std::string Display::renderSchedulerMetrics() {
    std::stringstream ss;
    
    Scheduler* sched = simulation->getScheduler();
    if (!sched) return "";
    
    SchedulerMetrics stats = sched->getMetrics();
    
    ss << "\n" << centerString("[ HMFQ-PPRA SCHEDULER METRICS ]", displayWidth) << "\n\n";
    
    // Layer 1: Multi-Level Feedback Queue stats
    ss << "  === Multi-Level Feedback Queue ==="  << "\n";
    ss << "  Operations Scheduled: " << stats.totalOperationsScheduled << "\n";
    ss << "  Operations Completed: " << stats.operationsCompleted << "\n";
    ss << "  Operations Pending:   " << stats.operationsPending << "\n";
    
    // Layer 3/5/6/7: Aging, Inheritance, Preemption, Starvation
    ss << "  === Algorithm Activity ==="  << "\n";
    ss << "  Promotions (Aging):   " << stats.totalPromotions << "\n";
    ss << "  Demotions:            " << stats.totalDemotions << "\n";
    ss << "  Preemptions (L6):     " << stats.totalPreemptions << "\n";
    ss << "  Priority Inherit(L5): " << stats.priorityInheritances << "\n";
    ss << "  Starvation Prev(L7):  " << stats.starvationPreventions << "\n";
    
    // Performance metrics
    ss << "  === Performance ==="  << "\n";
    ss << "  Avg Wait Time:        " << stats.avgWaitTime << " sec\n";
    ss << "  Avg Turnaround:       " << stats.avgTurnaroundTime << " sec\n";
    double efficiency = (stats.operationsCompleted + stats.operationsPending > 0) ?
                        (double)stats.operationsCompleted / (stats.operationsCompleted + stats.operationsPending) : 0.0;
    ss << "  Scheduler Efficiency: " << formatPercent(efficiency * 100) << "\n";
    
    ss << "\n";
    return ss.str();
}

std::string Display::renderMemoryMetrics() {
    std::stringstream ss;
    
    ss << centerString("[ AWSC-PPC MEMORY METRICS ]", displayWidth) << "\n\n";
    
    SimulationStats stats = simulation->getStats();
    
    // Component 1: Working Set Tracking
    ss << "  === Working Set & Clock ==="  << "\n";
    ss << "  Memory Utilization:   " << formatPercent(stats.memoryUtilization * 100) << "\n";
    ss << "  Total Page Frames:    " << Config::TOTAL_PAGE_FRAMES << "\n";
    
    // Component 3: Prefetching & Component 4: Compression (placeholder metrics)
    ss << "  === Advanced Features ==="  << "\n";
    ss << "  Prefetch Mode:        Active\n";
    ss << "  Compression Mode:     Active\n";
    
    // Component 7: Thrashing Detection
    ss << "  === System Health ==="  << "\n";
    ss << "  Thrashing Threshold:  " << formatPercent(Config::THRASHING_THRESHOLD * 100) << "\n";
    ss << "  TLB Entries:          " << Config::TLB_SIZE << "\n";
    
    ss << "\n";
    return ss.str();
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string Display::formatTime(TimeUnit seconds) {
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << h << ":"
       << std::setw(2) << m << ":" << std::setw(2) << s;
    return ss.str();
}

std::string Display::formatCurrency(double amount) {
    std::stringstream ss;
    ss << "$" << std::fixed << std::setprecision(2) << amount;
    return ss.str();
}

std::string Display::formatPercent(double value) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << value << "%";
    return ss.str();
}

std::string Display::padString(const std::string& str, int width, char pad) {
    if (static_cast<int>(str.length()) >= width) {
        return str.substr(0, width);
    }
    return str + std::string(width - str.length(), pad);
}

std::string Display::centerString(const std::string& str, int width) {
    if (static_cast<int>(str.length()) >= width) return str;
    int padding = (width - str.length()) / 2;
    return std::string(padding, ' ') + str;
}

std::string Display::horizontalLine(int width, char ch) {
    return std::string(width, ch);
}

std::string Display::doubleLine(int width) {
    return std::string(width, '=');
}

std::string Display::flightStatusToString(FlightStatus status) {
    switch (status) {
        case FlightStatus::SCHEDULED: return "Scheduled";
        case FlightStatus::APPROACHING: return "Approaching";
        case FlightStatus::LANDING: return "Landing";
        case FlightStatus::LANDED: return "Landed";
        case FlightStatus::TAXIING_IN: return "Taxiing In";
        case FlightStatus::AT_GATE: return "At Gate";
        case FlightStatus::BOARDING: return "Boarding";
        case FlightStatus::PUSHBACK: return "Pushback";
        case FlightStatus::TAXIING_OUT: return "Taxiing Out";
        case FlightStatus::WAITING_TAKEOFF: return "Wait Takeoff";
        case FlightStatus::DEPARTING: return "Departing";
        case FlightStatus::DEPARTED: return "Departed";
        case FlightStatus::CANCELLED: return "Cancelled";
        case FlightStatus::DIVERTED: return "Diverted";
        case FlightStatus::GO_AROUND: return "Go Around";
        default: return "Unknown";
    }
}

std::string Display::runwayStatusToString(RunwayStatus status) {
    switch (status) {
        case RunwayStatus::AVAILABLE: return "Available";
        case RunwayStatus::OCCUPIED_LANDING: return "Landing";
        case RunwayStatus::OCCUPIED_TAKEOFF: return "Takeoff";
        case RunwayStatus::OCCUPIED_CROSSING: return "Crossing";
        case RunwayStatus::CLOSED_MAINTENANCE: return "Maintenance";
        case RunwayStatus::CLOSED_WEATHER: return "Weather";
        case RunwayStatus::CLOSED_EMERGENCY: return "Emergency";
        default: return "Unknown";
    }
}

std::string Display::weatherTypeToString(WeatherType type) {
    switch (type) {
        case WeatherType::CLEAR: return "Clear";
        case WeatherType::LIGHT_RAIN: return "Light Rain";
        case WeatherType::HEAVY_RAIN: return "Heavy Rain";
        case WeatherType::THUNDERSTORM: return "Thunderstorm";
        case WeatherType::FOG: return "Fog";
        case WeatherType::SNOW: return "Snow";
        case WeatherType::ICE: return "Ice";
        case WeatherType::HIGH_WIND: return "High Wind";
        case WeatherType::TORNADO_WARNING: return "TORNADO WARNING";
        default: return "Unknown";
    }
}

std::string Display::emergencyTypeToString(EmergencyType type) {
    switch (type) {
        case EmergencyType::NONE: return "None";
        case EmergencyType::MECHANICAL_FAILURE: return "Mechanical";
        case EmergencyType::MEDICAL_EMERGENCY: return "Medical";
        case EmergencyType::SECURITY_INCIDENT: return "Security";
        case EmergencyType::FUEL_SHORTAGE: return "Fuel";
        case EmergencyType::BIRD_STRIKE: return "Bird Strike";
        case EmergencyType::HYDRAULIC_FAILURE: return "Hydraulic";
        case EmergencyType::SUSPICIOUS_PACKAGE: return "Suspicious Pkg";
        case EmergencyType::UNRULY_PASSENGER: return "Unruly Pax";
        case EmergencyType::CYBER_THREAT: return "Cyber Threat";
        default: return "Unknown";
    }
}

std::string Display::aircraftTypeToString(AircraftType type) {
    switch (type) {
        case AircraftType::A380: return "A380";
        case AircraftType::B777: return "B777";
        case AircraftType::B737: return "B737";
        case AircraftType::A320: return "A320";
        case AircraftType::B747F: return "B747F";
        case AircraftType::B777F: return "B777F";
        case AircraftType::G650: return "G650";
        case AircraftType::FALCON_7X: return "F7X";
        case AircraftType::MEDICAL_EVAC: return "MEDEVAC";
        case AircraftType::DIVERSION: return "DIVERT";
        case AircraftType::FUEL_EMERGENCY: return "FUEL";
        default: return "UNK";
    }
}
