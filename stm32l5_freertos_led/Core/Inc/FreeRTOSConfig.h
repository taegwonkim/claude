/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS 설정 파일
 *
 * STM32L5 (Cortex-M33) + TIM6 기반 SysTick
 *
 * 핵심 설정:
 *   - configUSE_TICKLESS_IDLE = 0  (TIM6 인터럽트로 틱 공급)
 *   - configOVERRIDE_DEFAULT_TICK_CONFIGURATION = 1
 *     -> vPortSetupTimerInterrupt() 를 직접 구현하여 TIM6 사용
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32l5xx_hal.h"  /* SystemCoreClock 참조 */

/* ---------------------------------------------------------------
 * 기본 커널 설정
 * --------------------------------------------------------------- */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      SystemCoreClock
#define configTICK_RATE_HZ                      1000u          /* 1 ms 틱 */
#define configMAX_PRIORITIES                    7
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#define configSTACK_DEPTH_TYPE                  uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t

/* ---------------------------------------------------------------
 * 메모리 할당
 * --------------------------------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ( 16 * 1024 )  /* 16 KB */
#define configAPPLICATION_ALLOCATED_HEAP        0

/* ---------------------------------------------------------------
 * TIM6 를 FreeRTOS 틱 소스로 사용
 * 이 매크로를 1로 설정하면 커널이 vPortSetupTimerInterrupt()를 호출하고
 * 내부 SysTick 설정 코드를 건너뜀.
 * --------------------------------------------------------------- */
#define configOVERRIDE_DEFAULT_TICK_CONFIGURATION   1

/* ---------------------------------------------------------------
 * Hook 함수
 * --------------------------------------------------------------- */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1

/* ---------------------------------------------------------------
 * 런타임 통계 / 디버그
 * --------------------------------------------------------------- */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ---------------------------------------------------------------
 * 코루틴 (사용 안 함)
 * --------------------------------------------------------------- */
#define configUSE_CO_ROUTINES                   0

/* ---------------------------------------------------------------
 * 소프트웨어 타이머
 * --------------------------------------------------------------- */
#define configUSE_TIMERS                        0

/* ---------------------------------------------------------------
 * Cortex-M33 인터럽트 우선순위
 *   STM32L5 NVIC: 3비트 = 0~7 우선순위
 *   FreeRTOS 관리 인터럽트는 configMAX_SYSCALL_INTERRUPT_PRIORITY
 *   이하(숫자 >=)만 허용.
 * --------------------------------------------------------------- */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         7
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY         \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - __NVIC_PRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - __NVIC_PRIO_BITS) )

/* ---------------------------------------------------------------
 * API 함수 포함 여부
 * --------------------------------------------------------------- */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
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

/* ---------------------------------------------------------------
 * Cortex-M33 어설션 / 포트 매핑
 * --------------------------------------------------------------- */
#define configASSERT( x ) \
    if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for(;;); }

/* ARM_CM33_NTZ 포트에 필요한 추가 설정 */
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0
#define configRUN_FREERTOS_SECURE_ONLY          0

#endif /* FREERTOS_CONFIG_H */
