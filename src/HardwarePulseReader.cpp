#include "HardwarePulseReader.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#ifdef __linux__
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <sys/stat.h>
/* ioctl command numbers — mirror of driver/pulse_ioctl.h without linux/ioctl.h */
#  define PULSE_IOC_MAGIC  'P'
#  define PULSE_IOC_GET    _IOR(PULSE_IOC_MAGIC, 0, long)
#  define PULSE_IOC_RESET  _IO (PULSE_IOC_MAGIC, 1)
#  define PULSE_IOC_ADD    _IOW(PULSE_IOC_MAGIC, 2, long)
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  HardwarePulseReader — C++ userspace interface to /dev/pulse_counter
// ─────────────────────────────────────────────────────────────────────────────

HardwarePulseReader::HardwarePulseReader(PulseCounter& counter)
    : m_counter(counter), m_fd(-1), m_lastDriverCount(0) {}

HardwarePulseReader::~HardwarePulseReader() {
    close();
}

bool HardwarePulseReader::devicePresent() {
#ifdef __linux__
    struct stat st;
    return (stat(DEVICE_PATH, &st) == 0);
#else
    return false;   // driver only exists on Linux
#endif
}

bool HardwarePulseReader::open() {
#ifdef __linux__
    m_fd = ::open(DEVICE_PATH, O_RDWR);
    if (m_fd < 0) {
        std::cerr << "[HW] Cannot open " << DEVICE_PATH
                  << ": " << strerror(errno) << "\n";
        return false;
    }
    std::cout << "[HW] Opened " << DEVICE_PATH
              << " — using kernel driver for pulse counting\n";
    // Sync once immediately to get the current count
    sync();
    return true;
#else
    std::cerr << "[HW] HardwarePulseReader: not supported on this platform\n";
    return false;
#endif
}

void HardwarePulseReader::close() {
#ifdef __linux__
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

bool HardwarePulseReader::sync() {
#ifdef __linux__
    if (m_fd < 0) return false;

    // Use ioctl PULSE_IOC_GET for atomic read (preferred over read())
    long driverCount = 0;
    if (::ioctl(m_fd, PULSE_IOC_GET, &driverCount) < 0) {
        // Fall back to read() if ioctl fails (e.g. old driver)
        char buf[32];
        lseek(m_fd, 0, SEEK_SET);
        ssize_t n = ::read(m_fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            std::cerr << "[HW] read failed: " << strerror(errno) << "\n";
            return false;
        }
        buf[n] = '\0';
        driverCount = atol(buf);
    }

    uint64_t newCount = static_cast<uint64_t>(driverCount);

    // Only add the DELTA since last sync — avoids double-counting
    if (newCount > m_lastDriverCount) {
        uint64_t delta = newCount - m_lastDriverCount;
        m_counter.addPulses(delta);
    } else if (newCount < m_lastDriverCount) {
        // Driver was reset externally — treat as fresh start
        m_counter.addPulses(newCount);
    }

    m_lastDriverCount = newCount;
    return true;
#else
    return false;
#endif
}

bool HardwarePulseReader::inject(long pulses) {
#ifdef __linux__
    if (m_fd < 0) return false;
    // Use ioctl PULSE_IOC_ADD for atomic injection
    if (::ioctl(m_fd, PULSE_IOC_ADD, &pulses) < 0) {
        // Fall back to write()
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld\n", pulses);
        if (::write(m_fd, buf, strlen(buf)) < 0) {
            std::cerr << "[HW] inject write failed: " << strerror(errno) << "\n";
            return false;
        }
    }
    return true;
#else
    (void)pulses;
    return false;
#endif
}

bool HardwarePulseReader::reset() {
#ifdef __linux__
    if (m_fd < 0) return false;
    if (::ioctl(m_fd, PULSE_IOC_RESET) < 0) {
        std::cerr << "[HW] reset ioctl failed: " << strerror(errno) << "\n";
        return false;
    }
    m_lastDriverCount = 0;
    return true;
#else
    return false;
#endif
}
