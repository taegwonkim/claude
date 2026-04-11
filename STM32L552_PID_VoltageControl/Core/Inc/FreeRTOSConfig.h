/* FreeRTOSConfig.h - STM32L552 (Cortex-M33) FreeRTOS 설정 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;

/* ==========================================================
 * Cortex-M33 포트 설정
 * ========================================================== */
#define configENABLE_TRUSTZONE              0   /* TrustZone 비사용 */
#define configRUN_FREERTOS_SECURE_ONLY      1
#define configENABLE_MPU                    0
#define configENABLE_FPU                    1

/* ==========================================================
 * 기본 커널 설정
 * ========================================================== */
#define configUSE_PREEMPTION                1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE             0
#define configCPU_CLOCK_HZ                  ( SystemCoreClock )
#define configTICK_RATE_HZ                  ((TickType_t)1000)   /* 1ms tick */
#define configMAX_PRIORITIES                7
#define configMINIMAL_STACK_SIZE            ((uint16_t)256)
#define configMAX_TASK_NAME_LEN             16
#define configUSE_16_BIT_TICKS              0
#define configIDLE_SHOULD_YIELD             1
#define configUSE_TASK_NOTIFICATIONS        1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1
#define configUSE_MUTEXES                   1
#define configUSE_RECURSIVE_MUTEXES         0
#define configUSE_COUNTING_SEMAPHORES       1
#define configQUEUE_REGISTRY_SIZE           8
#define configUSE_QUEUE_SETS                0
#define configUSE_TIME_SLICING              1
#define configUSE_NEWLIB_REENTRANT          0
#define configENABLE_BACKWARD_COMPATIBILITY 0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#define configSTACK_DEPTH_TYPE              uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE    size_t

/* ==========================================================
 * 메모리 할당
 * ========================================================== */
#define configSUPPORT_STATIC_ALLOCATION     0
#define configSUPPORT_DYNAMIC_ALLOCATION    1
#define configTOTAL_HEAP_SIZE               ((size_t)(40960))    /* 40KB */
#define configAPPLICATION_ALLOCATED_HEAP    0

/* ==========================================================
 * 훅 함수
 * ========================================================== */
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0
#define configCHECK_FOR_STACK_OVERFLOW      2   /* 스택 오버플로우 감지 */
#define configUSE_MALLOC_FAILED_HOOK        1

/* ==========================================================
 * 런타임 통계 (비활성화)
 * ========================================================== */
#define configGENERATE_RUN_TIME_STATS       0
#define configUSE_TRACE_FACILITY            0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0

/* ==========================================================
 * 코루틴 (미사용)
 * ========================================================== */
#define configUSE_CO_ROUTINES               0
#define configMAX_CO_ROUTINE_PRIORITIES     1

/* ==========================================================
 * 소프트웨어 타이머
 * ========================================================== */
#define configUSE_TIMERS                    1
#define configTIMER_TASK_PRIORITY           ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH            10
#define configTIMER_TASK_STACK_DEPTH        256

/* ==========================================================
 * 인터럽트 우선순위 (Cortex-M33: 4비트 = 0~15)
 * FreeRTOS는 configMAX_SYSCALL_INTERRUPT_PRIORITY 이상의
 * 인터럽트에서는 FreeRTOS API를 호출하면 안 됨
 * ========================================================== */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY     \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - 4) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - 4) )

/* ==========================================================
 * INCLUDE 함수 선택
 * ========================================================== */
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_xResumeFromISR              1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetIdleTaskHandle      0
#define INCLUDE_eTaskGetState               1
#define INCLUDE_xEventGroupSetBitFromISR    1
#define INCLUDE_xTimerPendFunctionCall      1
#define INCLUDE_xTaskAbortDelay             0
#define INCLUDE_xTaskGetHandle              1
#define INCLUDE_xTaskResumeFromISR          1

/* ==========================================================
 * Cortex-M33 벡터 이름 매핑
 * ========================================================== */
#define xPortPendSVHandler      PendSV_Handler
#define vPortSVCHandler         SVC_Handler
#define xPortSysTickHandler     SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
