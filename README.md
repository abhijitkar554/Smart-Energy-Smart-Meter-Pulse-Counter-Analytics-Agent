# Smart Energy Smart-Meter — Pulse Counter & Analytics Agent

**Wipro Embedded Systems Capstone Project**

A fully self-contained **C++14** application that simulates a smart electricity
meter, counts energy pulses, persists readings to SQLite, and runs an AI
analytics agent that detects anomalies, forecasts bills, monitors night-waste,
and gamifies energy saving.

---

## Features

| Feature | Module | Description |
|---|---|---|
| 🔢 Pulse Counter | `PulseCounter` | Thread-safe atomic pulse accumulation with ring-buffer event log |
| ⚡ Energy Meter | `EnergyMeter` | Tariff pricing, interval readings, peak/min demand tracking |
| 💾 Data Store | `DataStore` | SQLite WAL-mode persistence — readings + meter state |
| 🔮 Bill Prediction | `AnalyticsEngine` | Projects monthly kWh and INR bill from current usage rate |
| 🚨 Anomaly Detection | `AnalyticsEngine` | Z-score based detection — flags unusual consumption spikes |
| 🌙 Night Watchdog | `NightWatchdog` | Alerts when unnecessary loads run during sleeping hours |
| 🏆 Energy Challenge | `EnergyChallenge` | Gamified daily/weekly scoring with badges and streak tracking |
| 🤖 Analytics Agent | `AnalyticsAgent` | AI reasoning layer — generates alerts and recommendations |
| 🎮 Simulator | `PulseSimulator` | DIURNAL / CONSTANT / RANDOM load profiles at configurable speed |

---

## Architecture

```
main.cpp
  ├── ConfigReader      (parses data/config.json)
  ├── PulseCounter      (thread-safe pulse accumulation)
  ├── EnergyMeter       (tariff, readings, cost)
  ├── DataStore         (SQLite persistence)
  ├── AnalyticsEngine   (stats, trend, bill forecast, anomaly)
  ├── AnalyticsAgent    (alerts, recommendations, report)
  ├── PulseSimulator    (background thread, realistic load profile)
  ├── NightWatchdog     (night-time waste detection)
  └── EnergyChallenge   (gamified scoring)
```

---

## Project Structure

```
Smart Meter/
├── include/                    # All header files
│   ├── ConfigReader.h
│   ├── PulseCounter.h
│   ├── EnergyMeter.h
│   ├── DataStore.h
│   ├── AnalyticsEngine.h
│   ├── AnalyticsAgent.h
│   ├── PulseSimulator.h
│   ├── NightWatchdog.h
│   ├── EnergyChallenge.h
│   └── threading_compat.h      # MinGW GCC 6.x thread/mutex compatibility
├── src/                        # All implementation files
│   ├── main.cpp
│   ├── ConfigReader.cpp
│   ├── PulseCounter.cpp
│   ├── EnergyMeter.cpp
│   ├── DataStore.cpp
│   ├── AnalyticsEngine.cpp
│   ├── AnalyticsAgent.cpp
│   ├── PulseSimulator.cpp
│   ├── NightWatchdog.cpp
│   └── EnergyChallenge.cpp
├── third_party/
│   ├── sqlite/                 # SQLite amalgamation (sqlite3.h + sqlite3.c)
│   └── mingw-std-threads/      # Win32 thread/mutex stubs for old MinGW
├── data/
│   └── config.json             # All user configuration
├── Makefile                    # MinGW build (no CMake required)
├── CMakeLists.txt              # CMake build (MSVC / GCC / Clang)
└── build.bat                   # One-click Windows build + run
```

---

## Quick Start

### Prerequisites

| Tool | Version |
|---|---|
| C++ Compiler | GCC 6+ / MSVC 2019+ (C++14) |
| MinGW-w64 | Any (for Windows Makefile build) |
| CMake | 3.16+ (optional) |

### Build & Run (Windows — MinGW)

```bat
REM Double-click build.bat  OR run from cmd:
mingw32-make
build\bin\SmartEnergyMeter.exe data\config.json
```

### Build (CMake — cross-platform)

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/SmartEnergyMeter data/config.json
```

---

## Configuration (`data/config.json`)

```json
{
    "meter_id":                   "WIPRO-METER-001",
    "pulses_per_kwh":             1000,
    "rate_per_kwh":               7.00,
    "standing_charge":            50.0,
    "currency":                   "INR",
    "reading_interval":           60,
    "report_interval_sec":        30,
    "run_duration_sec":           300,
    "night_start_hour":           23,
    "night_end_hour":             5,
    "sim_profile":                "DIURNAL",
    "sim_base_power_kw":          1.5,
    "sim_speed_multiplier":       120.0
}
```

### Key Parameters

| Parameter | Effect |
|---|---|
| `sim_speed_multiplier` | `120` = 1 simulated day in 12 real minutes |
| `sim_profile` | `DIURNAL` = realistic Indian household curve |
| `run_duration_sec` | `0` = run indefinitely until Ctrl+C |
| `rate_per_kwh` | Indian residential tariff (INR/kWh) |

---

## Sample Output

```
[15:08:59] Total: 0.0540 kWh  |  Power: 1.620 kW  |  Pulses: 54  |  Cost: INR 0.38

==============================================================
  SMART ENERGY METER  --  ANALYTICS REPORT
==============================================================
[NEXT BILL FORECAST]
  Today's avg usage  : 6.80 kWh/day
  Projected monthly  : 204.00 kWh
  Estimated bill     : INR 1,478

  >> "Your current usage is 6.8 kWh/day. At this rate, estimated monthly
     consumption is 204 kWh and your bill may be approximately INR 1,428."

[ALERTS]
  [WARNING] [ANOMALY] Your energy consumption between 2:00-3:00 is 42%
            higher than usual. Check your AC/refrigerator/water heater.

[NIGHT WATCHDOG]
  Night Energy Alert: 0.9 kWh consumed between 23:00-5:00.
  You may have devices running unnecessarily.

==============================================================
  ENERGY SAVING CHALLENGE  --  DAILY SCORE CARD
==============================================================
  Today's Energy Score:  82 / 100
  [SILVER] Energy Saver

  Saved  :  12.0%  GREAT JOB!
  >> Great job! You beat your average by 12%.
==============================================================
```

---

## Extending to Real Hardware

Replace `PulseSimulator` with a hardware GPIO reader:

```cpp
// Raspberry Pi (pigpio library)
void gpioCallback(int gpio, int level, uint32_t tick) {
    if (level == 1)           // rising edge
        counter.onPulse();    // same PulseCounter interface
}
gpioSetAlertFunc(PULSE_PIN, gpioCallback);
```

Everything else — `EnergyMeter → DataStore → AnalyticsEngine → AnalyticsAgent` — is hardware-agnostic.

---

## License

MIT — free to use, modify, and distribute.
