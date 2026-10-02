#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  threading_compat.h
//  Portable threading headers for old MinGW (GCC 6.x) AND modern compilers.
//
//  MinGW.org GCC 6.3 ships WITHOUT functional <thread> / <mutex>.
//  We detect this and pull in the Win32-backed stubs instead.
//  On GCC 9+, Clang, MSVC the real headers are used.
// ─────────────────────────────────────────────────────────────────────────────

#if defined(__MINGW32__) && !defined(__MINGW64__) && (__GNUC__ < 9)
    // Old 32-bit MinGW.org — use Win32-backed stubs
    #include "../third_party/mingw-std-threads/mingw.mutex.h"
    #include "../third_party/mingw-std-threads/mingw.thread.h"
#else
    #include <mutex>
    #include <thread>
#endif
