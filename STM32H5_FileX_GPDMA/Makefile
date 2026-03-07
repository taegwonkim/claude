##############################################################################
# STM32H5 FileX + GPDMA1/2 프로젝트 Makefile
# 타겟: STM32H563xx / STM32H573xx
##############################################################################

TARGET    = STM32H5_FileX_GPDMA
BUILD_DIR = build

# ─── 빌드 도구 ─────────────────────────────────────────────────────────────
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size
HEX     = $(CP) -O ihex
BIN     = $(CP) -O binary -S

# ─── MCU 설정 ───────────────────────────────────────────────────────────────
CPU    = -mcpu=cortex-m33
FPU    = -mfpu=fpv5-sp-d16
FLOAT  = -mfloat-abi=hard
MCU    = $(CPU) -mthumb $(FPU) $(FLOAT)

# ─── 소스 파일 ──────────────────────────────────────────────────────────────
C_SOURCES = \
    Core/Src/main.c                         \
    Core/Src/stm32h5xx_hal_msp.c            \
    Core/Src/stm32h5xx_it.c                 \
    FileX/App/app_filex.c                   \
    FileX/Target/fx_stm32_sd_driver.c       \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal.c            \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_cortex.c     \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma.c        \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma_ex.c     \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gpio.c       \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr.c        \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr_ex.c     \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc.c        \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc_ex.c     \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_sd.c         \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart.c       \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart_ex.c    \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_icache.c     \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c      \
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c   \
    Drivers/CMSIS/Device/ST/STM32H5xx/Source/system_stm32h5xx.c

# 시작 코드 (어셈블리)
ASM_SOURCES = \
    startup_stm32h563xx.s

# FileX 라이브러리 (사전 빌드된 .a 또는 소스 포함)
# STM32CubeH5 패키지에서 가져온 경로 예시:
FX_SOURCES_DIR = Middlewares/ST/FileX/common/src
FX_SOURCES = $(wildcard $(FX_SOURCES_DIR)/*.c)
C_SOURCES += $(FX_SOURCES)

# ─── 인클루드 경로 ──────────────────────────────────────────────────────────
C_INCLUDES = \
    -ICore/Inc                                                          \
    -IFileX/App                                                         \
    -IFileX/Target                                                      \
    -IDrivers/STM32H5xx_HAL_Driver/Inc                                  \
    -IDrivers/STM32H5xx_HAL_Driver/Inc/Legacy                           \
    -IDrivers/CMSIS/Device/ST/STM32H5xx/Include                         \
    -IDrivers/CMSIS/Include                                             \
    -IMiddlewares/ST/FileX/common/inc

# ─── 컴파일러 정의 ──────────────────────────────────────────────────────────
C_DEFS = \
    -DUSE_HAL_DRIVER       \
    -DSTM32H563xx          \
    -DFX_STANDALONE_ENABLE \
    -DFX_INCLUDE_USER_DEFINE_FILE

# ─── 컴파일 플래그 ──────────────────────────────────────────────────────────
CFLAGS  = $(MCU) $(C_DEFS) $(C_INCLUDES)
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -O2 -g3
CFLAGS += -std=c11

ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) -Wall -fdata-sections -ffunction-sections

# ─── 링커 설정 ──────────────────────────────────────────────────────────────
LDSCRIPT = STM32H563xx_FLASH.ld

LIBS    = -lc -lm -lnosys
LIBDIR  =
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
          -Wl,--gc-sections

# ─── 빌드 규칙 ──────────────────────────────────────────────────────────────
OBJECTS     = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c   $(sort $(dir $(C_SOURCES)))
OBJECTS    += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s   $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	-rm -fR $(BUILD_DIR)

# ─── 플래시 (ST-Link) ───────────────────────────────────────────────────────
flash: $(BUILD_DIR)/$(TARGET).elf
	STM32_Programmer_CLI -c port=SWD -w $< -v -rst

.PHONY: all clean flash
