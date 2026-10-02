#include "ConfigReader.h"
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iostream>

// ─── Minimal JSON value extractor ────────────────────────────────────────────
// Supports flat {"key": value} JSON; no nested objects or arrays needed here.

static std::string extractStr(const std::string& json, const std::string& key) {
    // "key": "value"
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static double extractNum(const std::string& json, const std::string& key,
                          double def = 0.0) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return def;
    // read until non-numeric
    std::string num;
    while (pos < json.size()) {
        char c = json[pos];
        if (c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E' ||
            (c >= '0' && c <= '9')) {
            num += c;
        } else {
            break;
        }
        pos++;
    }
    if (num.empty()) return def;
    try { return std::stod(num); } catch (...) { return def; }
}

bool ConfigReader::load(const std::string& path, Config& cfg) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[CONFIG] Cannot open " << path
                  << " — using defaults (INR tariff).\n";
        return false;
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    // Identity
    auto s = extractStr(json, "meter_id");
    if (!s.empty()) cfg.meterId = s;

    auto ppk = (int)extractNum(json, "pulses_per_kwh", cfg.pulsesPerKwh);
    if (ppk > 0) cfg.pulsesPerKwh = ppk;

    // Tariff
    auto rate = extractNum(json, "rate_per_kwh", -1);
    if (rate >= 0) cfg.ratePerKwh = rate;

    auto sc = extractNum(json, "standing_charge", -1);
    if (sc >= 0) cfg.standingCharge = sc;

    s = extractStr(json, "currency");
    if (!s.empty()) cfg.currency = s;

    // Intervals
    auto ri = (int)extractNum(json, "reading_interval", -1);
    if (ri > 0) cfg.readingIntervalSec = ri;

    auto rep = (int)extractNum(json, "report_interval_sec", -1);
    if (rep > 0) cfg.reportIntervalSec = rep;

    auto run = (int)extractNum(json, "run_duration_sec", -999);
    if (run >= 0) cfg.runDurationSec = run;

    // Thresholds
    auto ca = extractNum(json, "cost_alert_threshold", -1);
    if (ca >= 0) cfg.costAlertThreshold = ca;

    auto wz = extractNum(json, "anomaly_warn_zscore", -1);
    if (wz >= 0) cfg.anomalyWarnZScore = wz;

    auto cz = extractNum(json, "anomaly_crit_zscore", -1);
    if (cz >= 0) cfg.anomalyCritZScore = cz;

    // Night watchdog
    auto ns = (int)extractNum(json, "night_start_hour", -1);
    if (ns >= 0 && ns < 24) cfg.nightStartHour = ns;

    auto ne = (int)extractNum(json, "night_end_hour", -1);
    if (ne >= 0 && ne < 24) cfg.nightEndHour = ne;

    auto nw = extractNum(json, "night_waste_threshold_kwh", -1);
    if (nw >= 0) cfg.nightWasteThresholdKwh = nw;

    // Simulator
    s = extractStr(json, "sim_profile");
    if (!s.empty()) cfg.simProfile = s;

    auto bp = extractNum(json, "sim_base_power_kw", -1);
    if (bp >= 0) cfg.simBasePowerKw = bp;

    auto pm = extractNum(json, "sim_peak_multiplier", -1);
    if (pm >= 0) cfg.simPeakMultiplier = pm;

    auto sm = extractNum(json, "sim_speed_multiplier", -1);
    if (sm >= 0) cfg.simSpeedMultiplier = sm;

    // DB
    s = extractStr(json, "db_path");
    if (!s.empty()) cfg.dbPath = s;

    std::cout << "[CONFIG] Loaded: " << path << "\n";
    return true;
}
