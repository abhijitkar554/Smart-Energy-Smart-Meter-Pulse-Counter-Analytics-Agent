#pragma once
#include "AnalyticsEngine.h"
#include "EnergyMeter.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  AnalyticsAgent — generates alerts, recommendations, formatted report
//
//  This is the "AI agent" layer: it holds domain knowledge rules on top of the
//  numerical outputs from AnalyticsEngine and produces human-readable output.
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
        std::string              meterId;
        std::string              timestamp;
        EnergyMeter::Reading     latest;
        AnalyticsEngine::BillForecast   bill;
        AnalyticsEngine::TrendResult    trend;
        AnalyticsEngine::AnomalyResult  anomaly;
        std::vector<Alert>       alerts;
        std::vector<std::string> recommendations;
    };

    AnalyticsAgent(const AnalyticsEngine& engine,
                   const EnergyMeter&     meter,
                   double warnZScore = 2.0,
                   double critZScore = 3.0);

    Report generate(const EnergyMeter::Reading& latest,
                    double ratePerKwh,
                    double standingCharge,
                    const std::string& currency = "INR") const;

    // Prints the report to stdout with box-drawing borders
    static void printReport(const Report& r);

private:
    const AnalyticsEngine& m_engine;
    const EnergyMeter&     m_meter;
    double m_warnZ;
    double m_critZ;

    static std::string levelStr(AlertLevel l);
    static std::string formatTime(std::time_t t);
};
