##############################################################################
# MCP3465R_CAL — STM32L552RCT6
# Toolchain : arm-none-eabi-gcc (GNU Arm Embedded Toolchain 12+)
# HAL       : STM32Cube FW_L5 V1.5.0  (set CUBE_FW_PATH below)
##############################################################################

TARGET   = MCP3465R_CAL
BUILD    = build
DEBUG    = 1

# ── Path to STM32CubeL5 firmware package ─────────────────────────────────
# Adjust to match your installation, e.g.:
#   Windows : C:/Users/<you>/STM32Cube/Repository/STM32Cube_FW_L5_V1.5.0
#   Linux   : $(HOME)/STM32Cube/Repository/STM32Cube_FW_L5_V1.5.0
CUBE_FW_PATH ?= $(HOME)/STM32Cube/Repository/STM32Cube_FW_L5_V1.5.0

HAL_PATH  = $(CUBE_FW_PATH)/Drivers/STM32L5xx_HAL_Driver
CMSIS_PATH= $(CUBE_FW_PATH)/Drivers/CMSIS

# ── Sources ───────────────────────────────────────────────────────────────
C_SOURCES  = \
  Core/Src/main.c                         \
  Core/Src/stm32l5xx_hal_msp.c            \
  BSP/Src/stm32l552_flash.c               \
  BSP/Src/mcp3465r_cal.c                  \
  BSP/Src/mcp3465r_bsp.c                  \
  $(HAL_PATH)/Src/stm32l5xx_hal.c         \
  $(HAL_PATH)/Src/stm32l5xx_hal_rcc.c     \
  $(HAL_PATH)/Src/stm32l5xx_hal_rcc_ex.c  \
  $(HAL_PATH)/Src/stm32l5xx_hal_gpio.c    \
  $(HAL_PATH)/Src/stm32l5xx_hal_dma.c     \
  $(HAL_PATH)/Src/stm32l5xx_hal_dma_ex.c  \
  $(HAL_PATH)/Src/stm32l5xx_hal_cortex.c  \
  $(HAL_PATH)/Src/stm32l5xx_hal_pwr.c     \
  $(HAL_PATH)/Src/stm32l5xx_hal_pwr_ex.c  \
  $(HAL_PATH)/Src/stm32l5xx_hal_spi.c     \
  $(HAL_PATH)/Src/stm32l5xx_hal_uart.c    \
  $(HAL_PATH)/Src/stm32l5xx_hal_uart_ex.c \
  $(HAL_PATH)/Src/stm32l5xx_hal_exti.c

ASM_SOURCES = startup/startup_stm32l552rctx.s

# ── Include paths ─────────────────────────────────────────────────────────
C_INCLUDES = \
  -ICore/Inc                                                 \
  -IBSP/Inc                                                  \
  -I$(HAL_PATH)/Inc                                          \
  -I$(HAL_PATH)/Inc/Legacy                                   \
  -I$(CMSIS_PATH)/Device/ST/STM32L5xx/Include                \
  -I$(CMSIS_PATH)/Include

# ── Defines ───────────────────────────────────────────────────────────────
C_DEFS = \
  -DSTM32L552xx       \
  -DUSE_HAL_DRIVER

# ── Toolchain ─────────────────────────────────────────────────────────────
PREFIX   = arm-none-eabi-
CC       = $(PREFIX)gcc
AS       = $(PREFIX)gcc -x assembler-with-cpp
CP       = $(PREFIX)objcopy
SZ       = $(PREFIX)size
HEX      = $(CP) -O ihex
BIN      = $(CP) -O binary -S

# ── Compiler flags ────────────────────────────────────────────────────────
MCU      = -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

ifeq ($(DEBUG), 1)
OPT = -Og -g -gdwarf-2
else
OPT = -O2
endif

CFLAGS   = $(MCU) $(OPT) $(C_DEFS) $(C_INCLUDES) \
           -Wall -Wextra -fdata-sections -ffunction-sections \
           --specs=nano.specs -MMD -MP

ASFLAGS  = $(MCU) $(OPT) -Wall -fdata-sections -ffunction-sections

LDFLAGS  = $(MCU) -specs=nano.specs -specs=nosys.specs \
           -T STM32L552RCTX_FLASH.ld \
           -Wl,-Map=$(BUILD)/$(TARGET).map,--cref \
           -Wl,--gc-sections \
           -lc -lm

# ── Build rules ───────────────────────────────────────────────────────────
OBJECTS    = $(addprefix $(BUILD)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS   += $(addprefix $(BUILD)/,$(notdir $(ASM_SOURCES:.s=.o)))
VPATH      = $(sort $(dir $(C_SOURCES))) $(sort $(dir $(ASM_SOURCES)))
DEPS       = $(OBJECTS:.o=.d)

.PHONY: all clean flash

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin
	@echo ""
	$(SZ) $<

$(BUILD)/$(TARGET).elf: $(OBJECTS) | $(BUILD)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c $(CFLAGS) -MF $(BUILD)/$*.d $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(HEX) $< $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(BIN) $< $@

$(BUILD):
	mkdir -p $@

# Flash via OpenOCD (ST-Link)
flash: $(BUILD)/$(TARGET).elf
	openocd -f interface/stlink.cfg \
	        -f target/stm32l5x.cfg  \
	        -c "program $< verify reset exit"

clean:
	rm -rf $(BUILD)

-include $(DEPS)
