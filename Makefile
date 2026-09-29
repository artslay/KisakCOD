# Nintendo Switch build for KisakCOD.
# Requires devkitPro/libnx and Mesa Switch OpenGL/EGL libraries.
#
# The source set is generated from the engine tree instead of duplicating the
# old CMake source lists. Windows/D3D9 sources are deliberately excluded.

TARGET      := kisakcod
BUILD       := build

ARCH        := -march=armv8-a -mtune=cortex-a57 -mtp=soft
MESA_SDK    := $(CURDIR)/mesa-sdk/opt/devkitpro/portlibs/switch
CPPFLAGS    := -D__SWITCH__ -DKISAK_SWITCH -DKISAK_SP -DCINEMA -DUSE_SEPARATE_BLIT_TEXTURE \
               -I$(CURDIR)/src -I$(CURDIR)/src/gfx -I$(CURDIR)/deps \
               -I$(DEVKITPRO)/libnx/include -I$(MESA_SDK)/include
CXXFLAGS    := $(ARCH) -O2 -g -ffunction-sections -fdata-sections -fno-rtti -std=gnu++20 -MMD -MP
CFLAGS      := $(ARCH) -O2 -g -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS     := $(ARCH) -L$(MESA_SDK)/lib -L$(DEVKITPRO)/libnx/lib -specs=$(DEVKITPRO)/libnx/switch.specs -Wl,--gc-sections
LIBS        := -lGL -lEGL -lglapi -lnx -lm

include $(DEVKITPRO)/libnx/switch_rules

# Singleplayer Switch build. Keep shared engine/game code, but exclude MP, Windows and D3D9.
CPP_SOURCES := $(shell find src -type f -name '*.cpp' \
    ! -path 'src/gfx_d3d/*' \
    ! -path 'src/win32/*' \
    ! -path 'src/linux/*' \
    ! -path 'src/platform/*' \
    ! -name 'com_files.cpp' \
    ! -path 'src/groupvoice/*' \
    ! -path 'src/radiant/*' \
    ! -path 'src/*_mp/*' \
    ! -name '*_mp.cpp' \
    ! -name 'win_common.cpp' \
    ! -name 'win_shared.cpp' \
    ! -name 'snd_mss.cpp' \
    ! -name 'snd_driver.cpp' \
    ! -name 'threads.cpp' \
    ! -name 'timing.cpp' \
    ! -name 'profile.cpp' \
    ! -path 'src/physics/ode/array.cpp' \
    ! -path 'src/physics/ode/collision_trimesh*.cpp' \
    ! -name 'stack.cpp' \
    ! -name 'obstack.cpp' \
    ! -name 'testing.cpp' \
    ! -name 'scr_yacc.cpp' )

C_SOURCES := $(shell find src -type f -name '*.c' \
    ! -path 'src/gfx_d3d/*' \
    ! -path 'src/win32/*' \
    ! -path 'src/linux/*' \
    ! -path 'src/platform/*' \
    ! -path 'src/groupvoice/*' \
    ! -path 'src/radiant/*' \
    ! -path 'src/*_mp/*' \
    ! -name '*_mp.c')

# zlib is required by the engine's archive/zip loader.
C_SOURCES += $(shell find deps/zlib -type f -name '*.c')

CPP_SOURCES += src/platform/switch/switch_main.cpp src/platform/switch/switch_fs.cpp src/platform/switch/switch_threads.cpp src/platform/switch/switch_timing.cpp src/platform/switch/switch_profile.cpp src/platform/switch/switch_sys.cpp
CPP_SOURCES += src/gfx_d3d/r_init_switch.cpp src/gfx_d3d/r_buffers.cpp src/gfx_d3d/r_state.cpp
CPP_SOURCES += src/gfx_d3d/r_shade.cpp src/gfx_d3d/rb_shade.cpp src/gfx_d3d/r_material.cpp
CPP_SOURCES += src/gfx_d3d/r_material_override.cpp src/gfx_d3d/rb_uploadshaders.cpp src/gfx_d3d/r_dvars.cpp
CPP_SOURCES += src/gfx_d3d/r_image.cpp src/gfx_d3d/r_image_load_common.cpp src/gfx_d3d/r_image_load_obj.cpp src/gfx_d3d/r_image_utils.cpp src/gfx_d3d/r_image_wavelet.cpp src/gfx_d3d/r_imagedecode.cpp src/gfx_d3d/r_rendertarget.cpp
CPP_SOURCES += src/gfx_d3d/r_rendercmds.cpp
CPP_SOURCES += src/gfx_d3d/r_model.cpp src/gfx_d3d/r_scene.cpp src/gfx_d3d/r_dpvs.cpp
CPP_SOURCES += src/gfx_d3d/r_draw_bsp.cpp src/gfx_d3d/r_draw_lit.cpp src/gfx_d3d/r_draw_staticmodel.cpp src/gfx_d3d/r_draw_xmodel.cpp src/gfx_d3d/r_model_skin.cpp
CPP_SOURCES += src/gfx_d3d/r_bsp.cpp src/gfx_d3d/r_bsp_load_obj.cpp src/gfx_d3d/r_staticmodelcache.cpp src/gfx_d3d/r_dobj_skin.cpp src/gfx_d3d/r_model_lighting.cpp src/gfx_d3d/r_reflection_probe.cpp
CPP_SOURCES += src/gfx_d3d/r_model_pose.cpp src/gfx_d3d/r_state_utils.cpp src/gfx_d3d/r_drawsurf.cpp src/gfx_d3d/r_add_bsp.cpp src/gfx_d3d/r_add_staticmodel.cpp
CPP_SOURCES += src/gfx_d3d/r_dpvs_dynmodel.cpp src/gfx_d3d/r_dpvs_entity.cpp src/gfx_d3d/r_dpvs_sceneent.cpp src/gfx_d3d/r_dpvs_static.cpp
CPP_SOURCES += src/gfx_d3d/r_meshdata.cpp src/gfx_d3d/r_draw_method.cpp src/gfx_d3d/r_pretess.cpp
CPP_SOURCES += src/gfx_d3d/r_add_cmdbuf.cpp src/gfx_d3d/r_staticmodel.cpp src/gfx_d3d/r_xsurface.cpp src/gfx_d3d/r_reflection_probe_load_obj.cpp
CPP_SOURCES += src/gfx_d3d/r_light.cpp src/gfx_d3d/r_light_load_obj.cpp src/gfx_d3d/r_primarylights.cpp src/gfx_d3d/r_outdoor.cpp

CPP_OBJECTS := $(CPP_SOURCES:%.cpp=$(BUILD)/%.o)
C_OBJECTS   := $(C_SOURCES:%.c=$(BUILD)/%.o)
OBJECTS     := $(CPP_OBJECTS) $(C_OBJECTS)

.PHONY: all clean print-sources

all: $(TARGET).nro

$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@printf '  CXX %s\\n' "$(notdir $<)"
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf '  CC  %s\\n' "$(notdir $<)"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

-include $(OBJECTS:.o=.d)

print-sources:
	@printf '%s\n' $(CPP_SOURCES) $(C_SOURCES)

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).nacp
