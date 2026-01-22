/**
 * Smart Airport Operations Management System
 * main.cpp - Application Entry Point with ANSI Terminal UI
 * 
 * Uses ANSI escape codes for smooth terminal display (no external dependencies)
 * 
 * Compilation:
 *   g++ -std=c++17 -pthread -o airport *.cpp
 */

#include "simulation.h"
#include "display.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstring>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

// ANSI escape codes
namespace ANSI {
    const char* HOME = "\033[H";
    const char* CLEAR_LINE = "\033[2K";
    const char* RESET = "\033[0m";
    const char* BOLD = "\033[1m";
    const char* RED = "\033[31m";
    const char* GREEN = "\033[32m";
    const char* YELLOW = "\033[33m";
    const char* BLUE = "\033[34m";
    const char* MAGENTA = "\033[35m";
    const char* CYAN = "\033[36m";
    const char* WHITE = "\033[37m";
    const char* BRIGHT_GREEN = "\033[92m";
    const char* BRIGHT_YELLOW = "\033[93m";
    const char* BRIGHT_RED = "\033[91m";
    const char* HIDE_CURSOR = "\033[?25l";
    const char* SHOW_CURSOR = "\033[?25h";
}

// Fixed display width
const int WIDTH = 80;

// Global state
Simulation* globalSim = nullptr;
std::atomic<bool> shouldExit{false};
struct termios originalTermios;
bool termiosModified = false;

void restoreTerminal() {
    if (termiosModified) {
        tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
        std::cout << ANSI::SHOW_CURSOR << ANSI::RESET << std::flush;
    }
}

void signalHandler(int sig) {
    shouldExit = true;
    if (globalSim) globalSim->stop();
    restoreTerminal();
}

void setupTerminal() {
    tcgetattr(STDIN_FILENO, &originalTermios);
    termiosModified = true;
    struct termios raw = originalTermios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    std::cout << ANSI::HIDE_CURSOR << "\033[2J" << ANSI::HOME << std::flush;
}

int getKey() {
    char c;
    return (read(STDIN_FILENO, &c, 1) == 1) ? c : -1;
}

void printHelp() {
    std::cout << R"(
========================================
  Smart Airport Operations Management  
          HMFQ-PPRA + AWSC-PPC         
========================================

Usage: airport [options]

Options:
  -s <speed>   Speed multiplier (default: 1.0)
  -t <time>    Run for specified seconds
  -d           Demo mode
  -h           Show help

Controls:
  +/-  Speed up/down
  p    Pause/Resume
  q    Quit
)";
}

// Helper to create a fixed-width line
std::string line(const std::string& content) {
    std::string result = content;
    if (result.length() < WIDTH) {
        result += std::string(WIDTH - result.length(), ' ');
    } else if (result.length() > WIDTH) {
        result = result.substr(0, WIDTH);
    }
    return std::string(ANSI::CLEAR_LINE) + result + "\n";
}

std::string statusColor(FlightStatus status) {
    switch (status) {
        case FlightStatus::LANDED:
        case FlightStatus::AT_GATE:
        case FlightStatus::DEPARTED: return ANSI::GREEN;
        case FlightStatus::APPROACHING:
        case FlightStatus::WAITING_TAKEOFF: return ANSI::YELLOW;
        case FlightStatus::LANDING:
        case FlightStatus::DEPARTING: return ANSI::BRIGHT_RED;
        case FlightStatus::CANCELLED: return ANSI::RED;
        case FlightStatus::BOARDING: return ANSI::CYAN;
        default: return ANSI::WHITE;
    }
}

std::string statusStr(FlightStatus status) {
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
        default: return "Unknown";
    }
}

std::string typeStr(AircraftType type) {
    switch (type) {
        case AircraftType::A320: return "A320";
        case AircraftType::A380: return "A380";
        case AircraftType::B737: return "B737";
        case AircraftType::B777: return "B777";
        case AircraftType::B747F: return "B747F";
        case AircraftType::B777F: return "B777F";
        case AircraftType::G650: return "G650";
        case AircraftType::FALCON_7X: return "F7X";
        default: return "???";
    }
}

void render(Simulation* sim, double speed, bool paused) {
    std::cout << ANSI::HOME;  // Move to top-left
    
    // Header
    std::cout << ANSI::BOLD << ANSI::CYAN;
    std::cout << line("+==============================================================================+");
    std::cout << line("|           SMART AIRPORT OPERATIONS MANAGEMENT SYSTEM                         |");
    std::cout << line("|                   HMFQ-PPRA Scheduler | AWSC-PPC Memory                      |");
    std::cout << line("+==============================================================================+");
    std::cout << ANSI::RESET;
    
    // Status bar
    std::stringstream statusLine;
    statusLine << ANSI::MAGENTA << " Time: " << std::setw(5) << sim->getSimulationTime() << "s"
               << "  Speed: " << std::fixed << std::setprecision(1) << speed << "x"
               << "  [" << (paused ? "PAUSED" : "RUNNING") << "]"
               << "  Controls: [+/-] Speed  [p] Pause  [q] Quit" << ANSI::RESET;
    std::cout << line(statusLine.str());
    std::cout << line("");
    
    // Flight board header
    std::cout << ANSI::BOLD << ANSI::YELLOW;
    std::cout << line(" ========================= FLIGHT BOARD ========================================");
    std::cout << ANSI::RESET << ANSI::CYAN;
    std::cout << line(" Flight  Type   Status          Gate   Runway  Progress");
    std::cout << ANSI::RESET;
    
    // Flights - use FlightManager for current states, not DataManager copies
    FlightManager* fm = sim->getFlightManager();
    std::vector<Flight*> flights;
    if (fm) {
        flights = fm->getAllActiveFlights();
    }
    int shown = 0;
    for (Flight* fp : flights) {
        if (shown >= 8) break;
        if (!fp) continue;
        const Flight& f = *fp;
        
        // Calculate progress from FlightStatus (accurate representation)
        int progress = 0;
        switch (f.status) {
            case FlightStatus::SCHEDULED: progress = 0; break;
            case FlightStatus::APPROACHING: progress = 10; break;
            case FlightStatus::LANDING: progress = 20; break;
            case FlightStatus::LANDED: progress = 30; break;
            case FlightStatus::TAXIING_IN: progress = 40; break;
            case FlightStatus::AT_GATE: progress = 50; break;
            case FlightStatus::BOARDING: progress = 60; break;
            case FlightStatus::PUSHBACK: progress = 70; break;
            case FlightStatus::TAXIING_OUT: progress = 80; break;
            case FlightStatus::WAITING_TAKEOFF: progress = 85; break;
            case FlightStatus::DEPARTING: progress = 95; break;
            case FlightStatus::DEPARTED: progress = 100; break;
            default: progress = 0; break;
        }
        
        std::stringstream fs;
        fs << " " << std::left << std::setw(6) << f.flightNumber << std::setfill(' ');
        fs << "  " << std::left << std::setw(5) << typeStr(f.aircraftType);
        fs << "  " << statusColor(f.status) << std::left << std::setw(14) << statusStr(f.status) << ANSI::RESET;
        fs << "  " << std::setw(4) << (f.assignedGate >= 0 ? std::to_string(f.assignedGate) : "-");
        fs << "   " << std::setw(4) << (f.assignedRunway >= 0 ? std::to_string(f.assignedRunway) : "-");
        fs << "     " << std::setw(3) << progress << "%";
        std::cout << line(fs.str());
        shown++;
    }
    for (int i = shown; i < 8; i++) std::cout << line("");
    std::cout << line("");
    
    // Runways and Gates
    std::cout << ANSI::BOLD << ANSI::YELLOW;
    std::cout << line(" === RUNWAYS ===                                === GATES ===");
    std::cout << ANSI::RESET;
    
    std::vector<Runway> runways = sim->getRunwayManager()->getAllRunways();
    std::vector<Gate> gates = sim->getGateManager()->getAllGates();
    int occ = 0;
    for (const auto& g : gates) if (g.isOccupied) occ++;
    double gateUtil = gates.size() > 0 ? (double)occ / gates.size() * 100.0 : 0.0;
    
    for (size_t i = 0; i < 3; i++) {
        std::stringstream rs;
        if (i < runways.size()) {
            std::string st = "Available";
            const char* col = ANSI::GREEN;
            switch (runways[i].status) {
                case RunwayStatus::OCCUPIED_LANDING: st = "Landing"; col = ANSI::YELLOW; break;
                case RunwayStatus::OCCUPIED_TAKEOFF: st = "Takeoff"; col = ANSI::YELLOW; break;
                case RunwayStatus::CLOSED_WEATHER: st = "Closed"; col = ANSI::RED; break;
                case RunwayStatus::CLOSED_MAINTENANCE: st = "Maint."; col = ANSI::RED; break;
                default: break;
            }
            rs << " RWY " << runways[i].id << ": " << col << std::left << std::setw(10) << st << ANSI::RESET;
        } else {
            rs << std::string(20, ' ');
        }
        rs << "                                ";
        if (i == 0) rs << "Used: " << occ << "/" << gates.size() << " (" << std::fixed << std::setprecision(0) << gateUtil << "%)";
        else if (i == 1) {
            rs << "[";
            int bar = (int)(gateUtil / 5);
            for (int j = 0; j < bar; j++) rs << ANSI::GREEN << "#" << ANSI::RESET;
            for (int j = bar; j < 20; j++) rs << "-";
            rs << "]";
        }
        std::cout << line(rs.str());
    }
    std::cout << line("");
    
    // Resources and Weather
    std::cout << ANSI::BOLD << ANSI::YELLOW;
    std::cout << line(" === RESOURCES ===                              === WEATHER ===");
    std::cout << ANSI::RESET;
    
    DataManager* dm = sim->getDataManager();
    WeatherCondition w = dm->getCurrentWeather();
    std::string wStr = "Clear";
    const char* wCol = ANSI::GREEN;
    switch (w.type) {
        case WeatherType::THUNDERSTORM: wStr = "Thunderstorm"; wCol = ANSI::RED; break;
        case WeatherType::HEAVY_RAIN: wStr = "Heavy Rain"; wCol = ANSI::RED; break;
        case WeatherType::FOG: wStr = "Fog"; wCol = ANSI::YELLOW; break;
        case WeatherType::SNOW: wStr = "Snow"; wCol = ANSI::YELLOW; break;
        default: break;
    }
    
    std::stringstream r1, r2, r3, r4;
    r1 << " Fuel:    " << dm->getAvailableResourceCount(ResourceType::FUEL_TRUCK) << "/" << Config::NUM_FUEL_TRUCKS;
    r1 << "  Baggage: " << dm->getAvailableResourceCount(ResourceType::BAGGAGE_CART) << "/" << Config::NUM_BAGGAGE_CARTS;
    r1 << "                  Weather: " << wCol << wStr << ANSI::RESET;
    std::cout << line(r1.str());
    
    r2 << " Cater:   " << dm->getAvailableResourceCount(ResourceType::CATERING_TRUCK) << "/" << Config::NUM_CATERING_TRUCKS;
    r2 << "  Clean:   " << dm->getAvailableResourceCount(ResourceType::CLEANING_CREW) << "/" << Config::NUM_CLEANING_CREWS;
    r2 << "                  Wind: " << w.windSpeed << " kts @ " << w.windDirection << " deg";
    std::cout << line(r2.str());
    
    r3 << " Maint:   " << dm->getAvailableResourceCount(ResourceType::MAINTENANCE_TEAM) << "/" << Config::NUM_MAINTENANCE_TEAMS;
    r3 << "  De-ice:  " << dm->getAvailableResourceCount(ResourceType::DEICING_TRUCK) << "/" << Config::NUM_DEICING_TRUCKS;
    r3 << "                  Visibility: " << w.visibility << "m";
    std::cout << line(r3.str());
    std::cout << line("");
    
    // Stats and Financials
    std::cout << ANSI::BOLD << ANSI::YELLOW;
    std::cout << line(" === STATISTICS ===                             === FINANCIALS ===");
    std::cout << ANSI::RESET;
    
    SimulationStats st = sim->getStats();
    FinancialTracker* ft = sim->getFinancialTracker();
    double profit = ft ? ft->getProfit() : 0;
    
    std::stringstream s1, s2, s3;
    s1 << " Processed: " << std::setw(4) << st.totalFlightsProcessed;
    s1 << "  Active: " << std::setw(4) << flights.size();
    s1 << "                   Revenue:  $" << std::fixed << std::setprecision(0) << (ft ? ft->getTotalRevenue() : 0);
    std::cout << line(s1.str());
    
    s2 << " Avg Delay: " << std::setw(4) << st.averageDelay << " min";
    s2 << "                           Costs:    $" << std::fixed << std::setprecision(0) << (ft ? ft->getTotalCosts() : 0);
    std::cout << line(s2.str());
    
    s3 << " Emergencies: " << std::setw(2) << st.emergenciesHandled;
    s3 << "                             Profit:   ";
    if (profit >= 0) s3 << ANSI::GREEN << "$" << profit << ANSI::RESET;
    else s3 << ANSI::RED << "-$" << -profit << ANSI::RESET;
    std::cout << line(s3.str());
    std::cout << line("");
    
    // Algorithm metrics
    std::cout << ANSI::BOLD << ANSI::YELLOW;
    std::cout << line(" === HMFQ-PPRA SCHEDULER ===                    === AWSC-PPC MEMORY ===");
    std::cout << ANSI::RESET;
    
    SchedulerMetrics sm = sim->getScheduler()->getMetrics();
    std::stringstream a1, a2, a3;
    a1 << " Scheduled: " << sm.totalOperationsScheduled << "  Completed: " << sm.operationsCompleted;
    a1 << "              Utilization: " << std::fixed << std::setprecision(1) << (st.memoryUtilization * 100) << "%";
    std::cout << line(a1.str());
    
    a2 << " Promotions: " << sm.totalPromotions << "  Preemptions: " << sm.totalPreemptions;
    a2 << "            Page Frames: " << Config::TOTAL_PAGE_FRAMES;
    std::cout << line(a2.str());
    
    a3 << " Starvation Fixes: " << sm.starvationPreventions;
    
    // Get live memory metrics
    MemoryManager* mm = sim->getMemoryManager();
    if (mm) {
        MemoryMetrics memMetrics = mm->getMetrics();
        int tlbHitPercent = static_cast<int>(memMetrics.tlbHitRate * 100);
        bool thrashing = mm->isInThrashingMode();
        a3 << "                        TLB: " << tlbHitPercent << "%  Thrash: " << (thrashing ? "Yes" : "No");
    } else {
        a3 << "                        TLB: N/A  Thrash: N/A";
    }
    std::cout << line(a3.str());
    std::cout << line("");
    
    // Alerts
    std::vector<std::string> alerts = dm->getActiveEmergencies();
    if (!alerts.empty()) {
        std::cout << ANSI::BOLD << ANSI::RED;
        std::cout << line(" === ALERTS ===");
        std::cout << ANSI::RESET;
        for (size_t i = 0; i < std::min(alerts.size(), (size_t)2); i++)
            std::cout << line(" [!] " + alerts[i].substr(0, 70));
    } else {
        std::cout << ANSI::GREEN << line(" [OK] No active alerts") << ANSI::RESET;
    }
    
    std::cout << std::flush;
}

int main(int argc, char* argv[]) {
    double speed = 1.0;
    int runTime = 0;
    bool demoMode = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printHelp();
            return 0;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            speed = std::stod(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            runTime = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0) {
            demoMode = true;
        }
    }
    
    std::cout << "\n=== SMART AIRPORT OPERATIONS MANAGEMENT SYSTEM ===\n";
    std::cout << "    HMFQ-PPRA Scheduler | AWSC-PPC Memory Manager\n\n";
    std::cout << "Initializing...\n";
    
    Simulation simulation;
    globalSim = &simulation;
    
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    if (!simulation.initialize()) {
        std::cerr << "ERROR: Failed to initialize\n";
        return 1;
    }
    
    std::cout << "  Runways: " << Config::NUM_RUNWAYS << "\n";
    std::cout << "  Gates: " << Config::TOTAL_GATES << "\n";
    std::cout << "  Initial Flights: " << Config::INITIAL_FLIGHTS << "\n\n";
    
    Display display(&simulation);
    simulation.setDisplay(&display);
    simulation.setSpeed(speed);
    
    std::cout << "Starting at " << speed << "x speed...\n";
    std::cout << "Controls: +/- Speed, p Pause, q Quit\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    simulation.start();
    
    // Demo mode
    if (demoMode) {
        pthread_t dt;
        pthread_create(&dt, nullptr, [](void* arg) -> void* {
            Simulation* s = static_cast<Simulation*>(arg);
            std::this_thread::sleep_for(std::chrono::seconds(30));
            s->triggerWeatherEvent(WeatherType::THUNDERSTORM, 600);
            return nullptr;
        }, &simulation);
        pthread_detach(dt);
    }
    
    setupTerminal();
    
    bool paused = false;
    auto startTime = std::chrono::steady_clock::now();
    
    while (simulation.isRunning() && !shouldExit) {
        if (runTime > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= runTime) break;
        }
        
        int ch = getKey();
        switch (ch) {
            case '+': case '=': speed = std::min(speed + 0.5, 10.0); simulation.setSpeed(speed); break;
            case '-': case '_': speed = std::max(speed - 0.5, 0.5); simulation.setSpeed(speed); break;
            case 'p': case 'P':
                paused = !paused;
                if (paused) simulation.pause(); else simulation.resume();
                break;
            case 'q': case 'Q': shouldExit = true; break;
        }
        
        render(&simulation, speed, paused);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    restoreTerminal();
    simulation.stop();
    
    // Beautiful exit message with colors
    SimulationStats stats = simulation.getStats();
    FinancialTracker* ft = simulation.getFinancialTracker();
    SchedulerMetrics sm = simulation.getScheduler()->getMetrics();
    
    std::cout << "\n";
    std::cout << ANSI::BOLD << ANSI::CYAN;
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                     SIMULATION COMPLETE - FINAL REPORT                       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << ANSI::RESET << "\n";
    
    std::cout << ANSI::BOLD << ANSI::YELLOW << " ═══ FLIGHT OPERATIONS ═══" << ANSI::RESET << "\n";
    std::cout << ANSI::GREEN << " ✓ " << ANSI::RESET << "Flights Processed:    " << stats.totalFlightsProcessed << "\n";
    std::cout << ANSI::GREEN << " ✓ " << ANSI::RESET << "Average Delay:        " << stats.averageDelay << " minutes\n";
    std::cout << ANSI::GREEN << " ✓ " << ANSI::RESET << "Emergencies Handled:  " << stats.emergenciesHandled << "\n\n";
    
    std::cout << ANSI::BOLD << ANSI::YELLOW << " ═══ FINANCIAL SUMMARY ═══" << ANSI::RESET << "\n";
    if (ft) {
        double profit = ft->getProfit();
        std::cout << ANSI::GREEN << " ✓ " << ANSI::RESET << "Revenue:              $" << std::fixed << std::setprecision(2) << ft->getTotalRevenue() << "\n";
        std::cout << ANSI::GREEN << " ✓ " << ANSI::RESET << "Operating Costs:      $" << ft->getTotalCosts() << "\n";
        if (profit >= 0) {
            std::cout << ANSI::GREEN << " ✓ Net Profit:          $" << profit << " (PROFIT)" << ANSI::RESET << "\n\n";
        } else {
            std::cout << ANSI::RED << " ✗ Net Loss:            -$" << -profit << " (LOSS)" << ANSI::RESET << "\n\n";
        }
    }
    
    std::cout << ANSI::BOLD << ANSI::YELLOW << " ═══ HMFQ-PPRA SCHEDULER ═══" << ANSI::RESET << "\n";
    std::cout << ANSI::MAGENTA << " ★ " << ANSI::RESET << "Operations Scheduled: " << sm.totalOperationsScheduled << "\n";
    std::cout << ANSI::MAGENTA << " ★ " << ANSI::RESET << "Operations Completed: " << sm.operationsCompleted << "\n";
    std::cout << ANSI::MAGENTA << " ★ " << ANSI::RESET << "Queue Promotions:     " << sm.totalPromotions << "\n";
    std::cout << ANSI::MAGENTA << " ★ " << ANSI::RESET << "Preemptions:          " << sm.totalPreemptions << "\n";
    std::cout << ANSI::MAGENTA << " ★ " << ANSI::RESET << "Starvation Prevented: " << sm.starvationPreventions << "\n\n";
    
    std::cout << ANSI::BOLD << ANSI::YELLOW << " ═══ AWSC-PPC MEMORY ═══" << ANSI::RESET << "\n";
    std::cout << ANSI::MAGENTA << " ★ " << ANSI::RESET << "Page Frames:          " << Config::TOTAL_PAGE_FRAMES << "\n";
    std::cout << ANSI::MAGENTA << " ★ " << ANSI::RESET << "TLB Size:             " << Config::TLB_SIZE << " entries\n";
    std::cout << ANSI::MAGENTA << " ★ " << ANSI::RESET << "Thrashing Threshold:  " << (int)(Config::THRASHING_THRESHOLD * 100) << "%\n\n";
    
    std::cout << ANSI::BOLD << ANSI::CYAN;
    std::cout << "══════════════════════════════════════════════════════════════════════════════\n";
    std::cout << "  Smart Airport Operations Management System - Thank you for running!\n";
    std::cout << "  Implementing: HMFQ-PPRA Scheduler | AWSC-PPC Memory Manager\n";
    std::cout << "══════════════════════════════════════════════════════════════════════════════\n";
    std::cout << ANSI::RESET << "\n";
    
    return 0;
}
