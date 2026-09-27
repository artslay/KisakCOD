# Nintendo Switch platform bootstrap.
#
# The full game still contains Win32/D3D9 dependencies. The bootstrap target
# deliberately does not link against desktop graphics/system libraries.
# Switch-specific dependencies will be added here as their platform layers land.

if (NOT KISAK_PLATFORM STREQUAL "switch")
    message(FATAL_ERROR "KISAK_PLATFORM is incorrect for building switch.")
endif()

message(STATUS "Configuring Nintendo Switch platform")

# This file is included before add_executable() by the existing target files,
# so keep it target-independent.
add_compile_definitions(__SWITCH__ KISAK_SWITCH)
