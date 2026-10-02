#pragma once
#include "EnergyMeter.h"
#include "../third_party/sqlite/sqlite3.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  DataStore — SQLite persistence (WAL mode)
// ─────────────────────────────────────────────────────────────────────────────
class DataStore {
public:
    struct DailySummary {
        std::time_t date{0};        // epoch of midnight
        double totalKwh{0};
        double avgPowerKw{0};
        double peakPowerKw{0};
        double totalCost{0};
        int    readingCount{0};
    };

    struct WindowStats {
        double  meanKwh{0};
        double  stddevKwh{0};
        double  minKwh{0};
        double  maxKwh{0};
        int     sampleCount{0};
    };

    explicit DataStore(const std::string& dbPath);
    ~DataStore();

    bool open();
    void close();
    bool isOpen() const { return m_db != nullptr; }

    // Persist readings
    bool insertReading(const EnergyMeter::Reading& r);

    // Persist meter state (base pulse count for reboot recovery)
    bool saveMeterState(const std::string& meterId, uint64_t basePulseCount);
    uint64_t loadMeterState(const std::string& meterId);

    // Query helpers
    std::vector<DailySummary> getDailySummaries(int days = 30) const;
    WindowStats  getWindowStats(std::time_t since) const;
    double       getIntervalKwhSince(std::time_t since) const;

    // Raw recent readings (for hourly pattern analysis)
    std::vector<EnergyMeter::Reading> getReadingsSince(std::time_t since) const;

private:
    void createSchema();

    std::string  m_dbPath;
    sqlite3*     m_db{nullptr};
};
