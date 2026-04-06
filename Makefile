TARGET = 3dpinball-n64
BUILD_DIR = build
FS_DIR = filesystem
ASSETS_DIR = assets

include $(N64_INST)/include/n64.mk

CPP_SRCS := $(wildcard src/*.cpp)
C_SRCS := $(wildcard src/*.c)
OBJS := $(CPP_SRCS:%.cpp=$(BUILD_DIR)/%.o) $(C_SRCS:%.c=$(BUILD_DIR)/%.o)

N64_CFLAGS += -Isrc -DBIG_ENDIAN=1
N64_CXXFLAGS += -Isrc -DBIG_ENDIAN=1

PINBALL_XM := $(firstword $(wildcard $(ASSETS_DIR)/PINBALL.xm $(ASSETS_DIR)/PINBALL.XM))
ifeq ($(wildcard $(ASSETS_DIR)/PINBALL.DAT),)
$(error Missing required asset: $(ASSETS_DIR)/PINBALL.DAT)
endif
ifeq ($(PINBALL_XM),)
$(error Missing required asset: $(ASSETS_DIR)/PINBALL.xm (or PINBALL.XM))
endif

assets_xm_lower := $(wildcard $(ASSETS_DIR)/*.xm)
assets_xm_upper := $(wildcard $(ASSETS_DIR)/*.XM)
assets_wav_lower := $(wildcard $(ASSETS_DIR)/*.wav)
assets_wav_upper := $(wildcard $(ASSETS_DIR)/*.WAV)
assets_audio := $(assets_xm_lower) $(assets_xm_upper) $(assets_wav_lower) $(assets_wav_upper)
assets_other := $(filter-out $(assets_audio),$(wildcard $(ASSETS_DIR)/*))

assets_conv := $(addprefix $(FS_DIR)/,$(notdir $(assets_xm_lower:%.xm=%.xm64))) \
               $(addprefix $(FS_DIR)/,$(notdir $(assets_xm_upper:%.XM=%.xm64))) \
               $(addprefix $(FS_DIR)/,$(notdir $(assets_wav_lower:%.wav=%.wav64))) \
               $(addprefix $(FS_DIR)/,$(notdir $(assets_wav_upper:%.WAV=%.wav64)))
assets_copy := $(addprefix $(FS_DIR)/,$(notdir $(assets_other)))

all: $(TARGET).z64

$(FS_DIR)/%.xm64: $(ASSETS_DIR)/%.xm
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) --xm-compress 0 -o $(FS_DIR) "$<"

$(FS_DIR)/%.xm64: $(ASSETS_DIR)/%.XM
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) --xm-compress 0 -o $(FS_DIR) "$<"

$(FS_DIR)/%.wav64: $(ASSETS_DIR)/%.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) --wav-compress 0 -o $(FS_DIR) "$<"

$(FS_DIR)/%.wav64: $(ASSETS_DIR)/%.WAV
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) --wav-compress 0 -o $(FS_DIR) "$<"

$(FS_DIR)/%: $(ASSETS_DIR)/%
	@mkdir -p $(dir $@)
	@echo "    [COPY ] $@"
	@cp "$<" "$@"

$(BUILD_DIR)/$(TARGET).dfs: $(assets_conv) $(assets_copy)
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)

$(TARGET).z64: N64_ROM_TITLE="3D Pinball Space Cadet"
$(TARGET).z64: N64_ROM_SAVETYPE=eeprom4k
$(TARGET).z64: $(BUILD_DIR)/$(TARGET).dfs

clean:
	rm -rf $(BUILD_DIR) $(FS_DIR) $(TARGET).z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
