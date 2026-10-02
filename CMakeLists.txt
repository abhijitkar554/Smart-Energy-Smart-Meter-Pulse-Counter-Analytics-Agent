cmake_minimum_required(VERSION 3.16)
project(SmartEnergyMeter VERSION 3.0.0 LANGUAGES CXX C)

# C++14 for broadest GCC/MSVC compatibility
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Output directories ────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ── SQLite (bundled stub / amalgamation) ──────────────────────────────────────
add_library(sqlite3 STATIC
    third_party/sqlite/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
# Suppress warnings from the C amalgamation
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

# ── Platform thread support ───────────────────────────────────────────────────
if(WIN32)
    target_compile_definitions(SmartEnergyMeter PRIVATE _WIN32_WINNT=0x0601)
    # MinGW needs pthread linked explicitly
    if(MINGW)
        target_compile_options(SmartEnergyMeter PRIVATE -pthread)
        target_link_options(SmartEnergyMeter PRIVATE -pthread
                            -static-libgcc -static-libstdc++)
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(SmartEnergyMeter PRIVATE Threads::Threads)
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(SmartEnergyMeter PRIVATE /W3)
else()
    target_compile_options(SmartEnergyMeter PRIVATE -Wall -Wextra)
endif()

# ── Copy config to binary dir after build ─────────────────────────────────────
add_custom_command(TARGET SmartEnergyMeter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:SmartEnergyMeter>/data
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/data/config.json
        $<TARGET_FILE_DIR:SmartEnergyMeter>/data/config.json
    COMMENT "Copying config.json to output directory"
)

message(STATUS "SmartEnergyMeter v${PROJECT_VERSION} configured")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler:   ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
