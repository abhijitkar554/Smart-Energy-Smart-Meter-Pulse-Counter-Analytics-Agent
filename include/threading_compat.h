#pragma once


#if defined(__MINGW32__) && !defined(__MINGW64__) && (__GNUC__ < 9)
   
    #include "../third_party/mingw-std-threads/mingw.mutex.h"
    #include "../third_party/mingw-std-threads/mingw.thread.h"
#else
    #include <mutex>
    #include <thread>
#endif
