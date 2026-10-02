#include "AnalyticsAgent.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  AnalyticsAgent — AI reasoning layer over AnalyticsEngine outputs
//
//  BUG FIX #7: bill alert thresholds (warnBill, critBill) and peak demand
//  threshold (peakAlertKw) are now injected from Config rather than
//  hard-coded as magic numbers 1200 / 2000 / 5.0.
// ─────────────────────────────────────────────────────────────────────────────

AnalyticsAgent::AnalyticsAgent(const AnalyticsEngine& engine,
                                const EnergyMeter&     meter,
                                double warnZScore,
                                double critZScore,
                                double warnBillINR,
                                double critBillINR,
                                double peakAlertKw)
    : m_engine(engine), m_meter(meter),
      m_warnZ(warnZScore),  m_critZ(critZScore),
      m_warnBill(warnBillINR), m_critBill(critBillINR),
      m_peakAlertKw(peakAlertKw) {}

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

    rpt.bill    = m_engine.computeBillForecast(ratePerKwh, standingCharge, currency);
    rpt.trend   = m_engine.computeTrend(14);
    rpt.anomaly = m_engine.detectAnomaly(latest.intervalKwh, m_warnZ, m_critZ);

    // ── 1. Bill alert (thresholds from config via constructor) ─────────────
    double bill = rpt.bill.estimatedBill;
    if (bill > m_critBill) {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Projected monthly bill %s %.0f exceeds budget threshold (%.0f). "
            "Reduce high-draw appliance usage immediately.",
            currency.c_str(), bill, m_critBill);
        rpt.alerts.push_back({AlertLevel::CRITICAL, "BILL", buf});
    } else if (bill > m_warnBill) {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Projected monthly bill %s %.0f is above warning threshold (%.0f). "
            "Monitor usage.",
            currency.c_str(), bill, m_warnBill);
        rpt.alerts.push_back({AlertLevel::WARNING, "BILL", buf});
    }

    // ── 2. Anomaly alert ───────────────────────────────────────────────────
    if (rpt.anomaly.detected) {
        AlertLevel lv = (std::abs(rpt.anomaly.zScore) >= m_critZ)
                        ? AlertLevel::CRITICAL : AlertLevel::WARNING;
        rpt.alerts.push_back({lv, "ANOMALY",
            "[APPLIANCE ANOMALY] " + rpt.anomaly.message});
    }

    // ── 3. Trend alert ────────────────────────────────────────────────────
    if (rpt.trend.significant) {
        char buf[128];
        if (rpt.trend.increasing) {
            snprintf(buf, sizeof(buf),
                "Upward consumption trend: +%.2f kWh/day over last 14 days.",
                rpt.trend.slopeKwhPerDay);
            rpt.alerts.push_back({AlertLevel::WARNING, "TREND", buf});
        } else {
            snprintf(buf, sizeof(buf),
                "Consumption trending down: %.2f kWh/day. Great progress!",
                rpt.trend.slopeKwhPerDay);
            rpt.alerts.push_back({AlertLevel::INFO, "TREND", buf});
        }
    }

    // ── 4. Peak demand alert (threshold from config) ───────────────────────
    double peak = m_meter.getPeakPowerKW();
    if (peak > m_peakAlertKw) {
        char buf[128];
        snprintf(buf, sizeof(buf),
            "Peak demand %.2f kW detected (threshold %.1f kW). "
            "High loads spike your tariff slab.",
            peak, m_peakAlertKw);
        rpt.alerts.push_back({AlertLevel::WARNING, "DEMAND", buf});
    }

    // ── Recommendations ────────────────────────────────────────────────────
    if (rpt.trend.increasing)
        rpt.recommendations.push_back(
            "Consumption trending up. Audit AC, water heater, washing machine.");
    if (rpt.bill.dailyKwh > 8.0)
        rpt.recommendations.push_back(
            "Daily usage > 8 kWh. Set AC to 24-26 C and use 5-star appliances.");
    if (peak > 3.5)
        rpt.recommendations.push_back(
            "Avoid running AC + geyser + microwave simultaneously.");
    if (rpt.bill.estimatedBill > 1000.0)
        rpt.recommendations.push_back(
            "Shift heavy loads (washing machine) to off-peak hours (10 PM-6 AM).");
    if (rpt.recommendations.empty())
        rpt.recommendations.push_back("Consumption looks healthy. Keep it up!");

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

    std::cout << "\n[LIVE SNAPSHOT]\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Current Power      : " << r.latest.powerKw     << " kW\n";
    std::cout << "  Total Energy       : " << r.latest.energyKwh   << " kWh\n";
    std::cout << "  Interval Energy    : " << r.latest.intervalKwh << " kWh\n";
    std::cout << "  Total Pulses       : " << r.latest.pulseCount  << "\n";
    std::cout << std::setprecision(2);
    std::cout << "  Cumulative Cost    : " << r.latest.cost
              << " " << r.bill.currency << "\n";

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
              << "     consumption is "
              << std::setprecision(0) << r.bill.monthlyKwh
              << " kWh and your bill may be approximately "
              << r.bill.currency << " "
              << static_cast<int>(r.bill.estimatedBill) << ".\"\n";

    std::cout << "\n[14-DAY TREND]\n";
    std::cout << std::setprecision(3);
    if (r.trend.significant) {
        std::cout << "  Slope : "
                  << (r.trend.slopeKwhPerDay > 0 ? "+" : "")
                  << r.trend.slopeKwhPerDay << " kWh/day  ("
                  << (r.trend.increasing ? "INCREASING" : "DECREASING") << ")\n";
        std::cout << "  R^2   : " << r.trend.r2 << "\n";
    } else {
        std::cout << "  Consumption is stable (slope < 0.05 kWh/day).\n";
    }

    if (!r.alerts.empty()) {
        std::cout << "\n[ALERTS]\n";
        for (auto& a : r.alerts)
            std::cout << "  [" << levelStr(a.level) << "] ["
                      << a.category << "] " << a.message << "\n";
    }

    if (!r.recommendations.empty()) {
        std::cout << "\n[RECOMMENDATIONS]\n";
        for (size_t i = 0; i < r.recommendations.size(); i++)
            std::cout << "  " << (i + 1) << ". " << r.recommendations[i] << "\n";
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
#ifdef _WIN32
    struct tm* ltm = localtime(&t);
    if (!ltm) return "N/A";
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ltm);
#else
    struct tm ltbuf;
    struct tm* ltm = localtime_r(&t, &ltbuf);
    if (!ltm) return "N/A";
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ltm);
#endif
    return std::string(buf);
}
