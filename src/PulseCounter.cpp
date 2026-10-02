#include "PulseCounter.h"
#include <ctime>

PulseCounter::PulseCounter(int pulsesPerKwh)
    : m_pulsesPerKwh(pulsesPerKwh) {
    m_events.reserve(RING);
}

void PulseCounter::onPulse() {
    uint64_t total = ++m_count;
    recordEvent();

    // Fire callback if registered and threshold reached
    if (m_callbackEvery > 0 && m_callback) {
        uint64_t prev = m_lastCbAt.load();
        if (total - prev >= m_callbackEvery) {
            m_lastCbAt.store(total);
            m_callback(total);
        }
    }
}

void PulseCounter::addPulses(uint64_t n) {
    if (n == 0) return;
    uint64_t total = (m_count += n);

    // Record a single event for the batch
    {
        std::lock_guard<std::mutex> lk(m_eventMutex);
        PulseEvent ev;
        ev.ts         = std::time(nullptr);
        ev.cumulative = total;
        if (m_events.size() >= RING) m_events.erase(m_events.begin());
        m_events.push_back(ev);
    }

    if (m_callbackEvery > 0 && m_callback) {
        uint64_t prev = m_lastCbAt.load();
        if (total - prev >= m_callbackEvery) {
            m_lastCbAt.store(total);
            m_callback(total);
        }
    }
}

double PulseCounter::getTotalEnergyKwh() const {
    return static_cast<double>(m_count.load()) /
           static_cast<double>(m_pulsesPerKwh);
}

void PulseCounter::setCallback(std::function<void(uint64_t)> cb, uint64_t every) {
    m_callback     = std::move(cb);
    m_callbackEvery = every;
    m_lastCbAt.store(m_count.load());
}

std::vector<PulseCounter::PulseEvent>
PulseCounter::getRecentEvents(size_t n) const {
    std::lock_guard<std::mutex> lk(m_eventMutex);
    if (m_events.size() <= n) return m_events;
    return std::vector<PulseEvent>(
        m_events.end() - static_cast<std::ptrdiff_t>(n),
        m_events.end());
}

void PulseCounter::recordEvent() {
    std::lock_guard<std::mutex> lk(m_eventMutex);
    PulseEvent ev;
    ev.ts         = std::time(nullptr);
    ev.cumulative = m_count.load();
    if (m_events.size() >= RING) m_events.erase(m_events.begin());
    m_events.push_back(ev);
}
