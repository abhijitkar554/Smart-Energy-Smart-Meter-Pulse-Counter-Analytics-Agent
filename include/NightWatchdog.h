#pragma once
#include "DataStore.h"
#include <string>
#include <vector>


class NightWatchdog {
public:
    struct NightReport {
        bool   alertTriggered{false};
        double consumedKwh{0};
        int    startHour{23};
        int    endHour{5};
        double threshold{0.3};
        double averageNightKwh{0};    
        double percentAboveAvg{0};
        std::string message;
        std::vector<std::string> suspects; 
    };

    NightWatchdog(const DataStore& store,
                  int    nightStartHour   = 23,
                  int    nightEndHour     = 5,
                  double thresholdKwh     = 0.3);

    NightReport evaluate() const;

   
    bool isNightTime() const;

private:
    double getNightKwhLast(int nights) const;

    const DataStore& m_store;
    int    m_startHour;
    int    m_endHour;
    double m_threshold;
};
