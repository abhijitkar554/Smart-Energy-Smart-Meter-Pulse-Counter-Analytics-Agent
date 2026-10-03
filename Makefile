# =============================================================================
#  Smart Energy Meter -- MinGW Makefile
#  Usage:  mingw32-make          -> builds Release exe
#          mingw32-make clean    -> removes build artefacts
#          mingw32-make run      -> build + run
# =============================================================================
SHELL := cmd.exe
CXX      = g++
CC       = gcc

# C++14 for broadest MinGW compatibility; all code written to C++14 standard
CXXFLAGS = -std=c++14 -O2 -Wall -Wextra \
           -Iinclude -Ithird_party/sqlite \
           -DSQLITE_THREADSAFE=1

CFLAGS   = -O2 -DSQLITE_THREADSAFE=1

# Link flags: static stdlib so the .exe runs without MinGW DLLs
# NOTE: No -lpthread -- we use Win32 threads via mingw-std-threads stubs
LDFLAGS  = -static-libgcc -static-libstdc++ -lws2_32

OUT_DIR  = build\bin
OBJ_DIR  = build\obj
TARGET   = $(OUT_DIR)\SmartEnergyMeter.exe

# ── Sources ───────────────────────────────────────────────────────────────────
SRCS = src\main.cpp            \
       src\ConfigReader.cpp    \
       src\PulseCounter.cpp    \
       src\EnergyMeter.cpp     \
       src\DataStore.cpp       \
       src\AnalyticsEngine.cpp \
       src\AnalyticsAgent.cpp  \
       src\PulseSimulator.cpp  \
       src\NightWatchdog.cpp   \
	   src/HardwarePulseReader.cpp \
       src\EnergyChallenge.cpp \


SQLITE_SRC = third_party\sqlite\sqlite3.c
SQLITE_OBJ = $(OBJ_DIR)\sqlite3.o

OBJS = $(patsubst src\%.cpp,$(OBJ_DIR)\%.o,$(SRCS))

# ── Default target ────────────────────────────────────────────────────────────
all: dirs $(TARGET)
	@echo.
	@echo  =====================================================
	@echo  Build SUCCESS: $(TARGET)
	@echo  Run with:      $(TARGET) data\config.json
	@echo  =====================================================

# ── Create output directories ─────────────────────────────────────────────────
dirs:
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
	@if not exist $(OUT_DIR) mkdir $(OUT_DIR)
	@if not exist data       mkdir data

# ── Link ──────────────────────────────────────────────────────────────────────
$(TARGET): $(OBJS) $(SQLITE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# ── Compile C++ sources ───────────────────────────────────────────────────────
$(OBJ_DIR)\%.o: src\%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ── Compile SQLite (C source) ─────────────────────────────────────────────────
$(SQLITE_OBJ): $(SQLITE_SRC)
	$(CC) $(CFLAGS) -Ithird_party/sqlite -c -o $@ $<

# ── Run ───────────────────────────────────────────────────────────────────────
run: all
	$(TARGET) data\config.json

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	@if exist $(OBJ_DIR) rmdir /S /Q $(OBJ_DIR)
	@if exist $(OUT_DIR) rmdir /S /Q $(OUT_DIR)
	@echo  Cleaned.

.PHONY: all dirs run clean
