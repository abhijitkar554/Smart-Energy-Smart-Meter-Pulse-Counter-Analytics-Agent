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

// ── Thread-safe localtime helper ──────────────────────────────────────────────
static bool safeLocaltime(std::time_t t, struct tm& out) {
#ifdef _WIN32
    struct tm* p = localtime(&t);
    if (!p) return false;
    out = *p;
    return true;
#else
    return localtime_r(&t, &out) != nullptr;
#endif
}

bool NightWatchdog::isNightTime() const {
    std::time_t now = std::time(nullptr);
    struct tm lt;
    if (!safeLocaltime(now, lt)) return false;
    int h = lt.tm_hour;
    if (m_startHour > m_endHour)          // window wraps midnight e.g. 23→5
        return h >= m_startHour || h < m_endHour;
    return h >= m_startHour && h < m_endHour;
}

// ── Sum kWh that fell inside the night window over the last `nights` days ─────
double NightWatchdog::getNightKwhLast(int nights) const {
    std::time_t since = std::time(nullptr)
                        - static_cast<std::time_t>(nights) * 86400;
    auto readings = m_store.getReadingsSince(since);

    double total = 0.0;
    for (auto& r : readings) {
        struct tm lt;
        if (!safeLocaltime(r.ts, lt)) continue;
        int h = lt.tm_hour;
        bool inNight = (m_startHour > m_endHour)
                       ? (h >= m_startHour || h < m_endHour)
                       : (h >= m_startHour && h < m_endHour);
        if (inNight) total += r.intervalKwh;
    }
    return total;
}

NightWatchdog::NightReport NightWatchdog::evaluate() const {
    NightReport nr;
    nr.startHour = m_startHour;
    nr.endHour   = m_endHour;
    nr.threshold = m_threshold;

    double tonightKwh = getNightKwhLast(1);
    nr.consumedKwh    = tonightKwh;

    // Historical average — exclude tonight to avoid self-inflation
    // BUG FIX #5b: original averaged over 7 days including tonight,
    // masking abnormal nights.  Now we use the prior 6 nights only.
    double priorTotal = getNightKwhLast(7) - tonightKwh;
    double avgNight   = (priorTotal > 0.0) ? priorTotal / 6.0 : 0.0;
    nr.averageNightKwh = avgNight;

    if (avgNight > 0.001)
        nr.percentAboveAvg = ((tonightKwh - avgNight) / avgNight) * 100.0;

    // Infer appliances from approximate overnight power
    double approxPowerW = (tonightKwh * 1000.0) / 6.0;  // 6-hour window
    if (approxPowerW > 600) nr.suspects.push_back("Air Conditioner");
    if (approxPowerW > 250) nr.suspects.push_back("Water Heater / Geyser");
    if (approxPowerW > 100) nr.suspects.push_back("Refrigerator");
    if (approxPowerW > 50)  nr.suspects.push_back("Fans / Chargers / Set-top box");

    if (tonightKwh > m_threshold) {
        nr.alertTriggered = true;

        std::ostringstream oss;
        // BUG FIX #5: removed (char)0xF0 — garbled non-ASCII byte on Windows.
        // Use plain ASCII prefix instead; emoji can be added if terminal supports it.
        oss << "[NIGHT] Night Energy Alert: "
            << std::fixed << std::setprecision(2) << tonightKwh
            << " kWh consumed between "
            << m_startHour << ":00-" << m_endHour << ":00. ";

        if (nr.percentAboveAvg > 20.0)
            oss << "This is "
                << std::setprecision(0) << nr.percentAboveAvg
                << "% above your nightly average. ";

        oss << "You may have devices running unnecessarily.";
        nr.message = oss.str();
    }

    return nr;
}
