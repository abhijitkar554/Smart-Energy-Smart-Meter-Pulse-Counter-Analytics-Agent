# Smart Energy Smart-Meter — Pulse Counter & Analytics Agent

**Wipro Embedded Systems Capstone Project**  
C++14 · SQLite · POSIX signals · Linux/Windows

---

## What This Project Does

A self-contained embedded-style application that:

1. **Counts energy pulses** from a simulated (or real GPIO) source
2. **Converts pulses → kWh → INR cost** using a configurable tariff
3. **Persists every reading** to a SQLite database (WAL mode)
4. **Runs an AI analytics agent** that produces:
   - Next-bill forecast (extrapolated from today's rate)
   - Anomaly detection (Z-score on 30-day rolling window)
   - 14-day consumption trend (linear regression)
   - Night-waste detection (23:00–05:00 window)
   - Gamified daily/weekly energy-saving score (0–100, badges, streaks)

---

## Architecture

```
main.cpp  (signal handling, main loop, SIGUSR1 on-demand report)
    │
    ├── ConfigReader      — flat JSON parser, no third-party lib
    ├── PulseCounter      — atomic uint64_t counter, ISR-safe onPulse()
    ├── PulseSimulator    — background thread: DIURNAL / CONSTANT / RANDOM
    ├── EnergyMeter       — pulse→kWh→cost, steady_clock interval timing
    ├── DataStore         — SQLite WAL, parameterised queries, WAL mode
    │
    ├── AnalyticsEngine   — pure computation layer
    │       computeBillForecast()    linear extrapolation, local midnight
    │       detectAnomaly()          Z-score, real % above mean
    │       computeTrend()           OLS regression, R²
    │       buildHourlyPattern()     per-hour mean ± stddev
    │       computeEfficiencyScore() 0-100 vs baseline
    │
    ├── AnalyticsAgent    — rule-based AI layer, configurable thresholds
    │       BILL / ANOMALY / TREND / DEMAND alerts
    │       Actionable recommendations
    │
    ├── NightWatchdog     — night-window kWh vs prior 6-night average
    └── EnergyChallenge   — gamified score: 5 components, badge, streak
```

---

## Project Structure

```
Smart Meter/
├── include/            Header files (one per class)
├── src/                Implementation files
├── third_party/
│   ├── sqlite/         SQLite 3 amalgamation (sqlite3.h + sqlite3.c)
│   └── mingw-std-threads/  Win32 thread/mutex stubs for old MinGW GCC < 9
├── data/
│   ├── config.json     Primary configuration (edit this)
│   ├── demo_config.json  Short 15 s demo run
│   └── test_config.json  8 s unit-test run
├── GNUmakefile         Linux/macOS build (plain `make`)
├── Makefile            Windows MinGW build (`mingw32-make`)
├── CMakeLists.txt      Cross-platform CMake build
└── build.bat           Windows one-click build + run
```

---

## Building

### Linux / macOS (primary target)

```bash
# Prerequisites: GCC 5+, make
make -f Makefile.linux
./build/bin/SmartEnergyMeter data/config.json
```

Demo run (15 seconds):
```bash
make -f Makefile.linux run CFG=data/demo_config.json
```

### Windows — MinGW

```bat
mingw32-make
build\bin\SmartEnergyMeter.exe data\config.json
```

Or double-click `build.bat`.

### CMake (all platforms)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/SmartEnergyMeter data/config.json
```

---

## Configuration (`data/config.json`)

| Key | Default | Description |
|-----|---------|-------------|
| `meter_id` | `WIPRO-METER-001` | Meter identity string |
| `pulses_per_kwh` | `1000` | Hardware pulse constant |
| `rate_per_kwh` | `7.00` | INR per kWh |
| `standing_charge` | `50.0` | Fixed monthly charge (INR) |
| `cost_alert_threshold` | `1500.0` | Bill WARNING level (INR/month) |
| `reading_interval` | `60` | Seconds between readings |
| `report_interval_sec` | `30` | Seconds between full reports |
| `run_duration_sec` | `300` | `0` = run until Ctrl+C |
| `anomaly_warn_zscore` | `2.0` | Z-score for WARNING |
| `anomaly_crit_zscore` | `3.0` | Z-score for CRITICAL |
| `night_start_hour` | `23` | Night window start (24h) |
| `night_end_hour` | `5` | Night window end (24h) |
| `night_waste_threshold_kwh` | `0.3` | Night alert threshold |
| `sim_profile` | `DIURNAL` | `DIURNAL` / `CONSTANT` / `RANDOM` |
| `sim_base_power_kw` | `1.5` | Base household load |
| `sim_speed_multiplier` | `120.0` | `120` = 1 day in 12 minutes |
| `db_path` | `data/energy.db` | SQLite database file |

---

## Runtime Signals (Linux)

| Signal | Effect |
|--------|--------|
| `Ctrl+C` / `SIGTERM` | Graceful shutdown — saves state, prints final summary |
| `SIGUSR1` | Dumps a full analytics report immediately without waiting for the next scheduled interval |

Send an on-demand report:
```bash
# Find the PID from the startup banner, then:
kill -USR1 <pid>
```

---

## Sample Output

```
[01:11:24]  Total: 0.6290 kWh  |  Power: 1.620 kW  |  Pulses: 629  |  Cost: INR 4.40

==============================================================
  SMART ENERGY METER  --  ANALYTICS REPORT
==============================================================
  Meter  : WIPRO-METER-001
  Time   : 2026-10-03 01:11:24
--------------------------------------------------------------
[LIVE SNAPSHOT]
  Current Power      : 1.620 kW
  Total Energy       : 0.629 kWh
  Interval Energy    : 0.629 kWh
  Total Pulses       : 629
  Cumulative Cost    : 4.40 INR

[NEXT BILL FORECAST]
  Today's avg usage  : 6.80 kWh/day
  Projected monthly  : 204.00 kWh
  Estimated bill     : INR 1478
  Days remaining     : 27

  >> "Your current usage is 6.8 kWh/day. At this rate, estimated monthly
     consumption is 204 kWh and your bill may be approximately INR 1478."

[14-DAY TREND]
  Consumption is stable (slope < 0.05 kWh/day).

[ALERTS]
  [WARNING ] [BILL] Projected monthly bill INR 1478 is above warning threshold (1500).
  [WARNING ] [DEMAND] Peak demand 5.40 kW detected (threshold 5.0 kW).

[RECOMMENDATIONS]
  1. Avoid running AC + geyser + microwave simultaneously.
  2. Shift heavy loads (washing machine) to off-peak hours (10 PM-6 AM).
==============================================================

==============================================================
  ENERGY SAVING CHALLENGE  --  DAILY SCORE CARD
==============================================================

  Today's Energy Score:  82 / 100
  [SILVER] Energy Saver

  Today's usage    : 5.10 kWh
  7-day avg (base) : 5.80 kWh
  Saved            : 12.1%  GREAT JOB!

  Score breakdown:
    Base: 50 pts
    +20 pts: Below 7-day average (5.8 kWh)
    +10 pts: Less than yesterday
    +10 pts: No night-waste alert
    -8  pts: Peak power 5.4 kW under 4.5 kW limit

  Today's challenge:
    [ACHIEVED] Reduce today's consumption by 10% (target: 5.22 kWh, avg: 5.80 kWh)

  >> Great job! You beat your average by 12%.
==============================================================
```

---

## Known Limitations

| Item | Status |
|------|--------|
| **No Linux device driver** | The pulse source is simulated. A real deployment replaces `PulseSimulator` with a character device / GPIO interrupt handler. `PulseCounter::onPulse()` is the ISR-safe entry point. |
| **SQLite is bundled stub** | `third_party/sqlite/sqlite3.c` is a simplified in-memory engine. Drop in the real SQLite amalgamation from [sqlite.org/download.html](https://sqlite.org/download.html) for production use — the API is identical. |
| **Flat-rate tariff only** | Real Indian tariffs are tiered (MSEB slabs). Slab support is the next planned feature. |
| **Single meter** | Architecture supports multiple meter IDs but the main loop only instantiates one. |

---

## Connecting Real Hardware (Raspberry Pi)

Replace the `PulseSimulator` thread with a GPIO interrupt:

```cpp
// Using pigpio (https://abyz.me.uk/rpi/pigpio/)
#include <pigpio.h>

void gpioISR(int gpio, int level, uint32_t tick) {
    if (level == 1)
        counter.onPulse();   // same PulseCounter, ISR-safe
}

gpioInitialise();
gpioSetMode(PULSE_PIN, PI_INPUT);
gpioSetPullUpDown(PULSE_PIN, PI_PUD_UP);
gpioSetAlertFunc(PULSE_PIN, gpioISR);
// Everything else (EnergyMeter, DataStore, AnalyticsAgent) unchanged
```

---

## License

MIT — free to use, modify, and distribute for educational and commercial purposes.
