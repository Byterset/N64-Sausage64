ROM=sausage.z64
BUILD_DIR=build
FILESYSTEM_DIR=filesystem
include $(N64_INST)/include/n64.mk

src = main.c sausage64.c
assets_raw = assets/catherineMdl.bin
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix $(FILESYSTEM_DIR)/,$(notdir $(assets_png:%.png=%.sprite))) $(addprefix $(FILESYSTEM_DIR)/,$(notdir $(assets_raw)))

MKSPRITE_FLAGS ?=

all: $(ROM)

$(FILESYSTEM_DIR)/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -v -m BOX -o "$(dir $@)" "$<"

$(FILESYSTEM_DIR)/%.bin: assets/%.bin
	@mkdir -p $(dir $@)
	@echo "    [ASSET] $@"
	cp $< $@

$(BUILD_DIR)/$(ROM:.z64=.dfs): $(assets_conv)
$(BUILD_DIR)/$(ROM:.z64=.elf): $(src:%.c=$(BUILD_DIR)/%.o)

$(ROM): N64_ROM_TITLE="SAUSAGE64"
$(ROM): $(BUILD_DIR)/sausage.dfs

clean:
	rm -rf $(BUILD_DIR) $(ROM) $(FILESYSTEM_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
