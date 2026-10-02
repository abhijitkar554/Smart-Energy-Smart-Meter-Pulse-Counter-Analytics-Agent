#include "PulseSimulator.h"
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  PulseSimulator — realistic Indian household energy profile
//
//  DIURNAL curve (power multiplier vs hour):
//    00-04  0.3  (overnight base: fridge + standby)
//    05-06  0.5  (pre-dawn)
//    06-09  2.5  (morning peak: geyser, kitchen, AC start)
//    09-12  0.8  (mid-morning lull)
//    12-14  1.2  (lunch / cooler)
//    14-18  0.7  (afternoon)
//    18-22  2.8  (evening peak: cooking, AC, TV, washing machine)
//    22-23  1.0  (wind-down)
//    23-24  0.3  (sleeping)
// ─────────────────────────────────────────────────────────────────────────────

PulseSimulator::PulseSimulator(PulseCounter& counter,
                                Profile       profile,
                                double        basePowerKw,
                                double        peakMultiplier,
                                double        speedMultiplier)
    : m_counter(counter),
      m_profile(profile),
      m_basePowerKw(basePowerKw),
      m_peakMult(peakMultiplier),
      m_speedMult(speedMultiplier) {}

PulseSimulator::Profile PulseSimulator::fromString(const std::string& s) {
    if (s == "CONSTANT") return Profile::CONSTANT;
    if (s == "RANDOM")   return Profile::RANDOM;
    return Profile::DIURNAL;
}

void PulseSimulator::start() {
    if (m_running.load()) return;
    m_running.store(true);
    m_thread = std::thread(&PulseSimulator::runLoop, this);
}

void PulseSimulator::stop() {
    m_running.store(false);
    if (m_thread.joinable()) m_thread.join();
}

double PulseSimulator::powerAtSimTime(double simHour) const {
    // simHour in [0, 24)
    double h = std::fmod(simHour, 24.0);

    double mult = 0.3; // default overnight

    if      (h < 4.0)  mult = 0.3;
    else if (h < 5.0)  mult = 0.5;
    else if (h < 9.0)  mult = 2.5;   // morning peak
    else if (h < 12.0) mult = 0.8;
    else if (h < 14.0) mult = 1.2;
    else if (h < 18.0) mult = 0.7;
    else if (h < 22.0) mult = 2.8;   // evening peak
    else if (h < 23.0) mult = 1.0;
    else               mult = 0.3;

    // Clamp multiplier to user's peak multiplier setting
    mult = std::min(mult, m_peakMult);

    return m_basePowerKw * mult;
}

void PulseSimulator::runLoop() {
    // Real-time wall clock step: 10 ms
    // Each step advances simulated time by (speedMult * 10ms)
    const double realStepMs   = 10.0;
    const double simStepSec   = (realStepMs / 1000.0) * m_speedMult;

    std::mt19937                          rng(42);
    std::normal_distribution<double>      noise(0.0, 0.15); // 15% sigma

    double simTimeSec = 0.0;
    double accumulatedKwh = 0.0;
    int    pulsesPerKwh   = m_counter.pulsesPerKwh();

    while (m_running.load()) {
        // Determine power this step
        double powerKw = m_basePowerKw;

        switch (m_profile) {
            case Profile::CONSTANT:
                powerKw = m_basePowerKw;
                break;
            case Profile::DIURNAL: {
                double simHour = std::fmod(simTimeSec / 3600.0, 24.0);
                powerKw = powerAtSimTime(simHour);
                break;
            }
            case Profile::RANDOM: {
                double n = noise(rng);
                powerKw = m_basePowerKw * (1.0 + n);
                if (powerKw < 0.05) powerKw = 0.05;
                break;
            }
        }

        // Energy produced this step: P(kW) * t(h) = kWh
        double stepKwh = powerKw * (simStepSec / 3600.0);
        accumulatedKwh += stepKwh;

        // Convert accumulated kWh to pulses
        double pulsesF = accumulatedKwh * static_cast<double>(pulsesPerKwh);
        uint64_t pulses = static_cast<uint64_t>(pulsesF);
        if (pulses > 0) {
            m_counter.addPulses(pulses);
            accumulatedKwh -= static_cast<double>(pulses) /
                              static_cast<double>(pulsesPerKwh);
        }

        simTimeSec += simStepSec;

        std::this_thread::sleep_for(
            std::chrono::microseconds(static_cast<long long>(realStepMs * 1000)));
    }
}
