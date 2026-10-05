#pragma once
#include "threading_compat.h"
#include <atomic>
#include <vector>
#include <functional>
#include <cstdint>
#include <ctime>


class PulseCounter {
public:
    struct PulseEvent {
        std::time_t ts;
        uint64_t    cumulative;
    };

    explicit PulseCounter(int pulsesPerKwh = 1000);

   
    void onPulse();

   
    void addPulses(uint64_t n);

    
    uint64_t getTotalPulses()    const { return m_count.load(); }
    double   getTotalEnergyKwh() const;

   
    void     setBaseCount(uint64_t base) { m_base = base; }
    uint64_t getBaseCount()        const { return m_base; }

    
    void setCallback(std::function<void(uint64_t)> cb, uint64_t every);

    
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

   
    static const size_t          RING = 64;
    mutable std::mutex           m_eventMutex;
    std::vector<PulseEvent>      m_events;
};
