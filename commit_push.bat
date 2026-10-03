@echo off
setlocal
set GIT="C:\Program Files\Git\cmd\git.exe"
cd /d "c:\Users\ommab\OneDrive\Desktop\Smart Meter"

REM ── COMMIT 1: Linux kernel device driver ─────────────────────────────────
echo [1/3] Committing: Linux kernel device driver...
%GIT% add driver\pulse_counter_driver.c driver\Makefile driver\pulse_ioctl.h
%GIT% commit -m "feat: add Linux misc character device driver (pulse_counter_driver.ko)" -m "" -m "Kernel concepts implemented:" -m "  - module_init / module_exit lifecycle" -m "  - misc_register: auto-creates /dev/pulse_counter" -m "  - struct file_operations: open/release/read/write/ioctl" -m "  - atomic_t: interrupt-safe pulse counter" -m "  - copy_to_user / copy_from_user: safe kernel<->userspace transfer" -m "  - DEFINE_MUTEX: read serialisation" -m "  - ioctl: PULSE_IOC_GET / RESET / ADD commands" -m "  - irqreturn_t stub: shows real GPIO IRQ wiring pattern" -m "  - driver/Makefile: obj-m kernel module build system" -m "  - driver/pulse_ioctl.h: shared ioctl defs for kernel and userspace"

REM ── COMMIT 2: HardwarePulseReader + main.cpp integration ──────────────────
echo [2/3] Committing: HardwarePulseReader + main.cpp integration...
%GIT% add include\HardwarePulseReader.h src\HardwarePulseReader.cpp src\main.cpp
%GIT% add CMakeLists.txt Makefile Makefile.linux
%GIT% commit -m "feat: integrate kernel driver with C++ app via HardwarePulseReader" -m "" -m "  - HardwarePulseReader: opens /dev/pulse_counter, uses ioctl GET/ADD/RESET" -m "  - Falls back to PulseSimulator when /dev/pulse_counter not present" -m "  - main.cpp: auto-detects driver presence at startup" -m "  - Makefile.linux: adds HardwarePulseReader.cpp to Linux build" -m "  - CMakeLists.txt: adds HardwarePulseReader.cpp source"

REM ── COMMIT 3: Unit tests ──────────────────────────────────────────────────
echo [3/3] Committing: Unit tests...
%GIT% add tests\test_pulse_counter.cpp tests\Makefile
%GIT% commit -m "test: add unit tests for PulseCounter, EnergyMeter, AnalyticsEngine" -m "" -m "  - 11 tests covering pulse counting, kWh conversion, cost calculation," -m "    interval delta, peak power tracking, efficiency score" -m "  - No third-party framework — plain assert + custom TEST() macro" -m "  - tests/Makefile: builds and runs on Linux with make -C tests run"

REM ── PUSH all 3 commits ────────────────────────────────────────────────────
echo Pushing all 3 commits to GitHub...
%GIT% push origin main

echo.
%GIT% log --oneline --decorate -5
echo.
echo DONE: https://github.com/abhijitkar554/Smart-Energy-Smart-Meter-Pulse-Counter-Analytics-Agent
