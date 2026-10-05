#pragma once
#include "PulseCounter.h"
#include <string>


class HardwarePulseReader {
public:
    static constexpr const char* DEVICE_PATH = "/dev/pulse_counter";

    explicit HardwarePulseReader(PulseCounter& counter);
    ~HardwarePulseReader();

    bool open();
    void close();
    bool isOpen() const { return m_fd >= 0; }

   
    bool sync();

    
    bool inject(long pulses);

   
    bool reset();

   
    static bool devicePresent();

private:
    PulseCounter& m_counter;
    int           m_fd;
    uint64_t      m_lastDriverCount;
};
