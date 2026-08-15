/**
 * FreeRTOSConfig.h — CubeMX Middleware(FreeRTOS) 설정과 동일하게 유지
 * (CubeMX "Generate Code" 시 자동 생성되는 파일입니다. 본 저장소 버전은
 *  README.md 3.8절 표와 1:1로 대응하도록 수치를 맞춰 두었습니다.)
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;

#define configUSE_PREEMPTION                    1
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     (56)
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)
#define configTOTAL_HEAP_SIZE                    ((size_t)20480)
#define configMAX_TASK_NAME_LEN                  16
#define configUSE_TRACE_FACILITY                 1
#define configUSE_16_BIT_TICKS                   0
#define configUSE_MUTEXES                        1
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configUSE_MALLOC_FAILED_HOOK             1
#define configENABLE_BACKWARD_COMPATIBILITY      0

/* Co-routine 미사용 */
#define configUSE_CO_ROUTINES                    0
#define configMAX_CO_ROUTINE_PRIORITIES          2

/* 소프트웨어 타이머 (tmrHeartbeat / tmrWifiWatchdog 용) */
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                (55)
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             256

/* Cortex-M33 NVIC 우선순위 비트 수 (STM32L5) */
#define configPRIO_BITS                          4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Optional functions */
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTimerPendFunctionCall      1
#define INCLUDE_xQueueGetMutexHolder        1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_eTaskGetState               1

#define USE_CUSTOM_SYSTICK_HANDLER_IMPLEMENTATION 0

#ifdef __NVIC_PRIO_BITS
#define configPRIO_BITS __NVIC_PRIO_BITS
#endif

#endif /* FREERTOS_CONFIG_H */
