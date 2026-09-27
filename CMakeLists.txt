cmake_minimum_required(VERSION 3.16)

# KisakCOD is currently built only for Nintendo Switch.
set(KISAK_PLATFORM "switch" CACHE STRING "KisakCOD target platform" FORCE)

if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (DEFINED ENV{DEVKITPRO} AND EXISTS "$ENV{DEVKITPRO}/cmake/Switch.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/cmake/Switch.cmake"
            CACHE FILEPATH "Nintendo Switch CMake toolchain")
    else()
        message(FATAL_ERROR
            "Nintendo Switch build requires DEVKITPRO and "$DEVKITPRO/cmake/Switch.cmake")
    endif()
endif()

project(KisakCOD)

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)

set(USE_ASAN OFF)
set(KISAK_OPENAL OFF)
set(KISAK_SWITCH_BOOTSTRAP ON CACHE BOOL "Build the minimal Switch bootstrap target" FORCE)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(SCRIPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
set(PLATFORM_DIR ${SCRIPTS_DIR}/platform)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

include(${SCRIPTS_DIR}/extern/increment_build.cmake)

message(STATUS "Building KISAK_PLATFORM: switch")

# Only the Switch bootstrap target is part of the build graph.
add_subdirectory(${SCRIPTS_DIR}/mp)