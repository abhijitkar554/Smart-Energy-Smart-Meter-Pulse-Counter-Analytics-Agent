#pragma once
#include "threading_compat.h"
#include <atomic>
#include <vector>
#include <functional>
#include <cstdint>
#include <ctime>

// ─────────────────────────────────────────────────────────────────────────────
//  PulseCounter — thread-safe pulse accumulator
//
//  Hardware pulse -> onPulse() -> atomic m_count
//  Every N pulses the optional callback fires (used by EnergyMeter).
// ─────────────────────────────────────────────────────────────────────────────
class PulseCounter {
public:
    struct PulseEvent {
        std::time_t ts;
        uint64_t    cumulative;
    };

    explicit PulseCounter(int pulsesPerKwh = 1000);

    // Called from ISR / simulator thread -- must be lock-free path
    void onPulse();

    // Bulk add (used by simulator for speed > real-time)
    void addPulses(uint64_t n);

    // Getters
    uint64_t getTotalPulses()    const { return m_count.load(); }
    double   getTotalEnergyKwh() const;

    // Set a base count (restored from DB on startup)
    void     setBaseCount(uint64_t base) { m_base = base; }
    uint64_t getBaseCount()        const { return m_base; }

    // Register callback fired every `every` pulses
    void setCallback(std::function<void(uint64_t)> cb, uint64_t every);

    // Ring-buffer of last 64 pulse events
    std::vector<PulseEvent> getRecentEvents(size_t n = 64) const;

    int pulsesPerKwh() const { return m_pulsesPerKwh; }

private:
    void recordEvent();

    const int                    m_pulsesPerKwh;
    std::atomic<uint64_t>        m_count{0};
    uint64_t                     m_base{0};

    std::function<void(uint64_t)> m_callback;
    uint64_t                      m_callbackEvery{0};
    std::atomic<uint64_t>         m_lastCbAt{0};

    // Event ring buffer
    static const size_t          RING = 64;
    mutable std::mutex           m_eventMutex;
    std::vector<PulseEvent>      m_events;
};
