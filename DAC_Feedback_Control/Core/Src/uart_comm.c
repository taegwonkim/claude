/**
 ******************************************************************************
 * @file    uart_comm.c
 * @brief   UART PC 통신 모듈 구현
 *
 * DMA 수신: HAL_UARTEx_ReceiveToIdle_DMA 사용
 *           수신 완료 또는 IDLE 이벤트 시 HAL_UARTEx_RxEventCallback 호출
 ******************************************************************************
 */

#include "uart_comm.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

/* -------------------------------------------------------------------------
 * 내부 버퍼
 * ------------------------------------------------------------------------- */
static uint8_t  s_rx_dma_buf[UART_RX_BUF_SIZE];
static uint8_t  s_rx_line_buf[UART_MAX_CMD_LEN];
static uint16_t s_rx_line_len = 0;
static uint16_t s_rx_prev_pos = 0;    /* DMA 이전 처리 위치 */

static uint8_t  s_tx_buf[UART_TX_BUF_SIZE];
static volatile uint8_t s_tx_busy = 0;

/* -------------------------------------------------------------------------
 * 명령 파서 유틸리티
 * ------------------------------------------------------------------------- */

/* 문자열 앞뒤 공백 제거 (in-place) */
static char *_trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return s;
}

static void _parse_and_enqueue(char *line)
{
    if (line == NULL || *line == '\0') return;

    char *p = _trim(line);
    UartCmd_t cmd = { 0 };

    if (strncasecmp(p, "SET", 3) == 0) {
        /* SET <ch> <mV> */
        cmd.type = CMD_SET_VOLTAGE;
        if (sscanf(p + 3, " %hhu %hu", &cmd.channel, &cmd.value) == 2) {
            if (cmd.channel >= 1 && cmd.channel <= NUM_CHANNELS) {
                cmd.channel -= 1;   /* 1-based → 0-based */
                osMessageQueuePut(xQueueUARTCmd, &cmd, 0, 0);
            }
        }
    } else if (strncasecmp(p, "EN", 2) == 0) {
        cmd.type = CMD_ENABLE_CH;
        if (sscanf(p + 2, " %hhu", &cmd.channel) == 1) {
            if (cmd.channel >= 1 && cmd.channel <= NUM_CHANNELS) {
                cmd.channel -= 1;
                osMessageQueuePut(xQueueUARTCmd, &cmd, 0, 0);
            }
        }
    } else if (strncasecmp(p, "DIS", 3) == 0) {
        cmd.type = CMD_DISABLE_CH;
        if (sscanf(p + 3, " %hhu", &cmd.channel) == 1) {
            if (cmd.channel >= 1 && cmd.channel <= NUM_CHANNELS) {
                cmd.channel -= 1;
                osMessageQueuePut(xQueueUARTCmd, &cmd, 0, 0);
            }
        }
    } else if (strncasecmp(p, "RST", 3) == 0) {
        cmd.type = CMD_RESET_FAULT;
        uint8_t ch = 0;
        sscanf(p + 3, " %hhu", &ch);
        cmd.channel = (ch > 0) ? ch - 1 : 0xFF;  /* 0xFF = 전체 리셋 */
        osMessageQueuePut(xQueueUARTCmd, &cmd, 0, 0);
    } else if (strncasecmp(p, "GET", 3) == 0) {
        cmd.type = CMD_GET_STATUS;
        osMessageQueuePut(xQueueUARTCmd, &cmd, 0, 0);
    }
}

/* -------------------------------------------------------------------------
 * 공개 API 구현
 * ------------------------------------------------------------------------- */

void UART_Init(void)
{
    s_rx_prev_pos = 0;
    s_rx_line_len = 0;
    s_tx_busy     = 0;
    memset(s_rx_dma_buf, 0, sizeof(s_rx_dma_buf));

    /* DMA 원형 버퍼 수신 시작 (IDLE 이벤트 활성화) */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_rx_dma_buf, sizeof(s_rx_dma_buf));

    /* DMA 반전 인터럽트 비활성화 (Circular 모드에서 Half-Transfer 방지) */
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
}

void UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    if (huart->Instance != USART1) return;

    uint16_t pos = size;                /* 현재 DMA write pointer */
    uint16_t prev = s_rx_prev_pos;

    /* 수신된 데이터를 라인 버퍼에 복사 (wrap-around 처리) */
    uint16_t new_bytes;
    if (pos >= prev) {
        new_bytes = pos - prev;
    } else {
        new_bytes = UART_RX_BUF_SIZE - prev + pos;
    }

    for (uint16_t i = 0; i < new_bytes; i++) {
        uint8_t c = s_rx_dma_buf[(prev + i) % UART_RX_BUF_SIZE];

        if (c == '\r' || c == '\n') {
            if (s_rx_line_len > 0) {
                s_rx_line_buf[s_rx_line_len] = '\0';
                _parse_and_enqueue((char *)s_rx_line_buf);
                s_rx_line_len = 0;
            }
        } else if (s_rx_line_len < UART_MAX_CMD_LEN - 1) {
            s_rx_line_buf[s_rx_line_len++] = c;
        }
    }

    s_rx_prev_pos = pos;

    /* DMA 재시작 (HAL_UARTEx_RxEventCallback 내에서 DMA가 멈춘 경우) */
    if (HAL_UART_GetState(&huart1) == HAL_UART_STATE_READY) {
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_rx_dma_buf, sizeof(s_rx_dma_buf));
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }
}

void UART_RxProcess(void)
{
    /* UARTRxTask에서 명령 처리 – 실제 파싱은 인터럽트 콜백에서 수행
     * 여기서는 추가 처리 없음 (osDelay로 CPU 양보) */
    osDelay(1);
}

void UART_SendString(const char *str)
{
    if (str == NULL) return;
    uint16_t len = (uint16_t)strlen(str);
    if (len == 0) return;

    /* DMA 송신 완료 대기 (최대 200ms) */
    uint32_t tick = osKernelGetTickCount();
    while (s_tx_busy && (osKernelGetTickCount() - tick < 200)) {
        osDelay(1);
    }

    if (len > UART_TX_BUF_SIZE) len = UART_TX_BUF_SIZE;
    memcpy(s_tx_buf, str, len);
    s_tx_busy = 1;
    HAL_UART_Transmit_DMA(&huart1, s_tx_buf, len);
}

void UART_Printf(const char *fmt, ...)
{
    static char buf[UART_TX_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        UART_SendString(buf);
    }
}

void UART_SendChannelStatus(void)
{
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
        if (osMutexAcquire(xMutexChannelData, 10) != osOK) continue;

        uint16_t target  = g_channels[ch].target_mv;
        uint16_t meas    = g_channels[ch].measured_mv;
        int16_t  curr    = g_channels[ch].current_ma;
        FaultStatus_t flt = g_channels[ch].fault;
        uint8_t  ena     = g_channels[ch].enabled;

        osMutexRelease(xMutexChannelData);

        const char *fault_str = "NONE";
        if      (flt & FAULT_SHORT) fault_str = "SHORT";
        else if (flt & FAULT_OPEN)  fault_str = "OPEN";

        UART_Printf(">CH%u TARGET=%umV MEAS=%umV CURR=%dmA FAULT=%s EN=%u\r\n",
                    ch + 1, target, meas, curr, fault_str, ena);
    }
    UART_SendString("\r\n");
}

/* -------------------------------------------------------------------------
 * HAL 콜백 재정의 (uart_comm.c에서 관리)
 * ------------------------------------------------------------------------- */

/* HAL_UARTEx_RxEventCallback은 main.c 또는 여기에 구현 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    UART_RxEventCallback(huart, Size);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        s_tx_busy = 0;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        s_tx_busy = 0;
        /* 오류 복구: DMA 재시작 */
        HAL_UART_DMAStop(&huart1);
        s_rx_prev_pos = 0;
        s_rx_line_len = 0;
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_rx_dma_buf, sizeof(s_rx_dma_buf));
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }
}
