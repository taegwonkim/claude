/**
 * @file    freertos_tasks.c
 * @brief   FreeRTOS 태스크 구현
 *
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │  vADCTask    (Priority: MAX-1)                                        │
 * │  ADC DMA 완료 세마포어 대기 → VCtrl_UpdateFeedback()                 │
 * ├──────────────────────────────────────────────────────────────────────┤
 * │  vControlTask (Priority: MAX-2)                                       │
 * │  10 ms 주기 (vTaskDelayUntil) → VCtrl_RunControl()                   │
 * ├──────────────────────────────────────────────────────────────────────┤
 * │  vUARTRxTask  (Priority: MAX-3)                                       │
 * │  PC → STM32 수신 명령 처리                                            │
 * │                                                                        │
 * │  ISR (HAL_UARTEx_RxEventCallback)                                     │
 * │    └─ xStreamBufferSendFromISR(xUARTRxStream, ...)                   │
 * │         └─ vUARTRxTask 깨움                                           │
 * │              └─ 줄 단위 조립 → UCmd_Process() → UART 응답            │
 * ├──────────────────────────────────────────────────────────────────────┤
 * │  vMonitorTask (Priority: IDLE+1)                                      │
 * │  g_stream_enabled == true  → 500 ms 마다 UCmd_PrintStream()          │
 * │  g_stream_enabled == false → 2000 ms 마다 전체 채널 상태 출력        │
 * └──────────────────────────────────────────────────────────────────────┘
 *
 * UART 프로토콜 요약:
 *   PC → STM32  : "SET 0 2.5\r\n", "GET ALL\r\n", "PID 0 200 50 5\r\n" …
 *   STM32 → PC  : "OK: SET 0 2.500V\r\n"
 *                 "#CH0,SET=2.500,FB=2.498,ERR=+0.002,OUT=3101,TYPE=DAC\r\n"
 *                 "@12345,1.000,1.500,2.000,2.500,1241,1861,2482,3103\r\n"
 */

#include "main.h"
#include "voltage_ctrl.h"
#include "uart_cmd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/*  상수                                                                      */
/* ======================================================================== */

#define CTRL_PERIOD_MS          10U     /**< PID 제어 주기 [ms] */
#define STREAM_PERIOD_MS        500U    /**< 스트림 출력 주기 [ms] */
#define STATUS_PERIOD_MS        2000U   /**< 스트림 OFF 시 상태 출력 주기 [ms] */
#define ADC_SEM_TIMEOUT_MS      50U

/* ======================================================================== */
/*  초기 목표 전압 테이블                                                     */
/* ======================================================================== */

static const float s_default_setpoints[VCTRL_NUM_CHANNELS] = {
    1.0f,   /**< 채널 0: 1.0 V */
    1.5f,   /**< 채널 1: 1.5 V */
    2.0f,   /**< 채널 2: 2.0 V */
    2.5f,   /**< 채널 3: 2.5 V */
};

/* ======================================================================== */
/*  UART 송신 헬퍼 (모든 태스크 공용)                                        */
/* ======================================================================== */

static void uart_tx(const char *str)
{
    HAL_UART_Transmit(&huart3, (const uint8_t *)str, (uint16_t)strlen(str), 200);
}

/* ======================================================================== */
/*  vADCTask                                                                  */
/* ======================================================================== */

void vADCTask(void *pvParameters)
{
    uint16_t *adc_buf = (uint16_t *)pvParameters;

    for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
        VCtrl_SetTarget(i, s_default_setpoints[i]);
    }

    uart_tx("[ADC] Task started\r\n");

    while (1) {
        if (xSemaphoreTake(xADCDoneSem, pdMS_TO_TICKS(ADC_SEM_TIMEOUT_MS)) == pdTRUE) {
#if defined(STM32H5xx) && defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
            SCB_InvalidateDCache_by_Addr((uint32_t *)adc_buf,
                                         VCTRL_NUM_CHANNELS * sizeof(uint16_t));
#endif
            if (xSemaphoreTake(xCtrlMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                VCtrl_UpdateFeedback(adc_buf);
                xSemaphoreGive(xCtrlMutex);
            }
        }
    }
}

/* ======================================================================== */
/*  vControlTask                                                              */
/* ======================================================================== */

void vControlTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(CTRL_PERIOD_MS);

    uart_tx("[CTRL] Task started\r\n");

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        if (xSemaphoreTake(xCtrlMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            VCtrl_RunControl();
            xSemaphoreGive(xCtrlMutex);
        }
    }
}

/* ======================================================================== */
/*  vUARTRxTask                                                               */
/*                                                                            */
/*  StreamBuffer에서 바이트를 읽어 줄(\n) 단위로 조립한 뒤                  */
/*  UCmd_Process()에 전달한다.                                               */
/*                                                                            */
/*  지원 line ending: \r\n  /  \n  /  \r                                    */
/* ======================================================================== */

void vUARTRxTask(void *pvParameters)
{
    (void)pvParameters;

    char    line_buf[UCMD_LINE_MAX];
    size_t  line_len = 0;
    uint8_t byte;

    uart_tx("[URXCMD] Task started\r\n");
    UCmd_PrintHelp();  /* 부팅 시 도움말 출력 */

    while (1) {
        /* StreamBuffer에서 1바이트씩 수신 (portMAX_DELAY: 데이터 올 때까지 블록) */
        size_t n = xStreamBufferReceive(xUARTRxStream, &byte, 1,
                                        portMAX_DELAY);
        if (n == 0) continue;

        /* ---- 줄 조립 ---- */
        if (byte == '\r') {
            /* \r 무시 또는 \r만 단독 줄끝으로 처리 */
            goto process_line;
        }

        if (byte == '\n') {
            goto process_line;
        }

        /* 백스페이스 처리 (터미널 편의) */
        if (byte == '\b' || byte == 0x7F) {
            if (line_len > 0) {
                line_len--;
                uart_tx("\b \b");  /* 터미널에서 문자 지우기 */
            }
            continue;
        }

        /* 일반 문자: 에코 + 버퍼 저장 */
        if (line_len < UCMD_LINE_MAX - 1) {
            line_buf[line_len++] = (char)byte;
            /* 로컬 에코 (옵션: PC 터미널 에코 OFF 시 활성화) */
            HAL_UART_Transmit(&huart3, &byte, 1, 10);
        }
        continue;

process_line:
        line_buf[line_len] = '\0';

        if (line_len > 0) {
            uart_tx("\r\n");            /* 에코 후 줄바꿈 */
            UCmd_Process(line_buf);     /* 명령 파싱 및 응답 */
        }
        line_len = 0;
    }
}

/* ======================================================================== */
/*  vMonitorTask                                                              */
/*                                                                            */
/*  g_stream_enabled == true  → STREAM_PERIOD_MS 마다 스트림 형식 출력      */
/*  g_stream_enabled == false → STATUS_PERIOD_MS 마다 상세 상태 출력        */
/* ======================================================================== */

void vMonitorTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();

    uart_tx("[MON] Task started\r\n");

    while (1) {
        if (g_stream_enabled) {
            /* 스트림 모드: 빠른 주기, 단순 CSV 형식 → 그래프/로그 수집 용이 */
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(STREAM_PERIOD_MS));
            UCmd_PrintStream();
        } else {
            /* 상태 모드: 느린 주기, 읽기 쉬운 상세 형식 */
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(STATUS_PERIOD_MS));

            uart_tx("--- Channel Status ---\r\n");
            for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
                UCmd_PrintChannelStatus(i);
            }
        }
    }
}
