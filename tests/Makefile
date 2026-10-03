# =============================================================================
#  tests/Makefile — Unit Test Build (Linux/macOS)
#  Usage: cd tests && make && ./test_pulse_counter
# =============================================================================

CXX      = g++
CXXFLAGS = -std=c++14 -O0 -g -Wall -Wextra \
           -I../include -I../third_party/sqlite \
           -DSQLITE_THREADSAFE=1 \
           -pthread

LDFLAGS  = -pthread

SRCS = test_pulse_counter.cpp \
       ../src/PulseCounter.cpp \
       ../src/EnergyMeter.cpp  \
       ../src/DataStore.cpp    \
       ../src/AnalyticsEngine.cpp \
       ../src/ConfigReader.cpp

SQLITE_SRC = ../third_party/sqlite/sqlite3.c
SQLITE_OBJ = sqlite3_test.o

TARGET   = test_pulse_counter

all: $(TARGET)
	@echo "Build OK. Run: ./$(TARGET)"

$(TARGET): $(SRCS) $(SQLITE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(SQLITE_OBJ): $(SQLITE_SRC)
	gcc -O0 -DSQLITE_THREADSAFE=1 -I../third_party/sqlite -w -c -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(SQLITE_OBJ)

.PHONY: all run clean
