#pragma once
#include <string>


struct Config {
    
    std::string meterId          = "METER-001";
    int         pulsesPerKwh     = 1000;

    
    double      ratePerKwh       = 7.00;   
    double      standingCharge   = 50.0;   
    std::string currency         = "INR";

   
    int         readingIntervalSec  = 60;
    int         reportIntervalSec   = 30;
    int         runDurationSec      = 0;   

    
    double      costAlertThreshold  = 1500.0; 
    double      anomalyWarnZScore   = 2.0;
    double      anomalyCritZScore   = 3.0;

   
    int         nightStartHour   = 23;  
    int         nightEndHour     = 5;  
    double      nightWasteThresholdKwh = 0.3; 
    
    std::string simProfile          = "DIURNAL"; 
    double      simBasePowerKw      = 1.5;
    double      simPeakMultiplier   = 4.0;
    double      simSpeedMultiplier  = 120.0;

    
    std::string dbPath = "data/energy.db";
};

class ConfigReader {
public:
   
    static bool load(const std::string& path, Config& cfg);
};
