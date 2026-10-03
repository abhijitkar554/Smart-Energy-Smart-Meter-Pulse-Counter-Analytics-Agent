#pragma once
#include "PulseCounter.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  HardwarePulseReader — reads pulse count from the Linux character device
//                        /dev/pulse_counter created by pulse_counter_driver.ko
//
//  This class is the userspace C++ bridge to the kernel driver.
//  It opens /dev/pulse_counter, reads the current cumulative count, and feeds
//  it into the existing PulseCounter object so all downstream analytics
//  (EnergyMeter, DataStore, AnalyticsAgent) work unchanged.
//
//  Usage:
//    HardwarePulseReader reader(counter);
//    if (reader.open()) {
//        reader.sync();   // call periodically to pull latest count
//    }
//
//  If /dev/pulse_counter is not present (driver not loaded / Windows),
//  open() returns false and the caller falls back to PulseSimulator.
// ─────────────────────────────────────────────────────────────────────────────
class HardwarePulseReader {
public:
    static constexpr const char* DEVICE_PATH = "/dev/pulse_counter";

    explicit HardwarePulseReader(PulseCounter& counter);
    ~HardwarePulseReader();

    // Returns true if /dev/pulse_counter was opened successfully
    bool open();
    void close();
    bool isOpen() const { return m_fd >= 0; }

    // Read the current pulse count from the driver and sync into PulseCounter.
    // Call this on every reading interval instead of using PulseSimulator.
    bool sync();

    // Inject N pulses into the driver (write to device) — for testing
    bool inject(long pulses);

    // Reset driver counter to 0 via ioctl
    bool reset();

    // Static helper: check if the device file exists
    static bool devicePresent();

private:
    PulseCounter& m_counter;
    int           m_fd;
    uint64_t      m_lastDriverCount;
};
