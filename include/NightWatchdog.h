#pragma once
#include "DataStore.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  NightWatchdog — detects unnecessary energy use during sleeping hours
//
//  🌙 Night Energy Alert:
//    "0.9 kWh consumed between 1 AM–5 AM.
//     You may have devices running unnecessarily."
// ─────────────────────────────────────────────────────────────────────────────
class NightWatchdog {
public:
    struct NightReport {
        bool   alertTriggered{false};
        double consumedKwh{0};
        int    startHour{23};
        int    endHour{5};
        double threshold{0.3};
        double averageNightKwh{0};     // historical average for comparison
        double percentAboveAvg{0};
        std::string message;
        std::vector<std::string> suspects; // likely appliances
    };

    NightWatchdog(const DataStore& store,
                  int    nightStartHour   = 23,
                  int    nightEndHour     = 5,
                  double thresholdKwh     = 0.3);

    NightReport evaluate() const;

    // Returns true if current local hour is inside the night window
    bool isNightTime() const;

private:
    double getNightKwhLast(int nights) const;

    const DataStore& m_store;
    int    m_startHour;
    int    m_endHour;
    double m_threshold;
};
