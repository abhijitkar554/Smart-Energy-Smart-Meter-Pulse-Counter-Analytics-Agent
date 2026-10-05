#pragma once
#include "DataStore.h"
#include <vector>
#include <string>


class AnalyticsEngine {
public:

    struct BillForecast {
        double dailyKwh{0};         
        double monthlyKwh{0};        
        double estimatedBill{0};    
        int    daysRemaining{0};
        std::string currency{"INR"};
    };

    
    struct AnomalyResult {
        bool   detected{false};
        double zScore{0};
        double currentKwh{0};
        double meanKwh{0};
        double stddevKwh{0};
        int    hourOfDay{-1};
        std::string message;
    };

    
    struct TrendResult {
        double slopeKwhPerDay{0};    
        double interceptKwh{0};
        double r2{0};                
        bool   increasing{false};
        bool   significant{false};   
    };

    
    struct HourlyPattern {
        double meanKwh[24]{};        
        double stddev[24]{};
        int    sampleCount[24]{};
    };

    explicit AnalyticsEngine(const DataStore& store);

    BillForecast  computeBillForecast(double ratePerKwh,
                                       double standingCharge,
                                       const std::string& currency = "INR") const;

    AnomalyResult detectAnomaly(double currentIntervalKwh,
                                 double warnZScore = 2.0,
                                 double critZScore = 3.0) const;

    TrendResult   computeTrend(int days = 14) const;

    HourlyPattern buildHourlyPattern(int days = 7) const;

    
    int computeEfficiencyScore(double todayKwh, double baselineKwh) const;

private:
    const DataStore& m_store;

    static double   linearRegression(const std::vector<double>& x,
                                     const std::vector<double>& y,
                                     double& slope, double& intercept);
};
