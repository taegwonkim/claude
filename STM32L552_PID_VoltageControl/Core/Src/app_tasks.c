/* app_tasks.c - FreeRTOS 태스크 구현
 *
 * 태스크 구조:
 *   Task_PIDControl  (2ms)  : ADC 읽기 → PID 계산 → PWM 갱신
 *   Task_FDCANTx     (50ms) : 4채널 텔레메트리 전송
 *   Task_FDCANRx     (이벤트): 수신 큐 처리
 *   Task_Monitor     (1s)   : 스택 워터마크, LED 상태
 */
#include "app_tasks.h"
#include "channel_ctrl.h"
#include "adc_meas.h"
#include "pwm_out.h"
#include "fdcan_comm.h"
#include "main.h"

/* ==========================================================
 * 공유 FreeRTOS 객체 정의
 * ========================================================== */
SemaphoreHandle_t g_channel_mutex   = NULL;
QueueHandle_t     g_fdcan_cmd_queue = NULL;
TaskHandle_t      g_pid_task_handle  = NULL;
TaskHandle_t      g_fdcan_tx_handle  = NULL;
TaskHandle_t      g_fdcan_rx_handle  = NULL;
TaskHandle_t      g_monitor_handle   = NULL;

/* ==========================================================
 * AppTasks_Init
 * ========================================================== */
void AppTasks_Init(void)
{
    /* 뮤텍스 생성: 채널 데이터 동시 접근 보호 */
    g_channel_mutex = xSemaphoreCreateMutex();
    configASSERT(g_channel_mutex != NULL);

    /* FDCAN 수신 명령 큐 생성 */
    g_fdcan_cmd_queue = xQueueCreate(FDCAN_CMD_QUEUE_LEN,
                                      sizeof(FDCAN_RxCmd_t));
    configASSERT(g_fdcan_cmd_queue != NULL);

    /* 태스크 생성 */
    BaseType_t ret;

    ret = xTaskCreate(Task_PIDControl,
                      "PID_CTRL",
                      STACK_PID_CTRL,
                      NULL,
                      PRIO_PID_CTRL,
                      &g_pid_task_handle);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(Task_FDCANTx,
                      "FDCAN_TX",
                      STACK_FDCAN_TX,
                      NULL,
                      PRIO_FDCAN_TX,
                      &g_fdcan_tx_handle);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(Task_FDCANRx,
                      "FDCAN_RX",
                      STACK_FDCAN_RX,
                      NULL,
                      PRIO_FDCAN_RX,
                      &g_fdcan_rx_handle);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(Task_Monitor,
                      "MONITOR",
                      STACK_MONITOR,
                      NULL,
                      PRIO_MONITOR,
                      &g_monitor_handle);
    configASSERT(ret == pdPASS);
}

/* ==========================================================
 * Task_PIDControl  (주기: 2ms, 우선순위: 5)
 *
 *  루프:
 *    1. ADC 변환 완료 대기 (플래그 폴링, 최대 2ms)
 *    2. 채널별 측정값 갱신
 *    3. 채널별 PID 계산 → PWM 출력
 *    4. 다음 주기까지 대기
 * ========================================================== */
void Task_PIDControl(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* ADC 변환 완료 대기 (최대 1ms, 그 이후면 이전 값 사용) */
        uint32_t timeout = 100U; /* 100 * 10us = 1ms polling */
        while (!g_adc_conv_cplt && timeout > 0U) {
            timeout--;
        }
        g_adc_conv_cplt = 0U;

        /* 채널 데이터 뮤텍스 취득 */
        if (xSemaphoreTake(g_channel_mutex, pdMS_TO_TICKS(1U)) == pdTRUE) {

            for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
                /* 측정값 갱신 */
                uint32_t v_mV = ADCMeas_GetVoltage_mV(ch);
                uint32_t i_mA = ADCMeas_GetCurrent_mA(ch);
                ChannelCtrl_SetMeasured(ch, v_mV, i_mA);

                /* PID 계산 → PWM 출력 갱신 */
                ChannelCtrl_Update(ch);
            }

            xSemaphoreGive(g_channel_mutex);
        }

        /* 2ms 주기 유지 (vTaskDelayUntil: 지터 보상) */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIOD_PID_MS));
    }
}

/* ==========================================================
 * Task_FDCANTx  (주기: 50ms, 우선순위: 3)
 *
 *  루프:
 *    1. 4채널 텔레메트리 전송
 *    2. heartbeat 전송
 *    3. 50ms 대기
 * ========================================================== */
void Task_FDCANTx(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* 채널 데이터 읽기 (뮤텍스) */
        if (xSemaphoreTake(g_channel_mutex, pdMS_TO_TICKS(5U)) == pdTRUE) {

            for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
                FDCANComm_SendTelemetry(ch);
                /* TX FIFO 포화 방지를 위한 미세 지연 */
                vTaskDelay(pdMS_TO_TICKS(1U));
            }

            FDCANComm_SendHeartbeat();

            xSemaphoreGive(g_channel_mutex);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIOD_FDCAN_TX_MS));
    }
}

/* ==========================================================
 * Task_FDCANRx  (이벤트 주도, 우선순위: 4)
 *
 *  루프:
 *    1. 명령 큐 처리 (블로킹 대기 10ms)
 *    2. 뮤텍스 보호 하에 채널 설정 적용
 * ========================================================== */
void Task_FDCANRx(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        FDCAN_RxCmd_t cmd;

        /* 큐 대기 (최대 10ms) */
        if (xQueueReceive(g_fdcan_cmd_queue,
                          &cmd,
                          pdMS_TO_TICKS(10U)) == pdTRUE) {

            /* 명령 처리는 채널 뮤텍스 보호 하에 */
            if (xSemaphoreTake(g_channel_mutex, pdMS_TO_TICKS(5U)) == pdTRUE) {
                /* 큐에 넣은 명령을 다시 임시로 큐에 넣어서
                   ProcessCommands()가 일괄 처리하도록 함 */
                xQueueSendToFront(g_fdcan_cmd_queue, &cmd, 0);
                FDCANComm_ProcessCommands();
                xSemaphoreGive(g_channel_mutex);
            }
        }
    }
}

/* ==========================================================
 * Task_Monitor  (주기: 1000ms, 우선순위: 2)
 *
 *  루프:
 *    1. 각 태스크 스택 워터마크 확인
 *    2. LED 상태 업데이트
 *    3. IWDG 리프레시
 * ========================================================== */
void Task_Monitor(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* 오류 채널 확인 */
        uint8_t fault = 0U;
        for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
            if (g_channels[ch].status & (CH_STATUS_OVP | CH_STATUS_OCP | CH_STATUS_FAULT)) {
                fault = 1U;
            }
        }

        /* LED: 오류 시 RED 점멸, 정상 시 GREEN 토글 */
        if (fault) {
            HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
            HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
        } else {
            HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
        }

        /* IWDG 리프레시 (와치독 타이아웃 전에 반드시 호출) */
        HAL_IWDG_Refresh(&hiwdg);

        /* 스택 워터마크 디버그 (최소 여유 확인) */
        /* 필요 시 여기에 UART 출력 또는 변수 저장 */
        (void)uxTaskGetStackHighWaterMark(g_pid_task_handle);
        (void)uxTaskGetStackHighWaterMark(g_fdcan_tx_handle);
        (void)uxTaskGetStackHighWaterMark(g_fdcan_rx_handle);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIOD_MONITOR_MS));
    }
}

/* ==========================================================
 * FreeRTOS 훅 함수
 * ========================================================== */

/* 스택 오버플로우 감지 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /* 즉시 시스템 정지 → IWDG 리셋 유도 */
    __disable_irq();
    for (;;) {}
}

/* 힙 할당 실패 */
void vApplicationMallocFailedHook(void)
{
    __disable_irq();
    for (;;) {}
}
