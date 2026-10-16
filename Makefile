# STM32H7_LAN8770_Ethernet - command-line build (same sources/flags as the CubeIDE project)
#
#   make            -> build/STM32H7_LAN8770_Ethernet.elf/.hex/.bin
#   make flash      -> STM32_Programmer_CLI (ST-LINK, SWD)
#   make clean
#
# Needs arm-none-eabi-gcc in PATH (STM32CubeIDE ships one under plugins/.../tools/bin).

TARGET   = STM32H7_LAN8770_Ethernet
BUILD    = build
PREFIX  ?= arm-none-eabi-
CC       = $(PREFIX)gcc
OBJCOPY  = $(PREFIX)objcopy
SIZE     = $(PREFIX)size

# Every .c in the CubeIDE source folders (Core, Drivers, LWIP, Middlewares)
C_SRCS  := $(shell find Core/Src Drivers LWIP Middlewares -name '*.c' | sort)
ASM_SRCS = Core/Startup/startup_stm32h743zitx.s

C_DEFS   = -DUSE_HAL_DRIVER -DSTM32H743xx -DUSE_PWR_LDO_SUPPLY
C_INCS   = -ICore/Inc \
           -ILWIP/App \
           -ILWIP/Target \
           -IDrivers/BSP/Components/lan8770 \
           -IDrivers/STM32H7xx_HAL_Driver/Inc \
           -IDrivers/STM32H7xx_HAL_Driver/Inc/Legacy \
           -IDrivers/CMSIS/Device/ST/STM32H7xx/Include \
           -IDrivers/CMSIS/Include \
           -IMiddlewares/Third_Party/LwIP/src/include \
           -IMiddlewares/Third_Party/LwIP/system \
           -IMiddlewares/Third_Party/LwIP/system/arch

MCU      = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
OPT     ?= -Og -g3
CFLAGS   = $(MCU) $(C_DEFS) $(C_INCS) $(OPT) -std=gnu11 -Wall \
           -ffunction-sections -fdata-sections -fstack-usage -MMD -MP
ASFLAGS  = $(MCU) -x assembler-with-cpp $(OPT)
LDSCRIPT = STM32H743ZITX_FLASH.ld
LDFLAGS  = $(MCU) -T$(LDSCRIPT) --specs=nano.specs --specs=nosys.specs \
           -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--gc-sections -Wl,--print-memory-usage \
           -Wl,--start-group -lc -lm -Wl,--end-group

OBJS = $(addprefix $(BUILD)/,$(C_SRCS:.c=.o)) $(addprefix $(BUILD)/,$(ASM_SRCS:.s=.o))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) -c $(ASFLAGS) $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary -S $< $@

flash: $(BUILD)/$(TARGET).elf
	STM32_Programmer_CLI -c port=SWD -w $< -v -rst

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all flash clean
