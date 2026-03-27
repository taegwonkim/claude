cmake_minimum_required(VERSION 3.22)

# 툴체인 파일은 cmake 호출 시 지정:
#   cmake -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake ..

project(stm32l5_freertos_led C ASM)

# ---------------------------------------------------------------
# 타겟 MCU 정의
# ---------------------------------------------------------------
set(MCU_FLAGS
    -mcpu=cortex-m33
    -mthumb
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
)

# ---------------------------------------------------------------
# 소스 파일
# ---------------------------------------------------------------
set(SOURCES
    # 애플리케이션
    Core/Src/main.c
    Core/Src/tim6_tick.c
    Core/Src/stm32l5xx_it.c

    # STM32L5 HAL (필요한 파일만 포함)
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_rcc.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_rcc_ex.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_gpio.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_tim.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_tim_ex.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_pwr.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_pwr_ex.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_cortex.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_flash.c
    Drivers/STM32L5xx_HAL_Driver/Src/stm32l5xx_hal_flash_ex.c

    # CMSIS 시스템 초기화
    Drivers/CMSIS/Device/ST/STM32L5xx/Source/Templates/system_stm32l5xx.c
    Drivers/CMSIS/Device/ST/STM32L5xx/Source/Templates/gcc/startup_stm32l552xx.s

    # FreeRTOS 커널
    Drivers/FreeRTOS/Source/tasks.c
    Drivers/FreeRTOS/Source/queue.c
    Drivers/FreeRTOS/Source/list.c
    Drivers/FreeRTOS/Source/timers.c
    Drivers/FreeRTOS/Source/portable/MemMang/heap_4.c
    Drivers/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
    Drivers/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
)

# ---------------------------------------------------------------
# 인클루드 경로
# ---------------------------------------------------------------
set(INCLUDES
    Core/Inc
    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/ST/STM32L5xx/Include
    Drivers/STM32L5xx_HAL_Driver/Inc
    Drivers/FreeRTOS/Source/include
    Drivers/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure
)

# ---------------------------------------------------------------
# 컴파일 정의
# ---------------------------------------------------------------
set(DEFINES
    STM32L552xx
    USE_HAL_DRIVER
)

# ---------------------------------------------------------------
# 실행 파일 생성
# ---------------------------------------------------------------
add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_include_directories(${PROJECT_NAME}.elf PRIVATE ${INCLUDES})
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE ${DEFINES})

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -Wall
    -Wextra
    -fdata-sections
    -ffunction-sections
    -g3
    -O2
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${CMAKE_SOURCE_DIR}/STM32L552ZETXQ_FLASH.ld
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    -lc
    -lm
    -lnosys
)

# ---------------------------------------------------------------
# 빌드 후 hex / bin 생성
# ---------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMENT "Generating HEX and BIN files"
)
