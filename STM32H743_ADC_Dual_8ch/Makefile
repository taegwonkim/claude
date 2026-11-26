# STM32H743 듀얼 인터리브 ADC 8채널 - GNU Arm Embedded(arm-none-eabi-gcc) 빌드
#
#   ./tools/fetch_drivers.sh   # Drivers/ 아래에 ST HAL, CMSIS를 받습니다(최초 1회)
#   make                       # build/adc_dual_8ch.elf / .hex / .bin 생성
#
# STM32CubeH7 펌웨어 패키지를 이미 갖고 있다면 경로만 지정해도 됩니다.
#   make HAL_DIR=.../Drivers/STM32H7xx_HAL_Driver \
#        CMSIS_DEV_DIR=.../Drivers/CMSIS/Device/ST/STM32H7xx \
#        CMSIS_CORE_DIR=.../Drivers/CMSIS/Include

TARGET   := adc_dual_8ch
BUILD    := build

HAL_DIR        ?= Drivers/STM32H7xx_HAL_Driver
CMSIS_DEV_DIR  ?= Drivers/CMSIS/Device/ST/STM32H7xx
CMSIS_CORE_DIR ?= Drivers/CMSIS/Core/Include

PREFIX ?= arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

HAL_SRCS := \
  stm32h7xx_hal.c stm32h7xx_hal_cortex.c stm32h7xx_hal_rcc.c stm32h7xx_hal_rcc_ex.c \
  stm32h7xx_hal_pwr.c stm32h7xx_hal_pwr_ex.c stm32h7xx_hal_gpio.c stm32h7xx_hal_flash.c \
  stm32h7xx_hal_flash_ex.c stm32h7xx_hal_dma.c stm32h7xx_hal_dma_ex.c stm32h7xx_hal_mdma.c \
  stm32h7xx_hal_exti.c stm32h7xx_hal_hsem.c \
  stm32h7xx_hal_adc.c stm32h7xx_hal_adc_ex.c stm32h7xx_hal_tim.c stm32h7xx_hal_tim_ex.c \
  stm32h7xx_hal_uart.c stm32h7xx_hal_uart_ex.c

C_SRCS := \
  Core/Src/main.c Core/Src/adc_dual.c Core/Src/stm32h7xx_it.c Core/Src/syscalls_min.c \
  $(CMSIS_DEV_DIR)/Source/Templates/system_stm32h7xx.c \
  $(addprefix $(HAL_DIR)/Src/,$(HAL_SRCS))

ASM_SRCS := $(CMSIS_DEV_DIR)/Source/Templates/gcc/startup_stm32h743xx.s
LDSCRIPT := Linker/STM32H743ZITX_FLASH.ld

MCU := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard

DEFS := -DSTM32H743xx -DUSE_HAL_DRIVER
INCS := -ICore/Inc -I$(HAL_DIR)/Inc -I$(CMSIS_DEV_DIR)/Include -I$(CMSIS_CORE_DIR)

CFLAGS  := $(MCU) $(DEFS) $(INCS) -std=gnu11 -O2 -g3 -Wall -Wextra \
           -ffunction-sections -fdata-sections
ASFLAGS := $(MCU) -x assembler-with-cpp
LDFLAGS := $(MCU) -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map \
           --specs=nano.specs -Wl,--print-memory-usage -Wl,--no-warn-rwx-segments

OBJS := $(addprefix $(BUILD)/,$(notdir $(C_SRCS:.c=.o)) $(notdir $(ASM_SRCS:.s=.o)))
vpath %.c $(sort $(dir $(C_SRCS)))
vpath %.s $(sort $(dir $(ASM_SRCS)))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(CC) -c $(ASFLAGS) $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
