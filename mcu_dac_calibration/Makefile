##############################################################################
# Makefile – STM32L552 ADC/DAC Auto-Calibration project
# Toolchain: arm-none-eabi-gcc
##############################################################################

TARGET   = mcu_dac_calibration
MCU      = cortex-m33
FPU      = fpv5-sp-d16
FLOAT    = hard

CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy
SIZE     = arm-none-eabi-size

BUILD_DIR = build

SRCS = \
    Core/Src/main.c          \
    Core/Src/calibration.c   \
    Core/Src/uart_log.c      \
    Drivers/SPI/spi_hal.c    \
    Drivers/MCP3465R/mcp3465r.c \
    Drivers/DAC80501/dac80501.c \
    startup_stm32l552.s

INCS = \
    -ICore/Inc \
    -IDrivers/SPI \
    -IDrivers/MCP3465R \
    -IDrivers/DAC80501

CFLAGS = \
    -mcpu=$(MCU) \
    -mthumb \
    -mfpu=$(FPU) \
    -mfloat-abi=$(FLOAT) \
    -DSTM32L552xx \
    -O2 \
    -Wall \
    -Wextra \
    -fdata-sections \
    -ffunction-sections \
    $(INCS)

LDFLAGS = \
    -mcpu=$(MCU) \
    -mthumb \
    -mfpu=$(FPU) \
    -mfloat-abi=$(FLOAT) \
    -TSTM32L552ZETxQ_FLASH.ld \
    -Wl,--gc-sections \
    -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
    -specs=nano.specs \
    -lc -lm -lnosys

OBJS = $(addprefix $(BUILD_DIR)/, $(addsuffix .o, $(basename $(notdir $(SRCS)))))

VPATH = Core/Src Drivers/SPI Drivers/MCP3465R Drivers/DAC80501 .

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).hex
	STM32_Programmer_CLI -c port=SWD -w $(BUILD_DIR)/$(TARGET).hex -rst

.PHONY: all clean flash
