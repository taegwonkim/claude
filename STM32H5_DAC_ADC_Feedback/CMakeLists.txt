cmake_minimum_required(VERSION 3.22)

# ============================================================
# 툴체인 설정 (arm-none-eabi-gcc 필요)
# ============================================================
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/arm-none-eabi.cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(STM32H5_DAC_ADC_Feedback C ASM)

# ============================================================
# MCU 타겟 플래그
# ============================================================
set(MCU_FLAGS
    -mcpu=cortex-m33
    -mthumb
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
)

# ============================================================
# 소스 파일 수집
# ============================================================
set(APP_SOURCES
    Core/Src/main.c
    Core/Src/pid.c
    Core/Src/voltage_ctrl.c
    Core/Src/freertos_tasks.c
)

# FreeRTOS 소스 (STM32CubeH5 패키지에서 경로 조정)
set(FREERTOS_ROOT "${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source")
set(FREERTOS_SOURCES
    ${FREERTOS_ROOT}/tasks.c
    ${FREERTOS_ROOT}/queue.c
    ${FREERTOS_ROOT}/list.c
    ${FREERTOS_ROOT}/timers.c
    ${FREERTOS_ROOT}/event_groups.c
    ${FREERTOS_ROOT}/stream_buffer.c
    ${FREERTOS_ROOT}/portable/MemMang/heap_4.c
    ${FREERTOS_ROOT}/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
    ${FREERTOS_ROOT}/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
)

# STM32H5 HAL 드라이버 (STM32CubeH5 패키지 경로)
set(HAL_ROOT "${CMAKE_SOURCE_DIR}/Drivers/STM32H5xx_HAL_Driver")
file(GLOB HAL_SOURCES "${HAL_ROOT}/Src/stm32h5xx_hal*.c")

# 스타트업 + CMSIS
set(STARTUP_SOURCES
    "${CMAKE_SOURCE_DIR}/startup/startup_stm32h563xx.s"
)

# ============================================================
# 인클루드 경로
# ============================================================
set(INCLUDE_DIRS
    Core/Inc
    ${FREERTOS_ROOT}/include
    ${FREERTOS_ROOT}/portable/GCC/ARM_CM33_NTZ/non_secure
    Drivers/STM32H5xx_HAL_Driver/Inc
    Drivers/CMSIS/Device/ST/STM32H5xx/Include
    Drivers/CMSIS/Include
)

# ============================================================
# 전처리기 정의
# ============================================================
set(COMPILE_DEFS
    STM32H563xx
    USE_HAL_DRIVER
    $<$<CONFIG:Debug>:DEBUG>
)

# ============================================================
# 빌드 타겟
# ============================================================
add_executable(${PROJECT_NAME}.elf
    ${APP_SOURCES}
    ${FREERTOS_SOURCES}
    ${HAL_SOURCES}
    ${STARTUP_SOURCES}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE ${COMPILE_DEFS})

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -Wall -Wextra -Wpedantic
    -fdata-sections -ffunction-sections
    -fno-common
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2>
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T "${CMAKE_SOURCE_DIR}/STM32H563ZITX_FLASH.ld"
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    --specs=nosys.specs
    --specs=nano.specs
)

# ============================================================
# Post-build: .bin / .hex 생성
# ============================================================
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex  $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE}             $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating ${PROJECT_NAME}.bin / .hex"
)
