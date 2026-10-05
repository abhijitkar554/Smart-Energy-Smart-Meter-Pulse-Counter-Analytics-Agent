#pragma once
#include "DataStore.h"
#include <string>
#include <vector>


class EnergyChallenge {
public:
    struct ChallengeGoal {
        std::string description;
        double      targetKwh{0};      
        double      targetPctReduction{10.0};
        bool        achieved{false};
    };

    struct DailyScore {
        int    score{0};              
        int    streak{0};             
        double todayKwh{0};
        double baselineKwh{0};
        double savingPct{0};          
        std::string badge;           
        std::vector<std::string> breakdown; 
        std::vector<ChallengeGoal>  goals;
        std::string motivationalMsg;
    };

    struct WeeklyScore {
        int    totalScore{0};         
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

   
    ChallengeGoal getTodayGoal(double averageKwh) const;

   
    static void printScoreCard(const DailyScore& ds);
    static void printWeeklyCard(const WeeklyScore& ws);

private:
    const DataStore& m_store;
    double           m_baselineKwh;  

    static std::string badge(int score);
    static std::string motivate(double savingPct, int streak);
};
