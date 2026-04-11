/* app_tasks.h - FreeRTOS 태스크 및 공유 객체 선언 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ==========================================================
 * 태스크 스택 크기 (words, 1 word = 4 bytes on Cortex-M33)
 * ========================================================== */
#define STACK_PID_CTRL      512U    /* PID 제어 태스크 */
#define STACK_FDCAN_TX      384U    /* FDCAN 송신 태스크 */
#define STACK_FDCAN_RX      384U    /* FDCAN 수신 태스크 */
#define STACK_MONITOR       256U    /* 시스템 모니터 태스크 */

/* ==========================================================
 * 태스크 우선순위 (높을수록 먼저 실행)
 * ========================================================== */
#define PRIO_PID_CTRL       5U      /* 최고 우선순위 */
#define PRIO_FDCAN_RX       4U
#define PRIO_FDCAN_TX       3U
#define PRIO_MONITOR        2U

/* ==========================================================
 * 태스크 주기
 * ========================================================== */
#define PERIOD_PID_MS       2U      /* 2ms = 500Hz PID 루프 */
#define PERIOD_FDCAN_TX_MS  50U     /* 50ms = 20Hz 텔레메트리 */
#define PERIOD_MONITOR_MS   1000U   /* 1s 시스템 모니터 */

/* ==========================================================
 * 명령 큐 설정
 * ========================================================== */
#define FDCAN_CMD_QUEUE_LEN 16U

/* ==========================================================
 * 공유 FreeRTOS 객체 (app_tasks.c 에서 정의)
 * ========================================================== */
extern SemaphoreHandle_t g_channel_mutex;   /* 채널 데이터 보호 뮤텍스 */
extern QueueHandle_t     g_fdcan_cmd_queue; /* FDCAN 수신 명령 큐 */
extern TaskHandle_t      g_pid_task_handle;
extern TaskHandle_t      g_fdcan_tx_handle;
extern TaskHandle_t      g_fdcan_rx_handle;
extern TaskHandle_t      g_monitor_handle;

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */

/** @brief FreeRTOS 객체 생성 및 태스크 시작 */
void AppTasks_Init(void);

/* 태스크 함수 */
void Task_PIDControl(void *pvParameters);
void Task_FDCANTx(void *pvParameters);
void Task_FDCANRx(void *pvParameters);
void Task_Monitor(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* APP_TASKS_H */
