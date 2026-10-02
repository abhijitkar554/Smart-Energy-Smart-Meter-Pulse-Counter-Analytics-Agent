#pragma once
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  ConfigReader — parses data/config.json (no external JSON library)
// ─────────────────────────────────────────────────────────────────────────────
struct Config {
    // Meter identity
    std::string meterId          = "METER-001";
    int         pulsesPerKwh     = 1000;

    // Tariff (Indian Rupee defaults)
    double      ratePerKwh       = 7.00;   // INR/kWh
    double      standingCharge   = 50.0;   // INR/month fixed
    std::string currency         = "INR";

    // Reading schedule
    int         readingIntervalSec  = 60;
    int         reportIntervalSec   = 30;
    int         runDurationSec      = 0;   // 0 = run forever

    // Alert thresholds
    double      costAlertThreshold  = 1500.0; // INR
    double      anomalyWarnZScore   = 2.0;
    double      anomalyCritZScore   = 3.0;

    // Night watchdog
    int         nightStartHour   = 23;  // 11 PM
    int         nightEndHour     = 5;   // 5  AM
    double      nightWasteThresholdKwh = 0.3; // alert if >0.3 kWh in night window

    // Simulator
    std::string simProfile          = "DIURNAL"; // DIURNAL | CONSTANT | RANDOM
    double      simBasePowerKw      = 1.5;
    double      simPeakMultiplier   = 4.0;
    double      simSpeedMultiplier  = 120.0;

    // Database
    std::string dbPath = "data/energy.db";
};

class ConfigReader {
public:
    // Returns true on success; fills cfg. On failure returns false and cfg
    // contains safe defaults (Indian residential tariff).
    static bool load(const std::string& path, Config& cfg);
};
