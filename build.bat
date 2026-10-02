@echo off
REM =============================================================================
REM  Smart Energy Meter -- One-click build + run (MinGW)
REM  Wipro Embedded Systems Capstone Project
REM  Double-click this file or run from cmd in the project folder.
REM =============================================================================

title Smart Energy Meter -- Build

echo.
echo  =====================================================================
echo  Smart Energy Meter  --  Wipro Embedded Capstone
echo  =====================================================================
echo.

REM ── Check for g++ ─────────────────────────────────────────────────────────
where g++ >nul 2>&1
if errorlevel 1 (
    echo  [ERROR] g++ not found.
    echo  Please install MinGW-w64 and add its bin directory to PATH.
    echo  Download: https://www.mingw-w64.org/downloads/
    pause
    exit /b 1
)

REM ── Check for mingw32-make ─────────────────────────────────────────────────
where mingw32-make >nul 2>&1
if errorlevel 1 (
    echo  [ERROR] mingw32-make not found. Ensure MinGW-w64 is on PATH.
    pause
    exit /b 1
)

echo  [INFO] Compiler found:
g++ --version 2>&1 | findstr "g++"
echo.

REM ── Clean previous build ──────────────────────────────────────────────────
echo  [BUILD] Cleaning previous artefacts...
mingw32-make clean >nul 2>&1

REM ── Build ─────────────────────────────────────────────────────────────────
echo  [BUILD] Compiling all modules...
echo          (ConfigReader, PulseCounter, EnergyMeter, DataStore,
echo           AnalyticsEngine, AnalyticsAgent, PulseSimulator,
echo           NightWatchdog, EnergyChallenge, main)
echo.
mingw32-make 2>&1
if errorlevel 1 (
    echo.
    echo  [ERROR] Build FAILED. See errors above.
    pause
    exit /b 1
)

echo.
echo  =====================================================================
echo  Build complete! Starting Smart Energy Meter...
echo  =====================================================================
echo.

REM ── Run from project root so data/config.json is found ────────────────────
cd /d "%~dp0"
build\bin\SmartEnergyMeter.exe data\config.json

echo.
echo  [DONE] Program exited. Press any key to close.
pause
