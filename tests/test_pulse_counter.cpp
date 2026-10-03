// =============================================================================
//  tests/test_pulse_counter.cpp — Unit Tests
//  Smart Energy Meter — Wipro Embedded Systems Capstone Project
//
//  Tests covered:
//    1.  PulseCounter: single pulse increment
//    2.  PulseCounter: bulk pulse add
//    3.  PulseCounter: kWh conversion (1000 pulses = 1.0 kWh)
//    4.  PulseCounter: base count restoration (reboot recovery)
//    5.  PulseCounter: callback fires at correct threshold
//    6.  EnergyMeter:  cost calculation (1 kWh @ INR 7.00 = INR 7.00)
//    7.  EnergyMeter:  interval kWh delta tracking
//    8.  EnergyMeter:  peak power tracking
//    9.  AnalyticsEngine: efficiency score at baseline = 50
//    10. AnalyticsEngine: efficiency score at 0 kWh = 100
//
//  Build and run:
//    cd tests && make && ./test_pulse_counter
// =============================================================================

#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <chrono>

#include "../include/PulseCounter.h"
#include "../include/EnergyMeter.h"
#include "../include/DataStore.h"
#include "../include/AnalyticsEngine.h"

// ── Simple test framework ─────────────────────────────────────────────────────
static int g_pass = 0;
static int g_fail = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        std::cout << "  [PASS] " << name << "\n"; \
        g_pass++; \
    } else { \
        std::cout << "  [FAIL] " << name << "  (line " << __LINE__ << ")\n"; \
        g_fail++; \
    } \
} while(0)

#define APPROX(a, b, eps) (std::fabs((a) - (b)) < (eps))

// ── Test 1-5: PulseCounter ────────────────────────────────────────────────────
static void testPulseCounter() {
    std::cout << "\n[PulseCounter]\n";

    // 1. Single pulse
    {
        PulseCounter pc(1000);
        pc.onPulse();
        TEST("single pulse increments to 1", pc.getTotalPulses() == 1);
    }

    // 2. Bulk add
    {
        PulseCounter pc(1000);
        pc.addPulses(500);
        TEST("bulk add 500", pc.getTotalPulses() == 500);
    }

    // 3. kWh conversion: 1000 pulses / 1000 pulses-per-kWh = 1.0 kWh
    {
        PulseCounter pc(1000);
        pc.addPulses(1000);
        TEST("1000 pulses = 1.0 kWh", APPROX(pc.getTotalEnergyKwh(), 1.0, 1e-9));
    }

    // 4. Partial kWh: 500 pulses = 0.5 kWh
    {
        PulseCounter pc(1000);
        pc.addPulses(500);
        TEST("500 pulses = 0.5 kWh", APPROX(pc.getTotalEnergyKwh(), 0.5, 1e-9));
    }

    // 5. Callback fires exactly at threshold
    {
        PulseCounter pc(1000);
        int fired = 0;
        pc.setCallback([&](uint64_t) { fired++; }, 100);
        pc.addPulses(100);
        TEST("callback fires at 100 pulses", fired == 1);
        pc.addPulses(100);
        TEST("callback fires again at 200", fired == 2);
    }

    // 6. Base count restoration
    {
        PulseCounter pc(1000);
        pc.setBaseCount(5000);
        TEST("base count stored", pc.getBaseCount() == 5000);
    }
}

// ── Test 6-8: EnergyMeter ─────────────────────────────────────────────────────
static void testEnergyMeter() {
    std::cout << "\n[EnergyMeter]\n";

    // 7. Cost calculation: 1 kWh @ INR 7.00 = INR 7.00
    {
        PulseCounter pc(1000);
        EnergyMeter  em(pc, "TEST-001");
        EnergyMeter::TariffConfig t;
        t.ratePerKwh = 7.0;
        t.currency   = "INR";
        em.setTariff(t);
        pc.addPulses(1000);   // 1 kWh

        // Allow steady_clock guard to pass (> 200 ms since construction)
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        auto r = em.takeReading();
        TEST("1 kWh costs INR 7.00", APPROX(r.cost, 7.0, 0.001));
    }

    // 8. Interval delta
    {
        PulseCounter pc(1000);
        EnergyMeter  em(pc, "TEST-002");
        pc.addPulses(300);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        auto r1 = em.takeReading();
        pc.addPulses(200);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        auto r2 = em.takeReading();
        TEST("interval kWh = 0.2", APPROX(r2.intervalKwh, 0.2, 0.001));
    }

    // 9. Peak power tracking
    {
        PulseCounter pc(1000);
        EnergyMeter  em(pc, "TEST-003");
        pc.addPulses(100);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        em.takeReading();
        pc.addPulses(500);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        em.takeReading();
        TEST("peak power > 0", em.getPeakPowerKW() > 0.0);
    }
}

// ── Test 9-10: AnalyticsEngine ────────────────────────────────────────────────
static void testAnalyticsEngine() {
    std::cout << "\n[AnalyticsEngine]\n";

    DataStore      db(":memory:");   // in-memory SQLite for tests
    AnalyticsEngine eng(db);

    // 9. Efficiency score at exactly baseline = 50
    {
        int score = eng.computeEfficiencyScore(5.0, 5.0);
        TEST("efficiency score at baseline = 50", score == 50);
    }

    // 10. Efficiency score at 0 kWh = 100
    {
        int score = eng.computeEfficiencyScore(0.0, 5.0);
        TEST("efficiency score at 0 kWh = 100", score == 100);
    }

    // 11. Efficiency score at 2x baseline = 0
    {
        int score = eng.computeEfficiencyScore(10.0, 5.0);
        TEST("efficiency score at 2x baseline = 0", score == 0);
    }
}

// ── main ──────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "==============================================\n";
    std::cout << "  Smart Energy Meter — Unit Tests\n";
    std::cout << "  Wipro Embedded Capstone Project\n";
    std::cout << "==============================================\n";

    testPulseCounter();
    testEnergyMeter();
    testAnalyticsEngine();

    std::cout << "\n==============================================\n";
    std::cout << "  Results: " << g_pass << " passed, "
              << g_fail << " failed\n";
    std::cout << "==============================================\n";

    return (g_fail == 0) ? 0 : 1;
}
