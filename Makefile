# Main:
TARGET         := rp2350_main

# Examples:
# TARGET         := test_exti
# TARGET         := test_output

BUILD_DIR      := build
PRESET		   := pico2-debug
BIN_IMAGE	   := $(BUILD_DIR)/$(PRESET)/mcus/rp2350/$(TARGET).elf 


# OpenOCD stuff:
PICO_VERSION   := rp2350#rp2040
OPENOCD_DIR    := ~/.pico-sdk/openocd/0.12.0+dev
OPENOCD        := $(OPENOCD_DIR)/openocd
SCRIPTS		   := $(OPENOCD_DIR)/scripts
INTERFACE      := interface/cmsis-dap.cfg
TARGET_CFG     := target/$(PICO_VERSION).cfg
ADAPTER_SPEED  := 5000
OPENOCD_FLAGS  := -s $(SCRIPTS) -f $(INTERFACE) -f $(TARGET_CFG)  -c "adapter speed $(ADAPTER_SPEED)"



.PHONY: all flash clean reset build 
all: build flash 
 
build:
	@cmake --build build/$(PRESET)

$(BIN_IMAGE):
	build  

# The manual flash command using OpenOCD and Linux binaries
flash: $(BIN_IMAGE)
	echo "Checking for ELF binary at: $(BIN_IMAGE)"
	if [ ! -f $(BIN_IMAGE) ]; then echo "Error: Binary not found! Run 'make build' first."; exit 1; fi
	echo "Flashing $(BIN_IMAGE) to Raspberry Pi Pico..."
	$(OPENOCD) $(OPENOCD_FLAGS) -c "program $(BIN_IMAGE) verify reset exit"


# The manual flash command using OpenOCD and Linux binaries
debug:
	@echo "Checking for ELF binary at: $(BIN_IMAGE)"
	@if [ ! -f $(BIN_IMAGE) ]; then echo "Error: Binary not found! Run 'make build' first."; exit 1; fi
	@echo "Flashing $(BIN_IMAGE) to Raspberry Pi Pico..."
	$(OPENOCD) $(OPENOCD_FLAGS) -c "init" -c "rp2350.cm0 configure -rtos FreeRTOS" -c "program $(BIN_IMAGE) verify reset"


reset:
	$(OPENOCD) $(OPENOCD_FLAGS) -c "init; reset; exit"

clean:
	rm -rf $(BUILD_DIR)