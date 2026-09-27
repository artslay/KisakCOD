# Nintendo Switch build for KisakCOD.
# Requires devkitPro/libnx and Mesa Switch OpenGL/EGL libraries.
#
# The source set is generated from the engine tree instead of duplicating the
# old CMake source lists. Windows/D3D9 sources are deliberately excluded.

TARGET      := kisakcod
BUILD       := build

ARCH        := -march=armv8-a -mtune=cortex-a57 -mtp=soft
CPPFLAGS    := -D__SWITCH__ -DKISAK_SWITCH -DKISAK_SP -DCINEMA -DUSE_SEPARATE_BLIT_TEXTURE \\
               -I$(CURDIR)/src -I$(CURDIR)/deps -I$(CURDIR)/deps/msslib
CXXFLAGS    := $(ARCH) -O2 -g -ffunction-sections -fdata-sections -fno-rtti -std=gnu++20 -MMD -MP
CFLAGS      := $(ARCH) -O2 -g -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS     := $(ARCH) -specs=$(DEVKITPRO)/libnx/switch.specs -Wl,--gc-sections
LIBS        := -lglad -lEGL -lglapi -ldrm_nouveau -lnx -lm

include $(DEVKITPRO)/libnx/switch_rules

# Singleplayer Switch build. Keep shared engine/game code, but exclude MP, Windows and D3D9.
CPP_SOURCES := $(shell find src -type f -name '*.cpp' \\
    ! -path 'src/gfx_d3d/*' \\
    ! -path 'src/win32/*' \\
    ! -path 'src/linux/*' \\
    ! -path 'src/platform/*' \\
    ! -name 'com_files.cpp' \\
    ! -path 'src/groupvoice/*' \\
    ! -name 'win_common.cpp' \\
    ! -name 'win_shared.cpp' \\
    ! -name 'snd_mss.cpp' \\
    ! -name 'snd_driver.cpp' \\
    ! -name 'threads.cpp' \\
    ! -name 'timing.cpp' \\
    ! -name 'profile.cpp')

C_SOURCES := $(shell find src -type f -name '*.c' \\
    ! -path 'src/gfx_d3d/*' \\
    ! -path 'src/win32/*' \\
    ! -path 'src/linux/*' \\
    ! -path 'src/platform/*' \\
    ! -path 'src/groupvoice/*')

# zlib is required by the engine's archive/zip loader.
C_SOURCES += $(shell find deps/zlib -type f -name '*.c')

CPP_SOURCES += src/platform/switch/switch_main.cpp src/platform/switch/switch_fs.cpp src/platform/switch/switch_threads.cpp src/platform/switch/switch_timing.cpp src/platform/switch/switch_profile.cpp src/platform/switch/switch_sys.cpp
CPP_SOURCES += src/gfx_d3d/r_init_switch.cpp src/gfx_d3d/r_buffers.cpp src/gfx_d3d/r_state.cpp
CPP_SOURCES += src/gfx_d3d/r_shade.cpp src/gfx_d3d/rb_shade.cpp src/gfx_d3d/r_material.cpp
CPP_SOURCES += src/gfx_d3d/r_image.cpp src/gfx_d3d/r_image_load_common.cpp src/gfx_d3d/r_image_load_obj.cpp src/gfx_d3d/r_image_utils.cpp src/gfx_d3d/r_image_wavelet.cpp src/gfx_d3d/r_imagedecode.cpp src/gfx_d3d/r_rendertarget.cpp

CPP_OBJECTS := $(CPP_SOURCES:%.cpp=$(BUILD)/%.o)
C_OBJECTS   := $(C_SOURCES:%.c=$(BUILD)/%.o)
OBJECTS     := $(CPP_OBJECTS) $(C_OBJECTS)

.PHONY: all clean print-sources

all: $(TARGET).nro

$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

-include $(OBJECTS:.o=.d)

print-sources:
	@printf '%s\\n' $(CPP_SOURCES) $(C_SOURCES)

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).nacp
