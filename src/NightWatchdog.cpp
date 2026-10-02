#include "NightWatchdog.h"
#include <ctime>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
//  NightWatchdog — detects electricity waste during sleeping hours
// ─────────────────────────────────────────────────────────────────────────────

NightWatchdog::NightWatchdog(const DataStore& store,
                              int    nightStartHour,
                              int    nightEndHour,
                              double thresholdKwh)
    : m_store(store),
      m_startHour(nightStartHour),
      m_endHour(nightEndHour),
      m_threshold(thresholdKwh) {}

bool NightWatchdog::isNightTime() const {
    std::time_t now = std::time(nullptr);
    struct tm*  ltm = localtime(&now);
    if (!ltm) return false;
    int h = ltm->tm_hour;
    // Handle window that wraps around midnight: e.g. 23->5
    if (m_startHour > m_endHour)
        return h >= m_startHour || h < m_endHour;
    return h >= m_startHour && h < m_endHour;
}

// Get sum of night-window kWh for the last `nights` nights
double NightWatchdog::getNightKwhLast(int nights) const {
    // We grab readings for the past `nights` days and filter by hour
    std::time_t since = std::time(nullptr) - static_cast<std::time_t>(nights) * 86400;
    auto readings = m_store.getReadingsSince(since);

    double total = 0.0;
    for (auto& r : readings) {
        struct tm* ltm = localtime(&r.ts);
        if (!ltm) continue;
        int h = ltm->tm_hour;
        bool inNight = false;
        if (m_startHour > m_endHour)
            inNight = (h >= m_startHour || h < m_endHour);
        else
            inNight = (h >= m_startHour && h < m_endHour);

        if (inNight) total += r.intervalKwh;
    }
    return total;
}

NightWatchdog::NightReport NightWatchdog::evaluate() const {
    NightReport nr;
    nr.startHour  = m_startHour;
    nr.endHour    = m_endHour;
    nr.threshold  = m_threshold;

    // Tonight's night consumption (last ~8 hours of night window)
    // Use last 1-day window filtered by night hours
    double tonightKwh   = getNightKwhLast(1);
    nr.consumedKwh      = tonightKwh;

    // Historical average for comparison (last 7 nights)
    double sevenNightTotal = getNightKwhLast(7);
    double avgNight = sevenNightTotal / 7.0;
    nr.averageNightKwh = avgNight;

    if (avgNight > 0.001) {
        nr.percentAboveAvg = ((tonightKwh - avgNight) / avgNight) * 100.0;
    }

    // Build suspects list based on power level
    double approxPowerW = (tonightKwh * 1000.0) / 6.0; // ~6-hour night window
    if (approxPowerW > 600) nr.suspects.push_back("Air Conditioner");
    if (approxPowerW > 250) nr.suspects.push_back("Water Heater / Geyser");
    if (approxPowerW > 100) nr.suspects.push_back("Refrigerator (normal)");
    if (approxPowerW > 50)  nr.suspects.push_back("Fans / Chargers / Set-top box");

    // Trigger alert?
    if (tonightKwh > m_threshold) {
        nr.alertTriggered = true;

        std::ostringstream oss;
        oss << "Night Energy Alert: "
            << std::fixed << std::setprecision(2) << tonightKwh
            << " kWh consumed between "
            << m_startHour << ":00 - " << m_endHour << ":00. ";

        if (nr.percentAboveAvg > 20.0) {
            oss << "This is " << std::setprecision(0)
                << nr.percentAboveAvg << "% above your 7-night average. ";
        }

        oss << "You may have devices running unnecessarily.";
        nr.message = oss.str();
    }

    return nr;
}

// ── Static print helper ───────────────────────────────────────────────────────
void printNightReport(const NightWatchdog::NightReport& nr) {
    if (!nr.alertTriggered) return;

    std::cout << "\n";
    std::cout << "  [NIGHT WATCHDOG]\n";
    std::cout << "  " << (char)0xF0 << " Night Energy Alert\n"; // moon emoji fallback
    std::cout << "  " << nr.message << "\n";
    if (!nr.suspects.empty()) {
        std::cout << "  Possible culprits:\n";
        for (auto& s : nr.suspects) {
            std::cout << "    - " << s << "\n";
        }
    }
    std::cout << "  Historical avg (7 nights): "
              << std::fixed << std::setprecision(3)
              << nr.averageNightKwh << " kWh\n";
}
