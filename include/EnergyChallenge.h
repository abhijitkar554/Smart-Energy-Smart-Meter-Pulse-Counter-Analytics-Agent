#pragma once
#include "DataStore.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  EnergyChallenge — gamified daily/weekly energy-saving system
//
//  🏆 Today's Energy Score: 82/100
//     You saved 12% compared with your average.
//
//  Scoring rubric (daily):
//    Base 50 pts always earned.
//    +20 pts  if today < 7-day average
//    +10 pts  if today < yesterday
//    +10 pts  if no night-waste alert
//    +10 pts  peak power < 3× base
//    -5  pts  per anomaly alert
// ─────────────────────────────────────────────────────────────────────────────
class EnergyChallenge {
public:
    struct ChallengeGoal {
        std::string description;
        double      targetKwh{0};      // 0 = percentage goal
        double      targetPctReduction{10.0};
        bool        achieved{false};
    };

    struct DailyScore {
        int    score{0};              // 0-100
        int    streak{0};             // consecutive days >= 70
        double todayKwh{0};
        double baselineKwh{0};
        double savingPct{0};          // positive = saved
        std::string badge;            // 🥇 🥈 🥉 or ""
        std::vector<std::string> breakdown; // score component explanations
        std::vector<ChallengeGoal>  goals;
        std::string motivationalMsg;
    };

    struct WeeklyScore {
        int    totalScore{0};         // sum of 7 daily scores
        double totalKwh{0};
        double prevWeekKwh{0};
        double weekSavingPct{0};
        std::string summary;
    };

    EnergyChallenge(const DataStore& store, double baselineDailyKwh = 0.0);

    DailyScore  computeDailyScore(double todayKwh,
                                   bool   hadNightAlert,
                                   bool   hadAnomalyAlert,
                                   double peakPowerKw,
                                   double basePowerKw) const;

    WeeklyScore computeWeeklyScore() const;

    // Returns today's active challenge goal
    ChallengeGoal getTodayGoal(double averageKwh) const;

    // Print the gamified score card to stdout
    static void printScoreCard(const DailyScore& ds);
    static void printWeeklyCard(const WeeklyScore& ws);

private:
    const DataStore& m_store;
    double           m_baselineKwh;   // user-configured daily baseline

    static std::string badge(int score);
    static std::string motivate(double savingPct, int streak);
};
