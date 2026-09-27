message("----- KisakCOD Dedicated(MP) -----")

project(KisakCOD-dedi)

# Include all common files between mp/sp/dedi
include(${SCRIPTS_DIR}/common_files.cmake)

# Include all files specific to mp
include(${SCRIPTS_DIR}/mp/mp_files.cmake)
include(dedi_files.cmake)

# Create the source groups
include(${SCRIPTS_DIR}/source_groups.cmake)

# Handle platform configs and overrides
include (${PLATFORM_DIR}/${KISAK_PLATFORM}/platform.cmake)

# Switch currently builds the platform/graphics bootstrap only. This keeps the
# first Switch build independent from the Win32/D3D9 renderer while the
# platform layer is being ported.
if (KISAK_PLATFORM STREQUAL "switch" AND KISAK_SWITCH_BOOTSTRAP)
    add_executable(${PROJECT_NAME}
        ${SWITCH_SRC}
    )
else()
# Add the new executable
add_executable(${PROJECT_NAME}
    ${SRCROOT}
    ${DYNENTITY} 
    ${EFFECTSCORE} 
    ${AIM_ASSIST} 
    ${BGAME} 
    ${BINKLIB} 
    ${CGAME} 
    ${CGAME_MP} 
    ${CLIENT} 
    ${CLIENT_MP} 
    ${COMMON} 
    ${DATABASE} 
    ${DEVGUI} 
    ${GAME} 
    ${GAME_MP} 
    ${GFX_D3D} 
    ${GROUPVOICE} 
    ${GROUPVOICE_SPEEX} 
    ${MSSLIB} 
    ${ODE} 
    ${PHYSICS} 
    ${PHYSICS_ODE} 
    ${QCOMMON} 
    ${RAGDOLL} 
    ${SCRIPT} 
    ${SERVER} 
    ${SERVER_MP} 
    ${SOUND}
    ${SPEEX}
    ${STRINGED} 
    ${UI} 
    ${UI_MP} 
    ${UNIVERSAL} 
    ${WIN32_SRC}
    ${XANIM} 
    ${ZLIB} 
    ${STEAM_HEADERS}
)

endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC KISAK_MP KISAK_DEDICATED DEDICATED)

# We need all of the pre/post-build commands
if (NOT (KISAK_PLATFORM STREQUAL "switch" AND KISAK_SWITCH_BOOTSTRAP))
include(${SCRIPTS_DIR}/pre_build.cmake)
include(${SCRIPTS_DIR}/post_build.cmake)
endif()

# Tracey
if (NOT KISAK_PLATFORM STREQUAL "switch")
    include(${SCRIPTS_DIR}/extern/tracy.cmake)
endif()