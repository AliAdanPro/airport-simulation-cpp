/**
 * Smart Airport Operations Management System
 * display.h - Text-Based Display Interface
 * 
 * Formatted text output for simulation status:
 * - Real-time dashboard
 * - Flight information display
 * - Resource status
 * - Crisis alerts
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"
#include "simulation.h"
#include <string>
#include <vector>
#include <sstream>

/**
 * Display Manager
 * Handles all text-based output formatting
 */
class Display {
public:
    Display(Simulation* sim);
    
    // Render full dashboard
    std::string renderDashboard();
    
    // Render individual sections
    std::string renderHeader();
    std::string renderFlightBoard();
    std::string renderRunwayStatus();
    std::string renderGateStatus();
    std::string renderResourceStatus();
    std::string renderCrisisAlerts();
    std::string renderStatistics();
    std::string renderFinancials();
    std::string renderSchedulerMetrics();
    std::string renderMemoryMetrics();
    
    // Utilities
    std::string formatTime(TimeUnit seconds);
    std::string formatCurrency(double amount);
    std::string formatPercent(double value);
    std::string padString(const std::string& str, int width, char pad = ' ');
    std::string centerString(const std::string& str, int width);
    
    // Separator lines
    std::string horizontalLine(int width, char ch = '-');
    std::string doubleLine(int width);
    
private:
    Simulation* simulation;
    int displayWidth = 100;
    
    // Status to string conversions
    std::string flightStatusToString(FlightStatus status);
    std::string runwayStatusToString(RunwayStatus status);
    std::string weatherTypeToString(WeatherType type);
    std::string emergencyTypeToString(EmergencyType type);
    std::string aircraftTypeToString(AircraftType type);
};

#endif // DISPLAY_H
