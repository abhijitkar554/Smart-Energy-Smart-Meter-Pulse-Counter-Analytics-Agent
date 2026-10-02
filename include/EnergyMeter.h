#pragma once
#include "PulseCounter.h"
#include "threading_compat.h"
#include <chrono>
#include <vector>
#include <string>
#include <ctime>

// ─────────────────────────────────────────────────────────────────────────────
//  EnergyMeter — tariff, interval readings, peak/min demand tracking
// ─────────────────────────────────────────────────────────────────────────────
class EnergyMeter {
public:
    struct TariffConfig {
        double      ratePerKwh     = 7.00;    // INR/kWh
        double      standingCharge = 50.0;    // INR/month fixed
        std::string currency       = "INR";
    };

    struct Reading {
        std::string meterId;
        std::time_t ts{0};
        double      energyKwh{0};     // cumulative kWh
        double      intervalKwh{0};   // delta since last reading
        double      powerKw{0};       // average power over interval
        double      cost{0};          // cumulative cost
        uint64_t    pulseCount{0};
    };

    EnergyMeter(PulseCounter& counter, const std::string& meterId);

    void         setTariff(const TariffConfig& t);
    TariffConfig getTariff() const;

    // Snap a reading (thread-safe). Only call once per intended reading point.
    // Returns the last reading without advancing if called twice in same second.
    Reading takeReading();

    // Peek the most recent reading without taking a new one
    Reading lastReading() const;

    // Drain all pending readings (for DataStore batch insert)
    std::vector<Reading> drainPendingReadings();

    double getTotalCost()   const;
    double getPeakPowerKW() const;
    double getMinPowerKW()  const;
    void   resetPeakMin();

    const std::string& getMeterId() const { return m_meterId; }

private:
    PulseCounter&      m_counter;
    std::string        m_meterId;
    TariffConfig       m_tariff;
    mutable std::mutex m_readMutex;

    double      m_prevEnergyKwh{0};
    // Use steady_clock for sub-second interval precision
    std::chrono::steady_clock::time_point m_prevTime;
    bool        m_firstReading{true};

    double      m_totalCost{0};
    double      m_peakKw{0};
    double      m_minKw{1e18};

    Reading              m_lastReading;
    std::vector<Reading> m_pending;
};
