##############################################################################
# Makefile for STM32L552R 4-Channel PID Voltage Controller
#
# Toolchain: arm-none-eabi-gcc (STM32CubeIDE 내장 또는 별도 설치)
#
# 사용법:
#   make all     - 전체 빌드
#   make clean   - 빌드 결과물 삭제
#   make flash   - ST-Link로 플래시 (st-flash 필요)
#   make size    - 바이너리 크기 표시
#
# 참고: STM32CubeIDE로 빌드할 경우 이 Makefile 대신
#       IDE의 빌드 시스템을 사용합니다.
##############################################################################

######################################
# Target
######################################
TARGET = pid_voltage_controller

######################################
# Build directory
######################################
BUILD_DIR = build

######################################
# Source files
######################################

# C sources
C_SOURCES = \
Core/Src/main.c \
Core/Src/ad5641.c \
Core/Src/mcp3465r.c \
Core/Src/pid_controller.c \
Core/Src/voltage_control.c \
Core/Src/stm32l5xx_hal_msp.c \
Core/Src/stm32l5xx_it.c \
Core/Src/system_stm32l5xx.c

# ASM sources
ASM_SOURCES = \
startup_stm32l552retx.s

######################################
# Toolchain
######################################
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

######################################
# CFLAGS
######################################

# CPU
CPU = -mcpu=cortex-m33

# FPU (STM32L552 has FPU)
FPU = -mfpu=fpv5-sp-d16
FLOAT-ABI = -mfloat-abi=hard

# MCU flags
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# C defines
C_DEFS = \
-DSTM32L552xx \
-DUSE_HAL_DRIVER

# C includes
C_INCLUDES = \
-ICore/Inc \
-IDrivers/STM32L5xx_HAL_Driver/Inc \
-IDrivers/CMSIS/Device/ST/STM32L5xx/Include \
-IDrivers/CMSIS/Include \
-IMiddlewares/Third_Party/FreeRTOS/Source/include \
-IMiddlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ

# Compiler flags
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) -Wall -Wextra -fdata-sections \
         -ffunction-sections -g -gdwarf-2 -Os

# Generate dependency files
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

######################################
# LDFLAGS
######################################

# Linker script
LDSCRIPT = STM32L552RETx_FLASH.ld

# Libraries
LIBS = -lc -lm -lnosys
LIBDIR =

# Linker flags
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

######################################
# HAL & FreeRTOS Source Files
# (STM32Cube 펌웨어 패키지에서 복사하거나 경로 수정 필요)
######################################

# HAL Driver sources (필요한 모듈만)
C_SOURCES += \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_cortex.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_gpio.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_rcc.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_rcc_ex.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_pwr.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_pwr_ex.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_spi.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_spi_ex.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_uart.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_uart_ex.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_flash.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_flash_ex.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_dma.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_dma_ex.c \
Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_exti.c

# FreeRTOS sources
C_SOURCES += \
Middlewares/Third_Party/FreeRTOS/Source/croutine.c \
Middlewares/Third_Party/FreeRTOS/Source/event_groups.c \
Middlewares/Third_Party/FreeRTOS/Source/list.c \
Middlewares/Third_Party/FreeRTOS/Source/queue.c \
Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c \
Middlewares/Third_Party/FreeRTOS/Source/tasks.c \
Middlewares/Third_Party/FreeRTOS/Source/timers.c \
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/port.c \
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c \
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c

######################################
# Build Rules
######################################

# Default target
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex \
     $(BUILD_DIR)/$(TARGET).bin

# List of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

# Compile C files
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

# Compile ASM files
$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

# Link
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

# Generate HEX
$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

# Generate BIN
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

# Create build directory
$(BUILD_DIR):
	mkdir -p $@

######################################
# Utility targets
######################################

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $< 0x08000000

size: $(BUILD_DIR)/$(TARGET).elf
	$(SZ) --format=berkeley $<

# Display memory usage
info: $(BUILD_DIR)/$(TARGET).elf
	$(SZ) -A -d $<

######################################
# Dependencies
######################################
-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean flash size info
