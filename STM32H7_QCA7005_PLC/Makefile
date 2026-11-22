# STM32H7_QCA7005_PLC - command-line build (same sources/flags as the CubeIDE project)
#
#   make ROLE=EVSE   -> build/evse/STM32H7_QCA7005_PLC.elf/.hex/.bin  (charger)
#   make ROLE=EV     -> build/ev/STM32H7_QCA7005_PLC.elf/.hex/.bin    (vehicle)
#   make all-roles   -> both
#   make flash ROLE=EV   (STM32_Programmer_CLI, ST-LINK SWD)
#   make test        -> host unit tests of the PLC library (gcc)
#   make clean
#
# Needs arm-none-eabi-gcc in PATH (STM32CubeIDE ships one under plugins/.../tools/bin).

TARGET   = STM32H7_QCA7005_PLC
ROLE    ?= EVSE
ROLE_LC  = $(shell echo $(ROLE) | tr A-Z a-z)
BUILD    = build/$(ROLE_LC)
PREFIX  ?= arm-none-eabi-
CC       = $(PREFIX)gcc
OBJCOPY  = $(PREFIX)objcopy
SIZE     = $(PREFIX)size

C_SRCS  := $(shell find Core/Src PLC Drivers -name '*.c' | sort)
ASM_SRCS = Core/Startup/startup_stm32h743zitx.s

C_DEFS   = -DUSE_HAL_DRIVER -DSTM32H743xx -DUSE_PWR_LDO_SUPPLY -DPLC_ROLE=PLC_ROLE_$(ROLE)
C_INCS   = -ICore/Inc \
           -IPLC \
           -IDrivers/STM32H7xx_HAL_Driver/Inc \
           -IDrivers/STM32H7xx_HAL_Driver/Inc/Legacy \
           -IDrivers/CMSIS/Device/ST/STM32H7xx/Include \
           -IDrivers/CMSIS/Include

MCU      = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
OPT     ?= -O2 -g3
CFLAGS   = $(MCU) $(C_DEFS) $(C_INCS) $(OPT) -std=gnu11 -Wall \
           -ffunction-sections -fdata-sections -fstack-usage -MMD -MP
ASFLAGS  = $(MCU) -x assembler-with-cpp $(OPT)
LDSCRIPT = STM32H743ZITX_FLASH.ld
LDFLAGS  = $(MCU) -T$(LDSCRIPT) --specs=nano.specs --specs=nosys.specs \
           -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--gc-sections -Wl,--print-memory-usage \
           -Wl,--start-group -lc -lm -Wl,--end-group

OBJS = $(addprefix $(BUILD)/,$(C_SRCS:.c=.o)) $(addprefix $(BUILD)/,$(ASM_SRCS:.s=.o))

ifneq ($(ROLE),EVSE)
ifneq ($(ROLE),EV)
$(error ROLE must be EVSE or EV)
endif
endif

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

all-roles:
	$(MAKE) ROLE=EVSE
	$(MAKE) ROLE=EV

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

test:
	$(MAKE) -C tests

clean:
	rm -rf build
	$(MAKE) -C tests clean

-include $(OBJS:.o=.d)

.PHONY: all all-roles flash test clean
