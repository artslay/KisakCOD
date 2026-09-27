# Nintendo Switch build for KisakCOD.
# Requires devkitPro/libnx and Mesa Switch OpenGL/EGL libraries.

TARGET      := kisakcod
BUILD       := build
SOURCES     := src/platform/switch/switch_main.cpp \
               src/gfx/gfx_backend.cpp \
               src/gfx/opengl/gl_backend.cpp
OBJECTS     := $(SOURCES:%.cpp=$(BUILD)/%.o)

ARCH        := -march=armv8-a -mtune=cortex-a57 -mtp=soft
CXXFLAGS    := $(ARCH) -O2 -g -ffunction-sections -fdata-sections -fno-rtti -std=gnu++20 \
               -D__SWITCH__ -DKISAK_SWITCH -DKISAK_MP -DCINEMA -DUSE_SEPARATE_BLIT_TEXTURE \
               -I$(CURDIR)/src
LDFLAGS     := $(ARCH) -specs=$(DEVKITPRO)/libnx/switch.specs -Wl,--gc-sections
LIBS        := -lglad -lEGL -lglapi -ldrm_nouveau -lnx

include $(DEVKITPRO)/libnx/switch_rules

.PHONY: all clean

all: $(TARGET).nro

$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).nacp
