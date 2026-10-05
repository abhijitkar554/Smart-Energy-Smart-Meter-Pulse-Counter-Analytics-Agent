

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
#include "HardwarePulseReader.h"


static std::atomic<bool> g_stop{false};
static std::atomic<bool> g_dumpReport{false};   

static void sigStop(int)   { g_stop.store(true);       }
static void sigReport(int) { g_dumpReport.store(true);  }


static std::time_t localMidnight() {
    std::time_t now = std::time(nullptr);
    struct tm lt;
#ifdef _WIN32
    struct tm* p = localtime(&now);
    if (!p) return now;
    lt = *p;
#else
    if (localtime_r(&now, &lt) == nullptr) return now;
#endif
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0; lt.tm_isdst = -1;
    return mktime(&lt);
}


static void printBanner(const Config& cfg) {
    const std::string sep(62, '=');
    std::cout << "\n" << sep << "\n";
    std::cout << "  Smart Energy Meter  --  Pulse Counter & Analytics Agent\n";
    std::cout << "  Wipro Embedded Systems Capstone Project  v4.0\n";
    std::cout << sep << "\n";
    std::cout << "  Meter ID     : " << cfg.meterId           << "\n";
    std::cout << "  Pulses/kWh   : " << cfg.pulsesPerKwh      << "\n";
    std::cout << "  Tariff       : " << cfg.currency << " "
              << std::fixed << std::setprecision(2)
              << cfg.ratePerKwh << "/kWh\n";
    std::cout << "  Standing chg : " << cfg.currency << " "
              << cfg.standingCharge << "/month\n";
    std::cout << "  Bill warn    : " << cfg.currency << " "
              << cfg.costAlertThreshold << "\n";
    std::cout << "  Night window : " << cfg.nightStartHour << ":00 - "
              <<                        cfg.nightEndHour   << ":00\n";
    std::cout << "  Sim profile  : " << cfg.simProfile         << "\n";
    std::cout << "  Sim speed    : " << cfg.simSpeedMultiplier << "x\n";
    std::cout << "  Base power   : " << cfg.simBasePowerKw     << " kW\n";
    std::cout << "  DB           : " << cfg.dbPath             << "\n";
    if (cfg.runDurationSec > 0)
        std::cout << "  Run for      : " << cfg.runDurationSec << " seconds\n";
    else
        std::cout << "  Run for      : indefinitely  (Ctrl+C or SIGTERM to stop)\n";
#ifndef _WIN32
    std::cout << "  On-demand    : send SIGUSR1 to dump a report now\n";
#endif
    std::cout << sep << "\n\n";
}


static void printHeartbeat(const EnergyMeter::Reading& r,
                            const std::string& currency) {
    char tsbuf[32];
#ifdef _WIN32
    struct tm* ltm = localtime(&r.ts);
    if (ltm) strftime(tsbuf, sizeof(tsbuf), "%H:%M:%S", ltm);
    else snprintf(tsbuf, sizeof(tsbuf), "??:??:??");
#else
    struct tm ltbuf;
    struct tm* ltm = localtime_r(&r.ts, &ltbuf);
    if (ltm) strftime(tsbuf, sizeof(tsbuf), "%H:%M:%S", ltm);
    else snprintf(tsbuf, sizeof(tsbuf), "??:??:??");
#endif

    std::cout << "[" << tsbuf << "]"
              << "  Total: " << std::fixed << std::setprecision(4)
              << r.energyKwh   << " kWh"
              << "  |  Power: " << std::setprecision(3) << r.powerKw << " kW"
              << "  |  Pulses: " << r.pulseCount
              << "  |  Cost: "  << currency << " "
              << std::setprecision(2) << r.cost
              << "\n";
}


int main(int argc, char* argv[]) {
    
    std::signal(SIGINT,  sigStop);
    std::signal(SIGTERM, sigStop);
#ifndef _WIN32
   
    std::signal(SIGUSR1, sigReport);
#endif

  
    std::string configPath = (argc > 1) ? argv[1] : "data/config.json";
    Config cfg;
    ConfigReader::load(configPath, cfg);
    printBanner(cfg);

    
    PulseCounter counter(cfg.pulsesPerKwh);

    DataStore db(cfg.dbPath);
    if (!db.open()) {
        std::cerr << "[ERROR] Cannot open database. Exiting.\n";
        return 1;
    }

  
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

    AnalyticsEngine analytics(db);

   
    AnalyticsAgent agent(analytics, meter,
                         cfg.anomalyWarnZScore,
                         cfg.anomalyCritZScore,
                         cfg.costAlertThreshold,
                         cfg.costAlertThreshold * 1.5,
                         5.0 /* peak alert kW */);

    NightWatchdog   nightdog(db,
                             cfg.nightStartHour,
                             cfg.nightEndHour,
                             cfg.nightWasteThresholdKwh);

    double baselineDailyKwh = cfg.simBasePowerKw * 8.0;
    EnergyChallenge challenge(db, baselineDailyKwh);

   
    HardwarePulseReader hwReader(counter);
    bool useHardware = HardwarePulseReader::devicePresent() && hwReader.open();

    PulseSimulator sim(counter,
                       PulseSimulator::fromString(cfg.simProfile),
                       cfg.simBasePowerKw,
                       cfg.simPeakMultiplier,
                       cfg.simSpeedMultiplier);

    if (useHardware) {
        std::cout << "[HW]  Using /dev/pulse_counter (kernel driver)\n";
        std::cout << "[HW]  Inject pulses: echo N | sudo tee /dev/pulse_counter\n";
    } else {
        sim.start();
        std::cout << "[SIM] Pulse simulator started"
                  << "  profile=" << cfg.simProfile
                  << "  speed="   << cfg.simSpeedMultiplier << "x\n";
    }
    std::cout << "[RUN] Main loop started."
#ifndef _WIN32
              << " PID=" << getpid()
              << " — send SIGUSR1 for on-demand report"
#endif
              << "\n\n";

 
    using Clock = std::chrono::steady_clock;
    auto startWall   = Clock::now();
    auto lastReading = startWall;
    auto lastReport  = startWall;
    auto lastDBSave  = startWall;

    int  reportCount   = 0;
    bool hadNightAlert = false;
    bool hadAnomaly    = false;

  
    auto doReport = [&]() {
        reportCount++;
        EnergyMeter::Reading snap = meter.lastReading();

        auto report = agent.generate(snap,
                                     cfg.ratePerKwh,
                                     cfg.standingCharge,
                                     cfg.currency);
        AnalyticsAgent::printReport(report);

        
        auto nightRpt = nightdog.evaluate();
        if (nightRpt.alertTriggered) {
            hadNightAlert = true;
            std::cout << "\n[NIGHT WATCHDOG]\n";
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

      
        double todayKwh = db.getIntervalKwhSince(localMidnight());
        if (todayKwh <= 0.0) todayKwh = snap.energyKwh;

        auto daily = challenge.computeDailyScore(
            todayKwh,
            hadNightAlert,
            hadAnomaly,
            meter.getPeakPowerKW(),
            cfg.simBasePowerKw);
        EnergyChallenge::printScoreCard(daily);

        if (reportCount % 3 == 0) {
            auto weekly = challenge.computeWeeklyScore();
            EnergyChallenge::printWeeklyCard(weekly);
        }

        hadNightAlert = false;
        hadAnomaly    = false;
    };

    
    while (!g_stop.load()) {
        auto now = Clock::now();

        
        auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(
                              now - startWall).count();
        if (cfg.runDurationSec > 0 && elapsedSec >= cfg.runDurationSec) {
            std::cout << "\n[RUN] Duration " << cfg.runDurationSec
                      << "s reached. Stopping.\n";
            break;
        }

       
        auto sinceRead = std::chrono::duration_cast<std::chrono::seconds>(
                             now - lastReading).count();
        if (sinceRead >= cfg.readingIntervalSec) {
            lastReading = now;

           
            if (useHardware) hwReader.sync();

            EnergyMeter::Reading reading = meter.takeReading();
            printHeartbeat(reading, cfg.currency);
            db.insertReading(reading);

            auto anomaly = analytics.detectAnomaly(
                reading.intervalKwh,
                cfg.anomalyWarnZScore,
                cfg.anomalyCritZScore);
            if (anomaly.detected) {
                hadAnomaly = true;
                std::cout << "  [!] " << anomaly.message << "\n";
            }
        }

     
                               now - lastDBSave).count();
        if (sinceDBSave >= 60) {
            lastDBSave = now;
            db.saveMeterState(cfg.meterId, counter.getTotalPulses());
        }

      
        auto sinceReport = std::chrono::duration_cast<std::chrono::seconds>(
                               now - lastReport).count();
        if (sinceReport >= cfg.reportIntervalSec) {
            lastReport = now;
            doReport();
        }

        
        if (g_dumpReport.exchange(false)) {
            std::cout << "\n[SIGNAL] SIGUSR1 received — dumping report now.\n";
            doReport();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    
    std::cout << "\n[SHUTDOWN] Stopping pulse source...\n";
    if (useHardware)
        hwReader.close();
    else
        sim.stop();

    db.saveMeterState(cfg.meterId, counter.getTotalPulses());
    std::cout << "[SHUTDOWN] Meter state saved.\n";

    EnergyMeter::Reading final_r = meter.lastReading();
    const std::string sep(62, '=');
    std::cout << "\n" << sep << "\n";
    std::cout << "  FINAL READING SUMMARY\n";
    std::cout << sep << "\n";
    std::cout << "  Meter ID     : " << final_r.meterId << "\n";
    std::cout << "  Total Energy : " << std::fixed << std::setprecision(4)
              << final_r.energyKwh << " kWh\n";
    std::cout << "  Total Pulses : " << final_r.pulseCount << "\n";
    std::cout << "  Total Cost   : " << cfg.currency << " "
              << std::setprecision(2) << final_r.cost << "\n";
    std::cout << "  Peak Power   : " << std::setprecision(3)
              << meter.getPeakPowerKW() << " kW\n";
    std::cout << sep << "\n";

    db.close();
    std::cout << "[DONE] Smart Energy Meter stopped cleanly.\n";
    return 0;
}
