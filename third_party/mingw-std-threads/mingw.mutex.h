/**
 * @file mingw.mutex.h
 * @brief std::mutex / std::lock_guard for MinGW (GCC < 9) on Windows.
 *
 * Condensed, self-contained implementation using Win32 CRITICAL_SECTION.
 * Derived from the mingw-std-threads project (MIT License).
 * Original: https://github.com/meganz/mingw-std-threads
 */
#pragma once
#ifndef _GLIBCXX_HAS_GTHREADS

#include <windows.h>

namespace std {

class mutex {
public:
    mutex()          { InitializeCriticalSection(&m_cs); }
    ~mutex()         { DeleteCriticalSection(&m_cs); }
    mutex(const mutex&)            = delete;
    mutex& operator=(const mutex&) = delete;

    void lock()   { EnterCriticalSection(&m_cs); }
    void unlock() { LeaveCriticalSection(&m_cs); }
    bool try_lock() {
        return TryEnterCriticalSection(&m_cs) != 0;
    }

    CRITICAL_SECTION* native_handle() { return &m_cs; }

private:
    CRITICAL_SECTION m_cs;
};

template<class Mutex>
class lock_guard {
public:
    typedef Mutex mutex_type;
    explicit lock_guard(Mutex& m) : m_mutex(m) { m_mutex.lock(); }
    ~lock_guard() { m_mutex.unlock(); }
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
private:
    Mutex& m_mutex;
};

template<class Mutex>
class unique_lock {
public:
    typedef Mutex mutex_type;

    unique_lock() : m_mutex(nullptr), m_owns(false) {}
    explicit unique_lock(Mutex& m) : m_mutex(&m), m_owns(false) {
        m_mutex->lock();
        m_owns = true;
    }
    ~unique_lock() { if (m_owns) m_mutex->unlock(); }

    void lock()   { m_mutex->lock();   m_owns = true;  }
    void unlock() { m_mutex->unlock(); m_owns = false; }
    bool owns_lock() const { return m_owns; }

    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;

private:
    Mutex* m_mutex;
    bool   m_owns;
};

} // namespace std

#endif // _GLIBCXX_HAS_GTHREADS
