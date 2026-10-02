#include "EnergyChallenge.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  EnergyChallenge — gamified daily/weekly scoring system
// ─────────────────────────────────────────────────────────────────────────────

EnergyChallenge::EnergyChallenge(const DataStore& store, double baselineDailyKwh)
    : m_store(store), m_baselineKwh(baselineDailyKwh) {}

// ── Badge based on score ──────────────────────────────────────────────────────
std::string EnergyChallenge::badge(int score) {
    if (score >= 90) return "[GOLD]   Energy Master!";
    if (score >= 70) return "[SILVER] Energy Saver";
    if (score >= 50) return "[BRONZE] Getting Better";
    return "[--]     Keep Trying";
}

// ── Motivational message ──────────────────────────────────────────────────────
std::string EnergyChallenge::motivate(double savingPct, int streak) {
    if (savingPct >= 20.0 && streak >= 5)
        return "Outstanding! 5+ day streak of savings - you're an Energy Champion!";
    if (savingPct >= 15.0)
        return "Excellent work! You're saving serious energy (and money)!";
    if (savingPct >= 10.0)
        return "Great job! You beat your average by " +
               std::to_string(static_cast<int>(savingPct)) + "%.";
    if (savingPct >= 5.0)
        return "Good effort! Small savings add up over the month.";
    if (savingPct > 0)
        return "You're slightly below average. Try turning off standby devices.";
    if (savingPct > -10.0)
        return "Usage a bit high today. Check if AC was left on unnecessarily.";
    return "High usage day. Audit appliances and try again tomorrow!";
}

// ── Today's challenge goal ────────────────────────────────────────────────────
EnergyChallenge::ChallengeGoal
EnergyChallenge::getTodayGoal(double averageKwh) const {
    ChallengeGoal g;
    g.targetPctReduction = 10.0;
    g.targetKwh = averageKwh * (1.0 - g.targetPctReduction / 100.0);
    char buf[128];
    snprintf(buf, sizeof(buf),
        "Reduce today's consumption by 10%% (target: %.2f kWh, avg: %.2f kWh)",
        g.targetKwh, averageKwh);
    g.description = buf;
    return g;
}

// ── Daily Score ───────────────────────────────────────────────────────────────
EnergyChallenge::DailyScore
EnergyChallenge::computeDailyScore(double todayKwh,
                                    bool   hadNightAlert,
                                    bool   hadAnomalyAlert,
                                    double peakPowerKw,
                                    double basePowerKw) const {
    DailyScore ds;
    ds.todayKwh = todayKwh;

    // Compute 7-day baseline average
    // Only count days with meaningful consumption (> 0.01 kWh) to avoid
    // polluting the average with incomplete micro-sessions from development.
    auto summaries = m_store.getDailySummaries(7);
    double avgKwh   = 0.0;
    int    goodDays = 0;
    for (auto& s : summaries) {
        if (s.totalKwh > 0.01) {
            avgKwh += s.totalKwh;
            goodDays++;
        }
    }
    if (goodDays >= 2) {
        avgKwh /= static_cast<double>(goodDays);
    } else if (m_baselineKwh > 0.0) {
        // Not enough real history — use configured baseline
        avgKwh = m_baselineKwh;
    } else if (todayKwh > 0.001) {
        avgKwh = todayKwh;
    } else {
        avgKwh = 5.0; // safe default: 5 kWh/day typical Indian household
    }
    ds.baselineKwh = avgKwh;

    if (avgKwh > 0.001)
        ds.savingPct = ((avgKwh - todayKwh) / avgKwh) * 100.0;

    // ── Scoring rubric ─────────────────────────────────────────────────────
    int score = 50; // base points always earned
    ds.breakdown.push_back("Base: 50 pts");

    // +20 if today < 7-day average
    if (todayKwh < avgKwh) {
        score += 20;
        char buf[64];
        snprintf(buf, sizeof(buf), "+20 pts: Below 7-day average (%.1f kWh)", avgKwh);
        ds.breakdown.push_back(buf);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "+0  pts: Above 7-day average (%.1f kWh)", avgKwh);
        ds.breakdown.push_back(buf);
    }

    // +10 if today < yesterday (only compare if yesterday had real data)
    if (!summaries.empty()) {
        double yesterday = summaries.back().totalKwh;
        if (yesterday > 0.01 && todayKwh < yesterday) {
            score += 10;
            ds.breakdown.push_back("+10 pts: Less than yesterday");
        } else if (yesterday > 0.01) {
            ds.breakdown.push_back("+0  pts: More than or equal to yesterday");
        } else {
            ds.breakdown.push_back("+0  pts: Yesterday has no data yet");
        }
    }

    // +10 if no night-waste alert
    if (!hadNightAlert) {
        score += 10;
        ds.breakdown.push_back("+10 pts: No night-waste alert");
    } else {
        score -= 5;
        ds.breakdown.push_back("-5  pts: Night-waste alert triggered");
    }

    // +10 if peak power < 3x base
    double peakThreshold = basePowerKw * 3.0;
    if (peakPowerKw < peakThreshold) {
        score += 10;
        char buf[64];
        snprintf(buf, sizeof(buf), "+10 pts: Peak power %.1f kW under %.1f kW limit",
                 peakPowerKw, peakThreshold);
        ds.breakdown.push_back(buf);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "+0  pts: Peak power %.1f kW exceeded %.1f kW limit",
                 peakPowerKw, peakThreshold);
        ds.breakdown.push_back(buf);
    }

    // -5 per anomaly alert
    if (hadAnomalyAlert) {
        score -= 5;
        ds.breakdown.push_back("-5  pts: Anomaly alert detected");
    }

    // Clamp
    if (score < 0)   score = 0;
    if (score > 100) score = 100;
    ds.score = score;

    // ── Streak (count consecutive days with score >= 70) ──────────────────
    ds.streak = 0;
    for (int i = static_cast<int>(summaries.size()) - 1; i >= 0; i--) {
        // Rough proxy: below average = good day
        if (summaries[i].totalKwh <= avgKwh * 1.05)
            ds.streak++;
        else
            break;
    }

    // ── Badge + goal ──────────────────────────────────────────────────────
    ds.badge           = badge(score);
    ds.motivationalMsg = motivate(ds.savingPct, ds.streak);

    ChallengeGoal goal = getTodayGoal(avgKwh);
    goal.achieved = (todayKwh <= goal.targetKwh);
    ds.goals.push_back(goal);

    return ds;
}

// ── Weekly Score ──────────────────────────────────────────────────────────────
EnergyChallenge::WeeklyScore EnergyChallenge::computeWeeklyScore() const {
    WeeklyScore ws;

    auto thisWeek = m_store.getDailySummaries(7);
    auto lastWeek = m_store.getDailySummaries(14); // last 14 days

    for (auto& d : thisWeek) ws.totalKwh += d.totalKwh;

    // prev week = days 7-14
    double prevKwh = 0.0;
    if (lastWeek.size() > 7) {
        for (size_t i = 0; i < lastWeek.size() - thisWeek.size(); i++)
            prevKwh += lastWeek[i].totalKwh;
    }
    ws.prevWeekKwh = prevKwh;

    if (prevKwh > 0.001)
        ws.weekSavingPct = ((prevKwh - ws.totalKwh) / prevKwh) * 100.0;

    // Weekly total score = sum of daily proxies
    for (auto& d : thisWeek) {
        double avg = (ws.totalKwh / std::max((int)thisWeek.size(), 1));
        ws.totalScore += (d.totalKwh <= avg) ? 75 : 55;
    }
    ws.totalScore = ws.totalScore / std::max((int)thisWeek.size(), 1);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "Week total: " << ws.totalKwh << " kWh";
    if (ws.weekSavingPct > 0)
        oss << "  (saved " << ws.weekSavingPct << "% vs last week)";
    else if (ws.weekSavingPct < 0)
        oss << "  (used " << -ws.weekSavingPct << "% more than last week)";
    ws.summary = oss.str();

    return ws;
}

// ── Print Score Card ──────────────────────────────────────────────────────────
void EnergyChallenge::printScoreCard(const DailyScore& ds) {
    const std::string sep(62, '=');
    std::cout << "\n" << sep << "\n";
    std::cout << "  ENERGY SAVING CHALLENGE  --  DAILY SCORE CARD\n";
    std::cout << sep << "\n";

    // Big score display
    std::cout << "\n";
    std::cout << "  Today's Energy Score:  " << ds.score << " / 100\n";
    std::cout << "  " << ds.badge << "\n";
    std::cout << "\n";

    // Usage info
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Today's usage    : " << ds.todayKwh    << " kWh\n";
    std::cout << "  7-day avg (base) : " << ds.baselineKwh << " kWh\n";

    if (ds.savingPct > 0)
        std::cout << "  Saved            : "
                  << std::setprecision(1) << ds.savingPct << "%  GREAT JOB!\n";
    else
        std::cout << "  Over baseline    : "
                  << std::setprecision(1) << -ds.savingPct << "%  Try harder!\n";

    if (ds.streak > 1)
        std::cout << "  Current streak   : " << ds.streak << " days\n";

    // Breakdown
    std::cout << "\n  Score breakdown:\n";
    for (auto& b : ds.breakdown)
        std::cout << "    " << b << "\n";

    // Goals
    if (!ds.goals.empty()) {
        std::cout << "\n  Today's challenge:\n";
        for (auto& g : ds.goals) {
            std::cout << "    " << (g.achieved ? "[ACHIEVED] " : "[PENDING]  ")
                      << g.description << "\n";
        }
    }

    // Motivational message
    std::cout << "\n  >> " << ds.motivationalMsg << "\n";
    std::cout << sep << "\n";
}

// ── Print Weekly Card ─────────────────────────────────────────────────────────
void EnergyChallenge::printWeeklyCard(const WeeklyScore& ws) {
    const std::string sep(62, '-');
    std::cout << "\n" << sep << "\n";
    std::cout << "  WEEKLY SUMMARY\n";
    std::cout << sep << "\n";
    std::cout << "  Weekly score : " << ws.totalScore << " / 100\n";
    std::cout << "  " << ws.summary << "\n";
    std::cout << sep << "\n";
}
