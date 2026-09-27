cmake_minimum_required(VERSION 3.16)

project(KisakCOD)

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)

# Target platform. Keep win32 as the default for existing builds.
set(KISAK_PLATFORM "win32" CACHE STRING "KisakCOD target platform")
set_property(CACHE KISAK_PLATFORM PROPERTY STRINGS win32 linux switch)

set(USE_ASAN OFF)
set(KISAK_OPENAL OFF)

# Switch is bootstrapped separately until the game-wide Win32 dependencies have
# been replaced by platform-neutral implementations.
option(KISAK_SWITCH_BOOTSTRAP "Build the minimal Switch bootstrap target" ON)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(SCRIPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
set(PLATFORM_DIR ${SCRIPTS_DIR}/platform)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

include(${SCRIPTS_DIR}/extern/increment_build.cmake)
include(${SCRIPTS_DIR}/platform_override.cmake)

message("Building KISAK_PLATFORM: ${KISAK_PLATFORM}")

add_subdirectory(${SCRIPTS_DIR}/mp)
add_subdirectory(${SCRIPTS_DIR}/sp)
add_subdirectory(${SCRIPTS_DIR}/dedi)

if (KISAK_PLATFORM STREQUAL "win32")
    add_subdirectory(${SCRIPTS_DIR}/radiant)
endif()
