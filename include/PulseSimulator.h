#pragma once
#include "PulseCounter.h"
#include "threading_compat.h"
#include <atomic>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  PulseSimulator — background thread that injects pulses into PulseCounter
//
//  Profiles:
//    CONSTANT  -- flat load (good for unit verification)
//    DIURNAL   -- realistic Indian household: morning peak 6-9 AM,
//                 evening peak 6-10 PM, low overnight
//    RANDOM    -- Gaussian random walk around base load
// ─────────────────────────────────────────────────────────────────────────────
class PulseSimulator {
public:
    enum class Profile { CONSTANT, DIURNAL, RANDOM };

    PulseSimulator(PulseCounter& counter,
                   Profile       profile         = Profile::DIURNAL,
                   double        basePowerKw      = 1.5,
                   double        peakMultiplier   = 4.0,
                   double        speedMultiplier  = 120.0);

    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }

    static Profile fromString(const std::string& s);

private:
    void   runLoop();
    double powerAtSimTime(double simHour) const;

    PulseCounter&      m_counter;
    Profile            m_profile;
    double             m_basePowerKw;
    double             m_peakMult;
    double             m_speedMult;

    std::thread        m_thread;
    std::atomic<bool>  m_running{false};
};
