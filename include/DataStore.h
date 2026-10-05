#pragma once
#include "EnergyMeter.h"
#include "../third_party/sqlite/sqlite3.h"
#include <string>
#include <vector>


class DataStore {
public:
    struct DailySummary {
        std::time_t date{0};        
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

    
    bool insertReading(const EnergyMeter::Reading& r);

    
    bool saveMeterState(const std::string& meterId, uint64_t basePulseCount);
    uint64_t loadMeterState(const std::string& meterId);

    
    std::vector<DailySummary> getDailySummaries(int days = 30) const;
    WindowStats  getWindowStats(std::time_t since) const;
    double       getIntervalKwhSince(std::time_t since) const;

    
    std::vector<EnergyMeter::Reading> getReadingsSince(std::time_t since) const;

private:
    void createSchema();

    std::string  m_dbPath;
    sqlite3*     m_db{nullptr};
};
