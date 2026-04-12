/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS 설정 파일 — STM32L552R, 110 MHz
 *
 * CMSIS-RTOS v2 래퍼와 함께 사용.
 * STM32CubeIDE에서 CubeMX가 자동 생성하는 파일과 동일한 역할.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Cortex-M33 특화 설정 */
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0

/* 기본 설정 */
#define configUSE_PREEMPTION                    1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      110000000UL   /* 110 MHz */
#define configTICK_RATE_HZ                      1000U         /* 1 ms tick */
#define configMAX_PRIORITIES                    7U
#define configMINIMAL_STACK_SIZE                128U          /* word */
#define configMAX_TASK_NAME_LEN                 16U
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8U
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configUSE_POSIX_ERRNO                   0

/* 메모리 관리 */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ( 10 * 1024 )  /* 10 kB */
#define configAPPLICATION_ALLOCATED_HEAP        0

/* 훅 함수 */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0
#define configCHECK_FOR_STACK_OVERFLOW          2  /* 스택 오버플로우 감지 활성 */

/* 런타임 통계 */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* 코루틴 (미사용) */
#define configUSE_CO_ROUTINES                   0

/* 소프트웨어 타이머 */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                10U
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/* ------------------------------------------------------------------ */
/*  인터럽트 우선순위                                                      */
/*  STM32L5 NVIC: 4비트 우선순위 그룹 → 0~15                             */
/*  FreeRTOS syscall threshold: 우선순위 5 이상 (낮은 번호 = 높은 우선순위) */
/* ------------------------------------------------------------------ */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY     15U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5U

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configPRIO_BITS                         4U   /* STM32L5: 4비트 */

/* ------------------------------------------------------------------ */
/*  FreeRTOS → CMSIS 핸들러 매핑                                         */
/* ------------------------------------------------------------------ */
#define vPortSVCHandler         SVC_Handler
#define xPortPendSVHandler      PendSV_Handler
#define xPortSysTickHandler     SysTick_Handler

/* ------------------------------------------------------------------ */
/*  선택적 API 활성화                                                     */
/* ------------------------------------------------------------------ */
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskCleanUpResources       0
#define INCLUDE_vTaskSuspend                1
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
#define INCLUDE_xTaskGetHandle              0

#endif /* FREERTOS_CONFIG_H */
