#pragma once
#include "DataStore.h"
#include <vector>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  AnalyticsEngine — statistics, trend, bill forecast, anomaly
// ─────────────────────────────────────────────────────────────────────────────
class AnalyticsEngine {
public:
    // ── Bill Forecast ──────────────────────────────────────────────────────
    struct BillForecast {
        double dailyKwh{0};          // today's average usage so far
        double monthlyKwh{0};        // projected monthly kWh
        double estimatedBill{0};     // projected bill in INR
        int    daysRemaining{0};
        std::string currency{"INR"};
    };

    // ── Anomaly ────────────────────────────────────────────────────────────
    struct AnomalyResult {
        bool   detected{false};
        double zScore{0};
        double currentKwh{0};
        double meanKwh{0};
        double stddevKwh{0};
        int    hourOfDay{-1};
        std::string message;
    };

    // ── Trend (linear regression over daily totals) ────────────────────────
    struct TrendResult {
        double slopeKwhPerDay{0};    // +ve = growing, -ve = shrinking
        double interceptKwh{0};
        double r2{0};                // goodness of fit [0,1]
        bool   increasing{false};
        bool   significant{false};   // |slope| > 0.1 kWh/day
    };

    // ── Hourly Pattern ─────────────────────────────────────────────────────
    struct HourlyPattern {
        double meanKwh[24]{};        // average kWh per hour bucket
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

    // Returns 0-100 efficiency score vs historical baseline
    int computeEfficiencyScore(double todayKwh, double baselineKwh) const;

private:
    const DataStore& m_store;

    static double   linearRegression(const std::vector<double>& x,
                                     const std::vector<double>& y,
                                     double& slope, double& intercept);
};
