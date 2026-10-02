/**
 * @file mingw.thread.h
 * @brief std::thread implementation for MinGW (GCC < 9) on Windows.
 *
 * This is a condensed, self-contained implementation of std::thread
 * using Win32 threads, derived from the mingw-std-threads project.
 * Original: https://github.com/meganz/mingw-std-threads (MIT License)
 *
 * MIT License — free to use in any project.
 */
#pragma once
#ifndef _GLIBCXX_HAS_GTHREADS

#include <windows.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <utility>
#include <tuple>
#include <cstdint>
#include <process.h>

namespace std {

class thread {
public:
    typedef HANDLE native_handle_type;

    class id {
        DWORD m_id{0};
    public:
        id() = default;
        explicit id(DWORD d) : m_id(d) {}
        bool operator==(const id& o) const { return m_id == o.m_id; }
        bool operator!=(const id& o) const { return m_id != o.m_id; }
        bool operator< (const id& o) const { return m_id <  o.m_id; }
        friend class thread;
    };

    thread() : m_handle(nullptr), m_id() {}

    template<class F, class... Args>
    explicit thread(F&& f, Args&&... args) {
        // Pack function + args into a heap object
        auto fn = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto* p = new std::function<void()>(std::move(fn));
        DWORD tid = 0;
        m_handle = CreateThread(nullptr, 0, threadProc,
                                static_cast<LPVOID>(p), 0, &tid);
        if (!m_handle) {
            delete p;
            throw std::runtime_error("thread: CreateThread failed");
        }
        m_id = id(tid);
    }

    ~thread() {
        if (joinable()) {
            // Detach if not joined
            CloseHandle(m_handle);
        }
    }

    thread(const thread&) = delete;
    thread& operator=(const thread&) = delete;

    thread(thread&& o) : m_handle(o.m_handle), m_id(o.m_id) {
        o.m_handle = nullptr;
        o.m_id = id();
    }
    thread& operator=(thread&& o) {
        if (joinable()) CloseHandle(m_handle);
        m_handle = o.m_handle;
        m_id     = o.m_id;
        o.m_handle = nullptr;
        o.m_id = id();
        return *this;
    }

    bool joinable() const { return m_handle != nullptr; }

    void join() {
        WaitForSingleObject(m_handle, INFINITE);
        CloseHandle(m_handle);
        m_handle = nullptr;
        m_id = id();
    }

    void detach() {
        CloseHandle(m_handle);
        m_handle = nullptr;
        m_id = id();
    }

    id get_id() const { return m_id; }
    native_handle_type native_handle() { return m_handle; }

    static unsigned hardware_concurrency() {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return static_cast<unsigned>(si.dwNumberOfProcessors);
    }

private:
    static DWORD WINAPI threadProc(LPVOID param) {
        auto* fn = static_cast<std::function<void()>*>(param);
        try { (*fn)(); } catch (...) {}
        delete fn;
        return 0;
    }

    HANDLE m_handle;
    id     m_id;
};

namespace this_thread {
    inline thread::id get_id() {
        return thread::id(GetCurrentThreadId());
    }

    inline void yield() { Sleep(0); }

    template<class Rep, class Period>
    inline void sleep_for(const std::chrono::duration<Rep, Period>& d) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);
        if (ms.count() > 0)
            Sleep(static_cast<DWORD>(ms.count()));
        else
            Sleep(0);
    }

    template<class Clock, class Duration>
    inline void sleep_until(const std::chrono::time_point<Clock, Duration>& tp) {
        sleep_for(tp - Clock::now());
    }
}

} // namespace std

#endif // _GLIBCXX_HAS_GTHREADS
