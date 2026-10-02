#include "AnalyticsAgent.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  AnalyticsAgent — AI reasoning layer over AnalyticsEngine outputs
// ─────────────────────────────────────────────────────────────────────────────

AnalyticsAgent::AnalyticsAgent(const AnalyticsEngine& engine,
                                const EnergyMeter&     meter,
                                double warnZScore,
                                double critZScore)
    : m_engine(engine), m_meter(meter),
      m_warnZ(warnZScore), m_critZ(critZScore) {}

// ── Generate Report ───────────────────────────────────────────────────────────
AnalyticsAgent::Report
AnalyticsAgent::generate(const EnergyMeter::Reading& latest,
                          double ratePerKwh,
                          double standingCharge,
                          const std::string& currency) const {
    Report rpt;
    rpt.meterId   = latest.meterId;
    rpt.timestamp = formatTime(latest.ts);
    rpt.latest    = latest;

    // ── Run analytics ──────────────────────────────────────────────────────
    rpt.bill    = m_engine.computeBillForecast(ratePerKwh, standingCharge, currency);
    rpt.trend   = m_engine.computeTrend(14);
    rpt.anomaly = m_engine.detectAnomaly(latest.intervalKwh, m_warnZ, m_critZ);

    // ── Build alerts ───────────────────────────────────────────────────────

    // 1. Bill alert
    double monthlyBill = rpt.bill.estimatedBill;
    if (monthlyBill > 2000.0) {
        rpt.alerts.push_back({AlertLevel::CRITICAL, "BILL",
            "Projected monthly bill " + currency + " " +
            std::to_string(static_cast<int>(monthlyBill)) +
            " exceeds budget. Reduce high-draw appliance usage immediately."});
    } else if (monthlyBill > 1200.0) {
        rpt.alerts.push_back({AlertLevel::WARNING, "BILL",
            "Projected monthly bill " + currency + " " +
            std::to_string(static_cast<int>(monthlyBill)) +
            " is above average. Monitor usage."});
    }

    // 2. Anomaly alert
    if (rpt.anomaly.detected) {
        AlertLevel lv = (std::abs(rpt.anomaly.zScore) >= m_critZ)
                        ? AlertLevel::CRITICAL : AlertLevel::WARNING;
        rpt.alerts.push_back({lv, "ANOMALY",
            "[APPLIANCE ANOMALY] " + rpt.anomaly.message});
    }

    // 3. Trend alert
    if (rpt.trend.significant) {
        if (rpt.trend.increasing) {
            char buf[128];
            snprintf(buf, sizeof(buf),
                "Upward consumption trend: +%.2f kWh/day over last 14 days. "
                "Usage is growing.", rpt.trend.slopeKwhPerDay);
            rpt.alerts.push_back({AlertLevel::WARNING, "TREND", buf});
        } else {
            char buf[128];
            snprintf(buf, sizeof(buf),
                "Consumption trend down: %.2f kWh/day. Great progress!",
                rpt.trend.slopeKwhPerDay);
            rpt.alerts.push_back({AlertLevel::INFO, "TREND", buf});
        }
    }

    // 4. Peak demand alert
    double peak = m_meter.getPeakPowerKW();
    if (peak > 5.0) {
        char buf[128];
        snprintf(buf, sizeof(buf),
            "Peak demand %.2f kW detected. High loads spike your tariff slab.", peak);
        rpt.alerts.push_back({AlertLevel::WARNING, "DEMAND", buf});
    }

    // ── Recommendations ────────────────────────────────────────────────────
    if (rpt.trend.increasing) {
        rpt.recommendations.push_back(
            "Consumption is trending upward. Audit high-draw appliances "
            "(AC, water heater, washing machine) for efficiency.");
    }
    if (rpt.bill.dailyKwh > 8.0) {
        rpt.recommendations.push_back(
            "Daily usage > 8 kWh. Set AC to 24-26 deg C and use 5-star rated appliances.");
    }
    if (peak > 3.5) {
        rpt.recommendations.push_back(
            "Avoid running AC + geyser + microwave simultaneously to reduce peak demand.");
    }
    if (rpt.bill.estimatedBill > 1000.0) {
        rpt.recommendations.push_back(
            "Consider shifting heavy loads (washing machine, dishwasher) to off-peak hours (10 PM - 6 AM).");
    }
    if (rpt.recommendations.empty()) {
        rpt.recommendations.push_back(
            "Consumption looks healthy. Keep it up!");
    }

    return rpt;
}

// ── Print Report ──────────────────────────────────────────────────────────────
void AnalyticsAgent::printReport(const Report& r) {
    const std::string sep(62, '=');
    const std::string dash(62, '-');

    std::cout << "\n" << sep << "\n";
    std::cout << "  SMART ENERGY METER  --  ANALYTICS REPORT\n";
    std::cout << sep << "\n";
    std::cout << "  Meter  : " << r.meterId   << "\n";
    std::cout << "  Time   : " << r.timestamp << "\n";
    std::cout << dash << "\n";

    // ── Live Snapshot ──────────────────────────────────────────────────────
    std::cout << "\n[LIVE SNAPSHOT]\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Current Power      : " << r.latest.powerKw     << " kW\n";
    std::cout << "  Total Energy       : " << r.latest.energyKwh   << " kWh\n";
    std::cout << "  Interval Energy    : " << r.latest.intervalKwh << " kWh\n";
    std::cout << "  Total Pulses       : " << r.latest.pulseCount  << "\n";
    std::cout << std::setprecision(2);
    std::cout << "  Cumulative Cost    : " << r.latest.cost
              << " " << r.bill.currency << "\n";

    // ── Bill Forecast ──────────────────────────────────────────────────────
    std::cout << "\n[NEXT BILL FORECAST]\n";
    std::cout << std::setprecision(2);
    std::cout << "  Today's avg usage  : " << r.bill.dailyKwh   << " kWh/day\n";
    std::cout << "  Projected monthly  : " << r.bill.monthlyKwh << " kWh\n";
    std::cout << "  Estimated bill     : " << r.bill.currency
              << " " << std::setprecision(0) << r.bill.estimatedBill << "\n";
    std::cout << "  Days remaining     : " << r.bill.daysRemaining << "\n";
    std::cout << "\n  >> \"Your current usage is "
              << std::setprecision(1) << r.bill.dailyKwh
              << " kWh/day. At this rate, estimated monthly\n"
              << "     consumption is " << std::setprecision(0)
              << r.bill.monthlyKwh << " kWh and your bill may be approximately "
              << r.bill.currency << " "
              << static_cast<int>(r.bill.estimatedBill) << ".\"\n";

    // ── Trend ──────────────────────────────────────────────────────────────
    std::cout << "\n[14-DAY TREND]\n";
    std::cout << std::setprecision(3);
    if (r.trend.significant) {
        std::cout << "  Slope    : " << (r.trend.slopeKwhPerDay > 0 ? "+" : "")
                  << r.trend.slopeKwhPerDay << " kWh/day  ("
                  << (r.trend.increasing ? "INCREASING" : "DECREASING") << ")\n";
        std::cout << "  R^2      : " << r.trend.r2 << "\n";
    } else {
        std::cout << "  Consumption is stable (slope < 0.05 kWh/day).\n";
    }

    // ── Alerts ─────────────────────────────────────────────────────────────
    if (!r.alerts.empty()) {
        std::cout << "\n[ALERTS]\n";
        for (auto& a : r.alerts) {
            std::cout << "  [" << levelStr(a.level) << "] ["
                      << a.category << "] " << a.message << "\n";
        }
    }

    // ── Recommendations ────────────────────────────────────────────────────
    if (!r.recommendations.empty()) {
        std::cout << "\n[RECOMMENDATIONS]\n";
        for (size_t i = 0; i < r.recommendations.size(); i++) {
            std::cout << "  " << (i + 1) << ". " << r.recommendations[i] << "\n";
        }
    }

    std::cout << sep << "\n";
}

// ── Helpers ───────────────────────────────────────────────────────────────────
std::string AnalyticsAgent::levelStr(AlertLevel l) {
    switch (l) {
        case AlertLevel::INFO:     return "INFO    ";
        case AlertLevel::WARNING:  return "WARNING ";
        case AlertLevel::CRITICAL: return "CRITICAL";
    }
    return "INFO    ";
}

std::string AnalyticsAgent::formatTime(std::time_t t) {
    char buf[64];
    struct tm* ltm = localtime(&t);
    if (!ltm) return "N/A";
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ltm);
    return std::string(buf);
}
