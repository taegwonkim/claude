/*
 * FreeRTOSConfig.h - FreeRTOS 설정 파일
 * STM32L5 Cortex-M33 최적화 설정
 *
 * ⚠️  인터럽트 우선순위 규칙 (매우 중요!)
 *     configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
 *     → 우선순위 0~4 ISR: FreeRTOS API 절대 사용 금지
 *     → 우선순위 5~15 ISR: FromISR 함수 사용 가능
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* =========================================================
 * 1. 기본 설정
 * =========================================================*/

/* 선점형 스케줄링 (FreeRTOS 기본) */
#define configUSE_PREEMPTION                    1

/* 동일 우선순위 태스크 간 시간 분할 */
#define configUSE_TIME_SLICING                  1

/* 저전력 틱리스 idle (배터리 장치에 유용, 일단 비활성) */
#define configUSE_TICKLESS_IDLE                 0

/* 클럭 주파수: SystemCoreClock 사용 (동적 변경 지원) */
extern uint32_t SystemCoreClock;
#define configCPU_CLOCK_HZ                      (SystemCoreClock)

/* FreeRTOS 틱 주파수: 1000Hz = 1ms 해상도 */
#define configTICK_RATE_HZ                      ((TickType_t)1000)

/* 최대 태스크 우선순위 수 (0~6 = 7개 레벨) */
#define configMAX_PRIORITIES                    7

/* 최소 스택 크기 (words, = 128 × 4 = 512 bytes) */
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)

/* 태스크 이름 최대 길이 */
#define configMAX_TASK_NAME_LEN                 16

/* 16비트 틱 카운터 (32비트 MCU는 32비트 권장) */
#define configUSE_16_BIT_TICKS                  0

/* =========================================================
 * 2. 메모리 관리
 * =========================================================*/

/* FreeRTOS 힙 크기 (20KB) - SRAM 256KB 중 일부 */
#define configTOTAL_HEAP_SIZE                   ((size_t)(20 * 1024))

/* 힙 할당 방식: heap_4 권장 (병합 가능, 단편화 최소) */
/* Middleware/FreeRTOS/Source/portable/MemMang/heap_4.c 사용 */

/* =========================================================
 * 3. 동기화 객체
 * =========================================================*/

#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0

/* 큐 레지스트리 크기 (디버거에서 큐 이름 조회용) */
#define configQUEUE_REGISTRY_SIZE               10

/* =========================================================
 * 4. 소프트웨어 타이머
 * =========================================================*/

#define configUSE_TIMERS                        1
/* 타이머 태스크 우선순위: configMAX_PRIORITIES - 1 미만으로 설정 */
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 2)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* =========================================================
 * 5. 알림 (Task Notifications)
 * =========================================================*/

/* 태스크 알림 사용 (세마포어 대체, 더 빠름) */
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3

/* =========================================================
 * 6. 디버깅 및 모니터링
 * =========================================================*/

/* 런타임 통계 (정확한 CPU 사용률 측정, TIM 필요) */
#define configGENERATE_RUN_TIME_STATS           0

/* 스택 사용량 조회 함수 활성화 */
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/* 스택 오버플로우 감지 방법:
 * 0: 비활성
 * 1: 마지막 스택 워드 확인 (빠름)
 * 2: 마지막 16바이트 패턴 확인 (정확하지만 느림) ← 권장 */
#define configCHECK_FOR_STACK_OVERFLOW          2

/* 메모리 할당 실패 훅 */
#define configUSE_MALLOC_FAILED_HOOK            1

/* Idle 훅 (저전력 슬립 등에 사용) */
#define configUSE_IDLE_HOOK                     0

/* Tick 훅 */
#define configUSE_TICK_HOOK                     0

/* =========================================================
 * 7. Cortex-M33 인터럽트 우선순위 설정 (핵심!)
 * =========================================================*/

/*
 * STM32L5 NVIC: 4비트 우선순위 (0=최고, 15=최저)
 *
 * FreeRTOS 임계 구역 진입 시:
 * BASEPRI를 configMAX_SYSCALL_INTERRUPT_PRIORITY로 설정
 * → 이 값보다 낮은(숫자 큰) 우선순위 ISR만 마스킹됨
 * → 높은 우선순위(0~4) ISR은 항상 실행 가능
 */

/* 라이브러리 수준 (실제 우선순위 비트 = 4비트) */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

/* HAL이 사용하는 형식으로 변환 (왼쪽 시프트 4비트) */
/* STM32L5는 __NVIC_PRIO_BITS = 4 */
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - 4))        /* = 0xF0 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - 4))   /* = 0x50 */

/* =========================================================
 * 8. Cortex-M33 특화 설정
 * =========================================================*/

/* TrustZone 비보안 포트 사용 (STM32L5 기본) */
/* ARM_CM33_NTZ (Non-TrustZone) 포트 사용 */

/* MPU 사용 여부 */
#define configENABLE_MPU                        0

/* FPU 사용 (Cortex-M33에 FPU 내장) */
#define configENABLE_FPU                        1

/* TrustZone 비활성화 (단순 애플리케이션) */
#define configENABLE_TRUSTZONE                  0

/* Secure 스택 크기 (TrustZone 비활성 시 무관) */
#define configMINIMAL_SECURE_STACK_SIZE         ((uint16_t)128)

/* =========================================================
 * 9. CMSIS-OS v2 호환 설정
 * =========================================================*/

/* CMSIS-OS API 포함 */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_uxTaskGetStackHighWaterMark     1  /* 스택 모니터링 */
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_eTaskGetState                   1

/* vTaskList, vTaskGetRunTimeStats 사용 */
#define INCLUDE_xTaskGetIdleTaskHandle          1

/* =========================================================
 * 10. 어설션 (디버그 빌드에서 활성화)
 * =========================================================*/
#ifdef DEBUG
    #define configASSERT(x) \
        if ((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }
#else
    #define configASSERT(x)
#endif

#endif /* FREERTOS_CONFIG_H */
