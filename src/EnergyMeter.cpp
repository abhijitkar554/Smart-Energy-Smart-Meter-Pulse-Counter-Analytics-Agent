#include "EnergyMeter.h"
#include <algorithm>
#include <ctime>

// ─────────────────────────────────────────────────────────────────────────────
//  EnergyMeter — fixes:
//    1. Use steady_clock (sub-millisecond) for dt instead of time_t (1s res)
//    2. Guard against zero-interval double-calls: return last reading if
//       called within 100 ms of the previous call
//    3. Never let powerKw blow up — clamp to realistic household max (50 kW)
// ─────────────────────────────────────────────────────────────────────────────

EnergyMeter::EnergyMeter(PulseCounter& counter, const std::string& meterId)
    : m_counter(counter), m_meterId(meterId) {
    m_prevTime = std::chrono::steady_clock::now();
    m_lastReading.meterId = meterId;
}

void EnergyMeter::setTariff(const TariffConfig& t) {
    std::lock_guard<std::mutex> lk(m_readMutex);
    m_tariff = t;
}

EnergyMeter::TariffConfig EnergyMeter::getTariff() const {
    std::lock_guard<std::mutex> lk(m_readMutex);
    return m_tariff;
}

double EnergyMeter::getTotalCost() const {
    std::lock_guard<std::mutex> lk(m_readMutex);
    return m_totalCost;
}

double EnergyMeter::getPeakPowerKW() const {
    std::lock_guard<std::mutex> lk(m_readMutex);
    return m_peakKw;
}

double EnergyMeter::getMinPowerKW() const {
    std::lock_guard<std::mutex> lk(m_readMutex);
    return m_minKw > 1e17 ? 0.0 : m_minKw;
}

void EnergyMeter::resetPeakMin() {
    std::lock_guard<std::mutex> lk(m_readMutex);
    m_peakKw = 0.0;
    m_minKw  = 1e18;
}

EnergyMeter::Reading EnergyMeter::lastReading() const {
    std::lock_guard<std::mutex> lk(m_readMutex);
    return m_lastReading;
}

EnergyMeter::Reading EnergyMeter::takeReading() {
    std::lock_guard<std::mutex> lk(m_readMutex);

    auto now = std::chrono::steady_clock::now();
    double dtSec = std::chrono::duration<double>(now - m_prevTime).count();

    // ── Guard: if called twice within 200 ms, return the cached reading ──
    // This prevents the report loop from producing a zero-interval snapshot
    // when the reading loop already fired in the same scheduler tick.
    if (!m_firstReading && dtSec < 0.2) {
        return m_lastReading;
    }

    Reading r;
    r.meterId    = m_meterId;
    r.ts         = std::time(nullptr);
    r.pulseCount = m_counter.getTotalPulses();
    r.energyKwh  = m_counter.getTotalEnergyKwh();

    double intervalKwh = r.energyKwh - m_prevEnergyKwh;
    if (intervalKwh < 0.0) intervalKwh = 0.0;

    // Use at least 1 second to avoid divide-by-near-zero
    if (dtSec < 1.0) dtSec = 1.0;

    // Power = energy / time  (kWh / hours)
    double powerKw = (intervalKwh / (dtSec / 3600.0));

    // ── Sanity clamp: no single household circuit exceeds ~50 kW ──────────
    // This fires when the reading interval is very short (< a few seconds)
    // and the simulator has injected a large burst.  Cap so the display and
    // alerts are meaningful; the energy total remains exact.
    const double MAX_PLAUSIBLE_KW = 50.0;
    if (powerKw > MAX_PLAUSIBLE_KW) powerKw = MAX_PLAUSIBLE_KW;

    // Update peak / min
    if (powerKw > m_peakKw) m_peakKw = powerKw;
    if (powerKw < m_minKw)  m_minKw  = powerKw;

    // Cumulative cost
    m_totalCost = r.energyKwh * m_tariff.ratePerKwh;

    r.intervalKwh = intervalKwh;
    r.powerKw     = powerKw;
    r.cost        = m_totalCost;

    m_prevEnergyKwh = r.energyKwh;
    m_prevTime      = now;
    m_firstReading  = false;
    m_lastReading   = r;

    m_pending.push_back(r);
    return r;
}

std::vector<EnergyMeter::Reading> EnergyMeter::drainPendingReadings() {
    std::lock_guard<std::mutex> lk(m_readMutex);
    std::vector<Reading> out = std::move(m_pending);
    m_pending.clear();
    return out;
}
