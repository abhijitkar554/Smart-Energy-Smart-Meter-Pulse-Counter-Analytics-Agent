#pragma once
#include "AnalyticsEngine.h"
#include "EnergyMeter.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  AnalyticsAgent — AI reasoning layer: alerts, recommendations, report
//
//  BUG FIX #7: cost_alert_threshold and peak_alert_kw are now constructor
//  parameters so they come from config rather than being hard-coded.
// ─────────────────────────────────────────────────────────────────────────────
class AnalyticsAgent {
public:
    enum class AlertLevel { INFO, WARNING, CRITICAL };

    struct Alert {
        AlertLevel  level;
        std::string category;   // ANOMALY | TREND | BILL | DEMAND | NIGHT
        std::string message;
    };

    struct Report {
        std::string                      meterId;
        std::string                      timestamp;
        EnergyMeter::Reading             latest;
        AnalyticsEngine::BillForecast    bill;
        AnalyticsEngine::TrendResult     trend;
        AnalyticsEngine::AnomalyResult   anomaly;
        std::vector<Alert>               alerts;
        std::vector<std::string>         recommendations;
    };

    // warnBill / critBill come from cfg.costAlertThreshold (see main.cpp)
    AnalyticsAgent(const AnalyticsEngine& engine,
                   const EnergyMeter&     meter,
                   double warnZScore  = 2.0,
                   double critZScore  = 3.0,
                   double warnBillINR = 1200.0,
                   double critBillINR = 2000.0,
                   double peakAlertKw = 5.0);

    Report generate(const EnergyMeter::Reading& latest,
                    double ratePerKwh,
                    double standingCharge,
                    const std::string& currency = "INR") const;

    static void printReport(const Report& r);

private:
    const AnalyticsEngine& m_engine;
    const EnergyMeter&     m_meter;
    double m_warnZ;
    double m_critZ;
    double m_warnBill;   // INR — WARNING threshold
    double m_critBill;   // INR — CRITICAL threshold
    double m_peakAlertKw;

    static std::string levelStr(AlertLevel l);
    static std::string formatTime(std::time_t t);
};
