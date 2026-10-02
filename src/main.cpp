// =============================================================================
//  Smart Energy Smart-Meter  --  Pulse Counter & Analytics Agent
//  Wipro Embedded Systems Capstone Project
//
//  Author  : Wipro Capstone Team
//  Version : 3.1  (runtime bug fixes)
//  Language: C++14
// =============================================================================

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <csignal>
#include <atomic>
#include <ctime>
#include "threading_compat.h"

#include "ConfigReader.h"
#include "PulseCounter.h"
#include "EnergyMeter.h"
#include "DataStore.h"
#include "AnalyticsEngine.h"
#include "AnalyticsAgent.h"
#include "PulseSimulator.h"
#include "NightWatchdog.h"
#include "EnergyChallenge.h"

// ── Global stop flag ──────────────────────────────────────────────────────────
static std::atomic<bool> g_stop{false};

static void signalHandler(int) {
    g_stop.store(true);
}

// ── Banner ────────────────────────────────────────────────────────────────────
static void printBanner(const Config& cfg) {
    std::string sep(62, '=');
    std::cout << "\n" << sep << "\n";
    std::cout << "  Smart Energy Meter  --  Pulse Counter & Analytics Agent\n";
    std::cout << "  Wipro Embedded Systems Capstone Project\n";
    std::cout << sep << "\n";
    std::cout << "  Meter ID     : " << cfg.meterId              << "\n";
    std::cout << "  Pulses/kWh   : " << cfg.pulsesPerKwh         << "\n";
    std::cout << "  Tariff       : " << cfg.currency << " "
              << std::fixed << std::setprecision(2) << cfg.ratePerKwh << "/kWh\n";
    std::cout << "  Standing chg : " << cfg.currency << " "
              << cfg.standingCharge << "/month\n";
    std::cout << "  Night window : " << cfg.nightStartHour << ":00 - "
              <<                        cfg.nightEndHour   << ":00\n";
    std::cout << "  Sim profile  : " << cfg.simProfile           << "\n";
    std::cout << "  Sim speed    : " << cfg.simSpeedMultiplier   << "x real time\n";
    std::cout << "  Base power   : " << cfg.simBasePowerKw       << " kW\n";
    std::cout << "  DB           : " << cfg.dbPath               << "\n";
    if (cfg.runDurationSec > 0)
        std::cout << "  Run for      : " << cfg.runDurationSec << " seconds\n";
    else
        std::cout << "  Run for      : indefinitely  (Ctrl+C to stop)\n";
    std::cout << sep << "\n\n";
}

// ── Heartbeat line (printed every reading interval) ───────────────────────────
static void printHeartbeat(const EnergyMeter::Reading& r,
                            const std::string& currency) {
    char tsbuf[32];
    struct tm* ltm = localtime(&r.ts);
    if (ltm) strftime(tsbuf, sizeof(tsbuf), "%H:%M:%S", ltm);
    else     snprintf(tsbuf, sizeof(tsbuf), "??:??:??");

    std::cout << "[" << tsbuf << "]"
              << "  Total: " << std::fixed << std::setprecision(4)
              << r.energyKwh << " kWh"
              << "  |  Power: " << std::setprecision(3) << r.powerKw << " kW"
              << "  |  Pulses: " << r.pulseCount
              << "  |  Cost: " << currency << " "
              << std::setprecision(2) << r.cost
              << "\n";
}

// =============================================================================
//  main
// =============================================================================
int main(int argc, char* argv[]) {
    // ── Signal handler ─────────────────────────────────────────────────────
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── Config ─────────────────────────────────────────────────────────────
    std::string configPath = (argc > 1) ? argv[1] : "data/config.json";
    Config cfg;
    ConfigReader::load(configPath, cfg);
    printBanner(cfg);

    // ── Core objects ────────────────────────────────────────────────────────
    PulseCounter counter(cfg.pulsesPerKwh);

    DataStore db(cfg.dbPath);
    if (!db.open()) {
        std::cerr << "[ERROR] Cannot open database. Exiting.\n";
        return 1;
    }

    // Restore base pulse count from previous session
    uint64_t savedBase = db.loadMeterState(cfg.meterId);
    if (savedBase > 0) {
        counter.setBaseCount(savedBase);
        std::cout << "[INFO] Restored base pulse count: " << savedBase << "\n";
    }

    EnergyMeter meter(counter, cfg.meterId);
    {
        EnergyMeter::TariffConfig tariff;
        tariff.ratePerKwh     = cfg.ratePerKwh;
        tariff.standingCharge = cfg.standingCharge;
        tariff.currency       = cfg.currency;
        meter.setTariff(tariff);
    }

    AnalyticsEngine  analytics(db);
    AnalyticsAgent   agent(analytics, meter,
                           cfg.anomalyWarnZScore, cfg.anomalyCritZScore);
    NightWatchdog    nightdog(db,
                              cfg.nightStartHour,
                              cfg.nightEndHour,
                              cfg.nightWasteThresholdKwh);

    // Baseline = 8 hours × base power (typical daily kWh for configured load)
    double baselineDailyKwh = cfg.simBasePowerKw * 8.0;
    EnergyChallenge challenge(db, baselineDailyKwh);

    // ── Simulator ──────────────────────────────────────────────────────────
    PulseSimulator sim(counter,
                       PulseSimulator::fromString(cfg.simProfile),
                       cfg.simBasePowerKw,
                       cfg.simPeakMultiplier,
                       cfg.simSpeedMultiplier);
    sim.start();
    std::cout << "[SIM] Pulse simulator started"
              << "  profile=" << cfg.simProfile
              << "  speed=" << cfg.simSpeedMultiplier << "x\n";
    std::cout << "[RUN] Main loop started. Press Ctrl+C to stop.\n\n";

    // ── Timing ────────────────────────────────────────────────────────────
    using Clock = std::chrono::steady_clock;
    auto startWall   = Clock::now();
    auto lastReading = startWall;
    auto lastReport  = startWall;
    auto lastDBSave  = startWall;

    int  reportCount   = 0;
    bool hadNightAlert = false;
    bool hadAnomaly    = false;

    // ── Main loop ─────────────────────────────────────────────────────────
    while (!g_stop.load()) {
        auto now = Clock::now();

        // Check run duration
        auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(
                              now - startWall).count();
        if (cfg.runDurationSec > 0 && elapsedSec >= cfg.runDurationSec) {
            std::cout << "\n[RUN] Duration " << cfg.runDurationSec
                      << "s reached. Stopping.\n";
            break;
        }

        // ── Reading interval: snap + persist + heartbeat ───────────────────
        auto sinceRead = std::chrono::duration_cast<std::chrono::seconds>(
                             now - lastReading).count();
        if (sinceRead >= cfg.readingIntervalSec) {
            lastReading = now;

            EnergyMeter::Reading reading = meter.takeReading();
            printHeartbeat(reading, cfg.currency);
            db.insertReading(reading);

            // Check for anomaly
            auto anomaly = analytics.detectAnomaly(
                reading.intervalKwh,
                cfg.anomalyWarnZScore,
                cfg.anomalyCritZScore);
            if (anomaly.detected) {
                hadAnomaly = true;
                std::cout << "  [!] " << anomaly.message << "\n";
            }
        }

        // ── Save meter state every 60 seconds ─────────────────────────────
        auto sinceDBSave = std::chrono::duration_cast<std::chrono::seconds>(
                               now - lastDBSave).count();
        if (sinceDBSave >= 60) {
            lastDBSave = now;
            db.saveMeterState(cfg.meterId, counter.getTotalPulses());
        }

        // ── Report interval: full analytics printout ───────────────────────
        auto sinceReport = std::chrono::duration_cast<std::chrono::seconds>(
                               now - lastReport).count();
        if (sinceReport >= cfg.reportIntervalSec) {
            lastReport = now;
            reportCount++;

            // Reuse the last reading — don't call takeReading() again here,
            // which would produce a zero-interval snapshot
            EnergyMeter::Reading snap = meter.lastReading();

            // ── Analytics Report ──────────────────────────────────────────
            auto report = agent.generate(snap,
                                         cfg.ratePerKwh,
                                         cfg.standingCharge,
                                         cfg.currency);
            AnalyticsAgent::printReport(report);

            // ── Night Watchdog ────────────────────────────────────────────
            auto nightRpt = nightdog.evaluate();
            if (nightRpt.alertTriggered) {
                hadNightAlert = true;
                std::cout << "\n[NIGHT WATCHDOG]\n";
                std::cout << "  ** Night Energy Alert! **\n";
                std::cout << "  " << nightRpt.message << "\n";
                if (!nightRpt.suspects.empty()) {
                    std::cout << "  Possible culprits:\n";
                    for (auto& s : nightRpt.suspects)
                        std::cout << "    - " << s << "\n";
                }
                std::cout << "  7-night avg : "
                          << std::fixed << std::setprecision(3)
                          << nightRpt.averageNightKwh << " kWh\n";
            }

            // ── Energy Challenge Score Card ───────────────────────────────
            // "today" = kWh since midnight (UTC)
            std::time_t midnight = (std::time(nullptr) / 86400) * 86400;
            double todayKwh = db.getIntervalKwhSince(midnight);
            if (todayKwh <= 0.0) todayKwh = snap.energyKwh; // fallback for new sessions

            auto daily = challenge.computeDailyScore(
                todayKwh,
                hadNightAlert,
                hadAnomaly,
                meter.getPeakPowerKW(),
                cfg.simBasePowerKw);
            EnergyChallenge::printScoreCard(daily);

            // Weekly card every 3rd report
            if (reportCount % 3 == 0) {
                auto weekly = challenge.computeWeeklyScore();
                EnergyChallenge::printWeeklyCard(weekly);
            }

            // Reset per-report alert flags
            hadNightAlert = false;
            hadAnomaly    = false;
        }

        // Sleep 200 ms — tight enough for sub-second reading intervals
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // ── Shutdown ───────────────────────────────────────────────────────────
    std::cout << "\n[SHUTDOWN] Stopping simulator...\n";
    sim.stop();

    // Final state save
    db.saveMeterState(cfg.meterId, counter.getTotalPulses());
    std::cout << "[SHUTDOWN] Meter state saved.\n";

    // Final summary
    EnergyMeter::Reading final_r = meter.lastReading();
    double peakKw = meter.getPeakPowerKW();

    std::cout << "\n";
    std::string sep(62, '=');
    std::cout << sep << "\n";
    std::cout << "  FINAL READING SUMMARY\n";
    std::cout << sep << "\n";
    std::cout << "  Meter ID     : " << final_r.meterId << "\n";
    std::cout << "  Total Energy : " << std::fixed << std::setprecision(4)
              << final_r.energyKwh << " kWh\n";
    std::cout << "  Total Pulses : " << final_r.pulseCount << "\n";
    std::cout << "  Total Cost   : " << cfg.currency << " "
              << std::setprecision(2) << final_r.cost << "\n";
    std::cout << "  Peak Power   : " << std::setprecision(3) << peakKw << " kW\n";
    std::cout << sep << "\n";

    db.close();
    std::cout << "[DONE] Smart Energy Meter stopped cleanly.\n";
    return 0;
}
