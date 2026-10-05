#include "AnalyticsEngine.h"
#include <cmath>
#include <ctime>
#include <algorithm>
#include <numeric>



AnalyticsEngine::AnalyticsEngine(const DataStore& store)
    : m_store(store) {}

static std::time_t localMidnight() {
    std::time_t now = std::time(nullptr);
    struct tm lt;
#ifdef _WIN32
    struct tm* ltp = localtime(&now);
    if (!ltp) return now;
    lt = *ltp;
#else
    if (localtime_r(&now, &lt) == nullptr) return now;
#endif
    lt.tm_hour = 0;
    lt.tm_min  = 0;
    lt.tm_sec  = 0;
    lt.tm_isdst = -1;  
    return mktime(&lt);
}


AnalyticsEngine::computeBillForecast(double ratePerKwh,
                                      double standingCharge,
                                      const std::string& currency) const {
    BillForecast f;
    f.currency = currency;

    std::time_t now   = std::time(nullptr);
    std::time_t today = localMidnight();  

    double todayKwh = m_store.getIntervalKwhSince(today);

    double hoursElapsed = static_cast<double>(now - today) / 3600.0;
    if (hoursElapsed < 0.5) hoursElapsed = 0.5;

    f.dailyKwh = (todayKwh / hoursElapsed) * 24.0;

    
    if (f.dailyKwh > 100.0) f.dailyKwh = 100.0;
    
    if (hoursElapsed < 1.0 && todayKwh < 0.1) f.dailyKwh = todayKwh;

    f.monthlyKwh    = f.dailyKwh * 30.0;
    f.estimatedBill = f.monthlyKwh * ratePerKwh + standingCharge;

    
#ifdef _WIN32
    struct tm* ltm = localtime(&now);
    int dayOfMonth  = ltm ? ltm->tm_mday : 15;
#else
    struct tm ltbuf;
    struct tm* ltm = localtime_r(&now, &ltbuf);
    int dayOfMonth  = ltm ? ltm->tm_mday : 15;
#endif
    f.daysRemaining = 30 - dayOfMonth;
    if (f.daysRemaining < 0) f.daysRemaining = 0;

    return f;
}


AnalyticsEngine::AnomalyResult
AnalyticsEngine::detectAnomaly(double currentIntervalKwh,
                                double warnZScore,
                                double critZScore) const {
    AnomalyResult res;

    std::time_t since30 = std::time(nullptr) - 30LL * 86400;
    auto stats = m_store.getWindowStats(since30);

    if (stats.sampleCount < 5) return res;   

    res.meanKwh    = stats.meanKwh;
    res.stddevKwh  = stats.stddevKwh;
    res.currentKwh = currentIntervalKwh;

    if (stats.stddevKwh < 1e-9) return res;

    res.zScore = (currentIntervalKwh - stats.meanKwh) / stats.stddevKwh;

 
    std::time_t now = std::time(nullptr);
#ifdef _WIN32
    struct tm* ltm2 = localtime(&now);
    res.hourOfDay   = ltm2 ? ltm2->tm_hour : -1;
#else
    struct tm ltbuf2;
    struct tm* ltm2 = localtime_r(&now, &ltbuf2);
    res.hourOfDay   = ltm2 ? ltm2->tm_hour : -1;
#endif

    if (std::abs(res.zScore) >= critZScore) {
        res.detected = true;
        char buf[256];
        snprintf(buf, sizeof(buf),
            "CRITICAL: Consumption %.3f kWh is %.1f standard deviations above "
            "normal (mean=%.3f kWh). Possible appliance fault or meter error.",
            currentIntervalKwh, res.zScore, stats.meanKwh);
        res.message = buf;

    } else if (std::abs(res.zScore) >= warnZScore) {
        res.detected = true;
        char buf[256];
        if (res.hourOfDay >= 2 && res.hourOfDay <= 4) {
            
            double pctAbove = (stats.meanKwh > 1e-9)
                ? ((currentIntervalKwh - stats.meanKwh) / stats.meanKwh * 100.0)
                : 0.0;
            snprintf(buf, sizeof(buf),
                "Your energy consumption between %d:00-%d:00 is %.0f%% higher "
                "than usual. Check your AC / refrigerator / water heater.",
                res.hourOfDay, res.hourOfDay + 1, pctAbove);
        } else {
            double pctAbove = (stats.meanKwh > 1e-9)
                ? ((currentIntervalKwh - stats.meanKwh) / stats.meanKwh * 100.0)
                : 0.0;
            snprintf(buf, sizeof(buf),
                "Consumption %.3f kWh is %.0f%% above normal (mean=%.3f kWh). "
                "Consider checking high-draw appliances.",
                currentIntervalKwh, pctAbove, stats.meanKwh);
        }
        res.message = buf;
    }

    return res;
}


AnalyticsEngine::TrendResult
AnalyticsEngine::computeTrend(int days) const {
    TrendResult tr;

    auto summaries = m_store.getDailySummaries(days);
    if (summaries.size() < 3) return tr;

    std::vector<double> x, y;
    for (size_t i = 0; i < summaries.size(); i++) {
        x.push_back(static_cast<double>(i));
        y.push_back(summaries[i].totalKwh);
    }

    double slope = 0, intercept = 0;
    tr.r2             = linearRegression(x, y, slope, intercept);
    tr.slopeKwhPerDay = slope;
    tr.interceptKwh   = intercept;
    tr.increasing     = slope > 0;
    tr.significant    = std::abs(slope) > 0.05;

    return tr;
}


AnalyticsEngine::HourlyPattern
AnalyticsEngine::buildHourlyPattern(int days) const {
    HourlyPattern hp;

    std::time_t since = std::time(nullptr) - static_cast<std::time_t>(days) * 86400;
    auto readings = m_store.getReadingsSince(since);

    double sum[24]   = {};
    double sumSq[24] = {};
    int    cnt[24]   = {};

    for (auto& r : readings) {
        if (r.intervalKwh <= 0) continue;
#ifdef _WIN32
        struct tm* ltm = localtime(&r.ts);
        if (!ltm) continue;
        int h = ltm->tm_hour;
#else
        struct tm ltbuf;
        struct tm* ltm = localtime_r(&r.ts, &ltbuf);
        if (!ltm) continue;
        int h = ltm->tm_hour;
#endif
        sum[h]   += r.intervalKwh;
        sumSq[h] += r.intervalKwh * r.intervalKwh;
        cnt[h]++;
    }

    for (int h = 0; h < 24; h++) {
        hp.sampleCount[h] = cnt[h];
        if (cnt[h] > 0) {
            hp.meanKwh[h] = sum[h] / cnt[h];
            if (cnt[h] > 1) {
                double var = (sumSq[h] / cnt[h]) - (hp.meanKwh[h] * hp.meanKwh[h]);
                hp.stddev[h] = (var > 0.0) ? std::sqrt(var) : 0.0;
            }
        }
    }

    return hp;
}


int AnalyticsEngine::computeEfficiencyScore(double todayKwh,
                                             double baselineKwh) const {
    if (baselineKwh < 0.001) return 50;  

  
    double ratio = todayKwh / baselineKwh;
    int score = static_cast<int>(100.0 * (2.0 - ratio) / 2.0);
    if (score < 0)   score = 0;
    if (score > 100) score = 100;
    return score;
}


double AnalyticsEngine::linearRegression(const std::vector<double>& x,
                                          const std::vector<double>& y,
                                          double& slope,
                                          double& intercept) {
    size_t n = x.size();
    if (n < 2) { slope = 0; intercept = 0; return 0.0; }

    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (size_t i = 0; i < n; i++) {
        sumX  += x[i];
        sumY  += y[i];
        sumXY += x[i] * y[i];
        sumX2 += x[i] * x[i];
    }

    double denom = static_cast<double>(n) * sumX2 - sumX * sumX;
    if (std::abs(denom) < 1e-12) {
        slope = 0;
        intercept = sumY / static_cast<double>(n);
        return 0.0;
    }

    slope     = (static_cast<double>(n) * sumXY - sumX * sumY) / denom;
    intercept = (sumY - slope * sumX) / static_cast<double>(n);

    double meanY = sumY / static_cast<double>(n);
    double ssTot = 0, ssRes = 0;
    for (size_t i = 0; i < n; i++) {
        double predicted = slope * x[i] + intercept;
        ssRes += (y[i] - predicted) * (y[i] - predicted);
        ssTot += (y[i] - meanY)     * (y[i] - meanY);
    }

    return (ssTot < 1e-12) ? 1.0 : 1.0 - ssRes / ssTot;
}
