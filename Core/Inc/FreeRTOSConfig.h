/**
 ******************************************************************************
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS 커널 설정 (STM32L552R, Cortex-M33)
 *
 * 주요 설정:
 *   - Preemptive scheduling
 *   - SysTick 1ms tick (1000Hz)
 *   - 4 priority levels
 *   - Heap4 메모리 관리
 *   - Stack overflow detection enabled
 *   - Mutexes enabled
 ******************************************************************************
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Cortex-M33 specific defines */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS         __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS         4  /* STM32L5 = 4-bit priority */
#endif

/*-----------------------------------------------------------
 * Kernel Configuration
 *-----------------------------------------------------------*/
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      (110000000UL)  /* 110MHz */
#define configTICK_RATE_HZ                      ((TickType_t)1000)  /* 1ms tick */
#define configMAX_PRIORITIES                    (7)
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)  /* words */
#define configTOTAL_HEAP_SIZE                   ((size_t)(32 * 1024))  /* 32KB */
#define configMAX_TASK_NAME_LEN                 (16)
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configSTACK_DEPTH_TYPE                  uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t
#define configENABLE_BACKWARD_COMPATIBILITY     0

/* Memory allocation scheme */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/*-----------------------------------------------------------
 * Hook Functions
 *-----------------------------------------------------------*/
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     1
#define configCHECK_FOR_STACK_OVERFLOW          2  /* Both methods */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/*-----------------------------------------------------------
 * Runtime stats (disabled for production)
 *-----------------------------------------------------------*/
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/*-----------------------------------------------------------
 * Co-routine (disabled)
 *-----------------------------------------------------------*/
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         (2)

/*-----------------------------------------------------------
 * Software timer (disabled)
 *-----------------------------------------------------------*/
#define configUSE_TIMERS                        0
#define configTIMER_TASK_PRIORITY               (2)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/*-----------------------------------------------------------
 * Optional function includes
 *-----------------------------------------------------------*/
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xQueueGetMutexHolder            1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_eTaskGetState                   1

/*-----------------------------------------------------------
 * Cortex-M33 Interrupt Priority Configuration
 *-----------------------------------------------------------*/

/*
 * NVIC 우선순위 비트 설정:
 * STM32L5는 4-bit (16 levels), FreeRTOS는 상위 비트를 사용
 *
 * configLIBRARY_LOWEST_INTERRUPT_PRIORITY:
 *   FreeRTOS에서 사용할 수 있는 가장 낮은 인터럽트 우선순위 (숫자가 큰 것)
 *
 * configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY:
 *   FreeRTOS API를 호출할 수 있는 최고 인터럽트 우선순위
 *   이 값보다 높은(숫자가 작은) 우선순위의 ISR에서는 FreeRTOS API 사용 불가
 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

/* 하드웨어 레지스터에 쓸 실제 우선순위 값 */
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/*-----------------------------------------------------------
 * Cortex-M33 TrustZone (비보안 모드로 동작)
 *-----------------------------------------------------------*/
#define configENABLE_TRUSTZONE                  0
#define configRUN_FREERTOS_SECURE_ONLY          0
#define configENABLE_MPU                        0
#define configENABLE_FPU                        1
#define configENABLE_MVE                        0

/*-----------------------------------------------------------
 * Assert (디버그용)
 *-----------------------------------------------------------*/
#define configASSERT(x) if ((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

/*-----------------------------------------------------------
 * FreeRTOS interrupt handler mapping
 * (port.c의 핸들러와 STM32 벡터 테이블 매핑)
 *-----------------------------------------------------------*/
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler

/* SysTick은 stm32l5xx_it.c에서 수동 처리 */

#endif /* FREERTOS_CONFIG_H */
