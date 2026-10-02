#include "DataStore.h"
#include <iostream>
#include <cstring>
#include <ctime>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  DataStore — SQLite persistence layer  (WAL mode)
// ─────────────────────────────────────────────────────────────────────────────

DataStore::DataStore(const std::string& dbPath) : m_dbPath(dbPath) {}

DataStore::~DataStore() { close(); }

bool DataStore::open() {
    int rc = sqlite3_open_v2(
        m_dbPath.c_str(), &m_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);

    if (rc != SQLITE_OK) {
        // BUG FIX #6 / #8: sqlite3_open_v2 can return a non-null handle even
        // on failure (to allow sqlite3_errmsg() to work).  We must call
        // sqlite3_errmsg() BEFORE we close/null the handle, then clean up.
        std::cerr << "[DB] Open failed: "
                  << (m_db ? sqlite3_errmsg(m_db) : "out of memory") << "\n";
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    // WAL mode for concurrent reads; NORMAL sync is safe for logging workloads
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;",  nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    createSchema();
    std::cout << "[DB] Opened: " << m_dbPath << "\n";
    return true;
}

void DataStore::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

void DataStore::createSchema() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS readings ("
        "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  meter_id     TEXT    NOT NULL,"
        "  ts_epoch     INTEGER NOT NULL,"
        "  energy_kwh   REAL    NOT NULL,"
        "  interval_kwh REAL    NOT NULL,"
        "  power_kw     REAL    NOT NULL,"
        "  cost         REAL    NOT NULL,"
        "  pulse_count  INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS meter_state ("
        "  meter_id         TEXT    PRIMARY KEY,"
        "  base_pulse_count INTEGER NOT NULL,"
        "  last_updated     INTEGER NOT NULL"
        ");";

    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[DB] Schema error: " << (err ? err : "?") << "\n";
        sqlite3_free(err);
    }
}

bool DataStore::insertReading(const EnergyMeter::Reading& r) {
    if (!m_db) return false;

    const char* sql =
        "INSERT INTO readings "
        "(meter_id, ts_epoch, energy_kwh, interval_kwh, power_kw, cost, pulse_count)"
        " VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text  (stmt, 1, r.meterId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 2, static_cast<sqlite3_int64>(r.ts));
    sqlite3_bind_double(stmt, 3, r.energyKwh);
    sqlite3_bind_double(stmt, 4, r.intervalKwh);
    sqlite3_bind_double(stmt, 5, r.powerKw);
    sqlite3_bind_double(stmt, 6, r.cost);
    sqlite3_bind_int64 (stmt, 7, static_cast<sqlite3_int64>(r.pulseCount));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool DataStore::saveMeterState(const std::string& meterId,
                                uint64_t basePulseCount) {
    if (!m_db) return false;

    const char* sql =
        "INSERT OR REPLACE INTO meter_state"
        " (meter_id, base_pulse_count, last_updated) VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text (stmt, 1, meterId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(basePulseCount));
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(std::time(nullptr)));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

uint64_t DataStore::loadMeterState(const std::string& meterId) {
    if (!m_db) return 0;

    const char* sql =
        "SELECT base_pulse_count FROM meter_state WHERE meter_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, meterId.c_str(), -1, SQLITE_TRANSIENT);

    uint64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));

    sqlite3_finalize(stmt);
    return result;
}

std::vector<DataStore::DailySummary>
DataStore::getDailySummaries(int days) const {
    std::vector<DailySummary> out;
    if (!m_db) return out;

    std::time_t since = std::time(nullptr)
                        - static_cast<std::time_t>(days) * 86400;

    const char* sql =
        "SELECT"
        "  (ts_epoch / 86400) * 86400 AS day_epoch,"
        "  SUM(interval_kwh),"
        "  AVG(power_kw),"
        "  MAX(power_kw),"
        "  MAX(cost),"
        "  COUNT(*)"
        " FROM readings"
        " WHERE ts_epoch >= ?"
        " GROUP BY day_epoch"
        " ORDER BY day_epoch ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(since));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DailySummary ds;
        ds.date         = static_cast<std::time_t>(sqlite3_column_int64  (stmt, 0));
        ds.totalKwh     = sqlite3_column_double(stmt, 1);
        ds.avgPowerKw   = sqlite3_column_double(stmt, 2);
        ds.peakPowerKw  = sqlite3_column_double(stmt, 3);
        ds.totalCost    = sqlite3_column_double(stmt, 4);
        ds.readingCount = sqlite3_column_int   (stmt, 5);
        out.push_back(ds);
    }
    sqlite3_finalize(stmt);
    return out;
}

DataStore::WindowStats DataStore::getWindowStats(std::time_t since) const {
    WindowStats ws;
    if (!m_db) return ws;

    const char* sql =
        "SELECT COUNT(*), SUM(interval_kwh), AVG(interval_kwh),"
        "       MIN(interval_kwh), MAX(interval_kwh)"
        " FROM readings WHERE ts_epoch >= ? AND interval_kwh > 0;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return ws;

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(since));

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ws.sampleCount = sqlite3_column_int   (stmt, 0);
        ws.meanKwh     = sqlite3_column_double(stmt, 2);
        ws.minKwh      = sqlite3_column_double(stmt, 3);
        ws.maxKwh      = sqlite3_column_double(stmt, 4);
    }
    sqlite3_finalize(stmt);

    // Second pass: sample stddev
    if (ws.sampleCount > 1) {
        const char* sql2 =
            "SELECT interval_kwh FROM readings"
            " WHERE ts_epoch >= ? AND interval_kwh > 0;";
        sqlite3_stmt* s2 = nullptr;
        if (sqlite3_prepare_v2(m_db, sql2, -1, &s2, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(s2, 1, static_cast<sqlite3_int64>(since));
            double sumSq = 0.0;
            int    n     = 0;
            while (sqlite3_step(s2) == SQLITE_ROW) {
                double d = sqlite3_column_double(s2, 0) - ws.meanKwh;
                sumSq += d * d;
                n++;
            }
            if (n > 1) ws.stddevKwh = std::sqrt(sumSq / (n - 1));
            sqlite3_finalize(s2);
        }
    }
    return ws;
}

double DataStore::getIntervalKwhSince(std::time_t since) const {
    if (!m_db) return 0.0;

    const char* sql =
        "SELECT SUM(interval_kwh) FROM readings WHERE ts_epoch >= ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0.0;

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(since));
    double result = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_double(stmt, 0);

    sqlite3_finalize(stmt);
    return result;
}

std::vector<EnergyMeter::Reading>
DataStore::getReadingsSince(std::time_t since) const {
    std::vector<EnergyMeter::Reading> out;
    if (!m_db) return out;

    const char* sql =
        "SELECT meter_id, ts_epoch, energy_kwh, interval_kwh,"
        "       power_kw, cost, pulse_count"
        " FROM readings WHERE ts_epoch >= ? ORDER BY ts_epoch ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(since));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EnergyMeter::Reading r;
        r.meterId     = reinterpret_cast<const char*>(
                            sqlite3_column_text(stmt, 0));
        r.ts          = static_cast<std::time_t>(sqlite3_column_int64 (stmt, 1));
        r.energyKwh   = sqlite3_column_double(stmt, 2);
        r.intervalKwh = sqlite3_column_double(stmt, 3);
        r.powerKw     = sqlite3_column_double(stmt, 4);
        r.cost        = sqlite3_column_double(stmt, 5);
        r.pulseCount  = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
        out.push_back(r);
    }
    sqlite3_finalize(stmt);
    return out;
}
