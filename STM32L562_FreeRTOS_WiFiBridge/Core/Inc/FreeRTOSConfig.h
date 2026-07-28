/**
 * FreeRTOSConfig.h
 *
 * Reference configuration for this project on STM32L562 (Cortex-M33).
 *
 * NOTE: STM32CubeMX regenerates this file from the FREERTOS middleware
 * "Config parameters" / "Advanced settings" GUI panels every time you
 * click "Generate Code". Use this file as the source of truth for which
 * *values* to enter in those GUI panels (see
 * docs/FREERTOS_TASK_MEMORY.md, section 5) rather than hand-copying it
 * into a CubeMX-managed project, so regeneration doesn't overwrite your
 * intent silently.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ------------------------------------------------------------------------ *
 * Scheduling
 * ------------------------------------------------------------------------ */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                  0
#define configCPU_CLOCK_HZ                      (SystemCoreClock)
#define configTICK_RATE_HZ                      ((TickType_t)1000)

/* CMSIS-RTOS2 requires exactly 56 priority levels - do not change. */
#define configMAX_PRIORITIES                    (56)
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)  /* words, Idle task */
#define configMAX_TASK_NAME_LEN                 (16)
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

/* ------------------------------------------------------------------------ *
 * Synchronization primitives (mutex used by app_eeprom.c, semaphores by
 * app_esp32.c / app_fpga_if.c, message queue by app_esp32.c, event flags
 * by app_esp32.c)
 * ------------------------------------------------------------------------ */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_APPLICATION_TASK_TAG          0

/* ------------------------------------------------------------------------ *
 * Memory allocation - heap_4.c (CubeMX default), see
 * docs/FREERTOS_TASK_MEMORY.md section 3 for the sizing rationale.
 * ------------------------------------------------------------------------ */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ((size_t)24 * 1024)
#define configAPPLICATION_ALLOCATED_HEAP        0

/* ------------------------------------------------------------------------ *
 * Hooks
 * ------------------------------------------------------------------------ */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* ------------------------------------------------------------------------ *
 * Timer service task - not used directly by this application (osDelay()
 * uses vTaskDelay, not software timers), left at CubeMX defaults.
 * ------------------------------------------------------------------------ */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 2)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/* ------------------------------------------------------------------------ *
 * Runtime/debug stats - disabled by default; enable if using
 * Percepio Tracealyzer / SEGGER SystemView.
 * ------------------------------------------------------------------------ */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ------------------------------------------------------------------------ *
 * Optional functions
 * ------------------------------------------------------------------------ */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              1

/* ------------------------------------------------------------------------ *
 * Cortex-M33 (ARMv8-M) specific
 * ------------------------------------------------------------------------ */
#ifdef __NVIC_PRIO_BITS
#define configPRIO_BITS                         __NVIC_PRIO_BITS
#else
#define configPRIO_BITS                         4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ARMv8-M FPU / TrustZone-M. This project assumes CubeMX is configured
 * WITHOUT TrustZone (single, non-secure image) - the common/simplest
 * setup unless you specifically need a secure/non-secure partition.
 * If you enable TrustZone in CubeMX, let it regenerate this section
 * (configENABLE_TRUSTZONE, configRUN_FREERTOS_SECURE_ONLY, MPU regions
 * etc.) instead of reusing these values. */
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0

#define configASSERT(x) \
    if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;) {} }

/* CMSIS-RTOS2 / FreeRTOS interrupt handler name mapping. FreeRTOS's
 * Cortex-M port implements these under the vPort../xPort.. names; the
 * defines below alias them to the CMSIS vector-table names expected by
 * startup_stm32l562xx.s. This matches CubeMX's generated output for the
 * ARM_CM33_NTZ (non-TrustZone) port - if CubeMX selects a different
 * FreeRTOS port variant for your TrustZone setting, use its generated
 * block instead of this one. */
#define vPortSVCHandler       SVC_Handler
#define xPortPendSVHandler    PendSV_Handler
#define xPortSysTickHandler   SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
