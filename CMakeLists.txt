cmake_minimum_required(VERSION 3.16)
project(SmartEnergyMeter VERSION 4.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ── SQLite amalgamation ───────────────────────────────────────────────────────
add_library(sqlite3 STATIC
    third_party/sqlite/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_options(sqlite3 PRIVATE
    $<$<C_COMPILER_ID:GNU>:-w>
    $<$<C_COMPILER_ID:Clang>:-w>
    $<$<C_COMPILER_ID:MSVC>:/w>
)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(SmartEnergyMeter
    src/main.cpp
    src/ConfigReader.cpp
    src/PulseCounter.cpp
    src/EnergyMeter.cpp
    src/DataStore.cpp
    src/AnalyticsEngine.cpp
    src/AnalyticsAgent.cpp
    src/PulseSimulator.cpp
    src/NightWatchdog.cpp
    src/EnergyChallenge.cpp
)

target_include_directories(SmartEnergyMeter PRIVATE include)
target_link_libraries(SmartEnergyMeter PRIVATE sqlite3)

# ── Platform-specific thread support ─────────────────────────────────────────
# BUG FIX #10: previous version used target_link_options() with -pthread
# which fails on Linux CMake builds.  The correct approach is to use
# find_package(Threads) and link Threads::Threads on all platforms.
# On old MinGW we use our own Win32 thread stubs (no pthread needed).
if(WIN32 AND MINGW)
    # MinGW GCC < 9 uses Win32 thread stubs — no pthread library required
    target_compile_definitions(SmartEnergyMeter PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(SmartEnergyMeter PRIVATE ws2_32)
    target_link_options(SmartEnergyMeter PRIVATE
        -static-libgcc -static-libstdc++)
elseif(WIN32 AND NOT MINGW)
    # MSVC
    target_compile_definitions(SmartEnergyMeter PRIVATE _WIN32_WINNT=0x0601)
else()
    # Linux / macOS — use pthreads properly
    find_package(Threads REQUIRED)
    target_link_libraries(SmartEnergyMeter PRIVATE Threads::Threads)
    target_compile_options(SmartEnergyMeter PRIVATE -pthread)
endif()

# ── Warnings ──────────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(SmartEnergyMeter PRIVATE /W3)
else()
    target_compile_options(SmartEnergyMeter PRIVATE -Wall -Wextra)
endif()

# ── Copy config to binary directory after build ───────────────────────────────
add_custom_command(TARGET SmartEnergyMeter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:SmartEnergyMeter>/data
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/data/config.json
        $<TARGET_FILE_DIR:SmartEnergyMeter>/data/config.json
    COMMENT "Copying config.json to output directory"
)

message(STATUS "SmartEnergyMeter v${PROJECT_VERSION}")
message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler   : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "System     : ${CMAKE_SYSTEM_NAME}")
