#pragma once
#include "PulseCounter.h"
#include "threading_compat.h"
#include <chrono>
#include <vector>
#include <string>
#include <ctime>


class EnergyMeter {
public:
    struct TariffConfig {
        double      ratePerKwh     = 7.00;   
        double      standingCharge = 50.0;    
        std::string currency       = "INR";
    };

    struct Reading {
        std::string meterId;
        std::time_t ts{0};
        double      energyKwh{0};    
        double      intervalKwh{0};
        double      powerKw{0};      
        double      cost{0};         
        uint64_t    pulseCount{0};
    };

    EnergyMeter(PulseCounter& counter, const std::string& meterId);

    void         setTariff(const TariffConfig& t);
    TariffConfig getTariff() const;

   
    Reading takeReading();

   
    Reading lastReading() const;


    std::vector<Reading> drainPendingReadings();

    double getTotalCost()   const;
    double getPeakPowerKW() const;
    double getMinPowerKW()  const;
    void   resetPeakMin();

    const std::string& getMeterId() const { return m_meterId; }

private:
    PulseCounter&      m_counter;
    std::string        m_meterId;
    TariffConfig       m_tariff;
    mutable std::mutex m_readMutex;

    double      m_prevEnergyKwh{0};
  
    std::chrono::steady_clock::time_point m_prevTime;
    bool        m_firstReading{true};

    double      m_totalCost{0};
    double      m_peakKw{0};
    double      m_minKw{1e18};

    Reading              m_lastReading;
    std::vector<Reading> m_pending;
};
