/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS 설정 파일 (STM32H563 / 250 MHz)
 *
 * 참고: STM32H5는 Cortex-M33 코어(TrustZone 지원)
 *      configUSE_MPU_WRAPPERS는 선택적으로 활성화 가능
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ======================================================================== */
/*  기본 설정                                                                 */
/* ======================================================================== */

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      250000000UL  /* 250 MHz */
#define configTICK_RATE_HZ                      1000U        /* 1 kHz = 1 ms tick */
#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                128U         /* words */
#define configMAX_TASK_NAME_LEN                 12
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_READER_WRITER_LOCKS           0
#define configSTACK_DEPTH_TYPE                  uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t

/* ======================================================================== */
/*  메모리 할당                                                               */
/* ======================================================================== */

#define configSUPPORT_STATIC_ALLOCATION         0   /* heap_4.c 사용 */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (32 * 1024)  /* 32 KB */
#define configAPPLICATION_ALLOCATED_HEAP        0

/* ======================================================================== */
/*  훅 함수                                                                   */
/* ======================================================================== */

#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2   /* 스택 오버플로 감지 */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* ======================================================================== */
/*  런타임 통계 (선택)                                                        */
/* ======================================================================== */

#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ======================================================================== */
/*  코-루틴 (미사용)                                                          */
/* ======================================================================== */

#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

/* ======================================================================== */
/*  소프트웨어 타이머                                                         */
/* ======================================================================== */

#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                8
#define configTIMER_TASK_STACK_DEPTH            256U

/* ======================================================================== */
/*  포함할 API 함수                                                           */
/* ======================================================================== */

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
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0

/* ======================================================================== */
/*  Cortex-M33 인터럽트 우선순위                                              */
/*                                                                            */
/*  STM32H5 NVIC: 4비트 우선순위 → 0(최고) ~ 15(최저)                       */
/*  FreeRTOS 관리 인터럽트: configMAX_SYSCALL_INTERRUPT_PRIORITY 이하        */
/*  FreeRTOS 비관리 인터럽트: configMAX_SYSCALL_INTERRUPT_PRIORITY 초과      */
/* ======================================================================== */

/* 인터럽트 우선순위 비트수 (NVIC 구현) */
#define configPRIO_BITS                         4U

/* FreeRTOS가 관리하는 인터럽트의 최저 우선순위 (숫자가 낮을수록 높은 우선순위) */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15U

/* FreeRTOS API를 ISR에서 호출할 수 있는 최고 우선순위 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY     5U

/* CMSIS 형태 (숫자를 shift) */
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

/* SysTick / PendSV 우선순위 (Cortex-M 포트 필수) */
#define configSYSTICK_CLOCK_HZ                  configCPU_CLOCK_HZ

/* ======================================================================== */
/*  어설션 (디버그 빌드)                                                      */
/* ======================================================================== */

#define configASSERT(x) \
    do { if ((x) == 0) { taskDISABLE_INTERRUPTS(); while(1){} } } while(0)

/* ======================================================================== */
/*  Cortex-M33 TrustZone 설정 (보안 영역 미사용 시 0)                        */
/* ======================================================================== */

#define configRUN_FREERTOS_SECURE_ONLY          0
#define configENABLE_TRUSTZONE                  0
#define configENABLE_FPU                        1   /* M33 FPU 활성화 */
#define configENABLE_MVE                        0   /* Helium SIMD (선택) */

#endif /* FREERTOS_CONFIG_H */
