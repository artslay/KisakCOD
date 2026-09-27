# Nintendo Switch platform bootstrap.
#
# The full game still contains Win32/D3D9 dependencies. The bootstrap target
# deliberately does not link against any desktop graphics/system libraries.
# Switch-specific dependencies will be added here as their platform layers land.

message(STATUS "Configuring Nintendo Switch platform")

target_compile_definitions(${PROJECT_NAME} PRIVATE __SWITCH__ KISAK_SWITCH)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${SRC_DIR}"
)

# devkitPro's toolchain supplies the platform compiler/runtime. Do not add
# desktop Win32 flags or libraries here.
