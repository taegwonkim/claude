/**
 ******************************************************************************
 * @file    uart_gpdma.c
 * @brief   USART3 + GPDMA1 Channel 0 비동기 TX 드라이버 구현
 *
 *  SD 카드 IDMA 와 충돌하지 않는 구조:
 *
 *    ┌─────────────────┐         ┌─────────────────┐
 *    │  SDMMC1 IDMA    │         │  GPDMA1 CH0     │
 *    │  (내장 DMA)      │         │  USART3 TX      │
 *    │  버스: AHB      │         │  버스: AHB      │
 *    └────────┬────────┘         └────────┬────────┘
 *             │ SD DMA 버퍼              │ UART TX 버퍼
 *             │ (독립 SRAM 영역)         │ (독립 SRAM 영역)
 *             └──────────┬──────────────┘
 *                        │ AHB Arbiter (충돌 없이 중재)
 *                        ▼
 *                    SRAM1 (AHB)
 *
 *  두 DMA 가 동시에 다른 메모리 영역에 접근해도 AHB arbiter 가 중재.
 *  동일한 메모리 영역에 동시에 쓰는 것만 피하면 됨 (버퍼 분리).
 ******************************************************************************
 */
#include "uart_gpdma.h"
#include <string.h>
#include <stdio.h>

/* ── HAL 핸들 정의 ──────────────────────────────────────────────────────── */
UART_HandleTypeDef huart3;
DMA_HandleTypeDef  hdma_usart3_tx;

/* ── 내부 DMA 전송 버퍼 ─────────────────────────────────────────────────── */
/*
 * 반드시 32바이트 정렬:
 *   D-Cache 라인(32B) 경계에 맞춰야 SCB_CleanDCache_by_Addr 이
 *   인접 변수를 오염시키지 않음.
 *
 * .DMABufferSection 에 배치:
 *   링커 스크립트에서 이 섹션을 SRAM 으로 매핑.
 */
static uint8_t s_tx_buf[UART_TX_BUF_SIZE]
    __attribute__((aligned(UART_DCACHE_LINE_SZ), section(".DMABufferSection")));

/* ── 완료 플래그 (ISR ↔ 폴링 동기화) ─────────────────────────────────── */
static volatile uint8_t s_tx_done = 1U;   /* 초기: 전송 없음(완료 상태) */

/* ── HAL UART TX DMA 완료 콜백 ─────────────────────────────────────────── */

/**
 * @brief  GPDMA1 CH0 전송 완료 시 HAL 이 호출하는 약한 심볼 재정의
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
        s_tx_done = 1U;
}

/* ── 공개 API 구현 ──────────────────────────────────────────────────────── */

void UART_GPDMA_Init(void)
{
    /* ── 1. 클럭 활성화 ─────────────────────────────────────────────── */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPDMA1_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* ── 2. GPIO 핀 설정 (PD8=TX, PD9=RX) ──────────────────────────── */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* ── 3. GPDMA1 Channel 0 설정 (USART3 TX) ──────────────────────── */
    hdma_usart3_tx.Instance                   = GPDMA1_Channel0;
    hdma_usart3_tx.Init.Request               = GPDMA1_REQUEST_USART3_TX;
    hdma_usart3_tx.Init.BlkHWRequest          = DMA_BREQ_SINGLE_BURST;
    hdma_usart3_tx.Init.Direction             = DMA_MEMORY_TO_PERIPH;
    hdma_usart3_tx.Init.SrcInc               = DMA_SINC_INCREMENTED;
    hdma_usart3_tx.Init.DestInc              = DMA_DINC_FIXED;
    hdma_usart3_tx.Init.SrcDataWidth         = DMA_SRC_DATAWIDTH_BYTE;
    hdma_usart3_tx.Init.DestDataWidth        = DMA_DEST_DATAWIDTH_BYTE;
    hdma_usart3_tx.Init.Priority             = DMA_LOW_PRIORITY_LOW_WEIGHT;
    hdma_usart3_tx.Init.SrcBurstLength       = 1U;
    hdma_usart3_tx.Init.DestBurstLength      = 1U;
    hdma_usart3_tx.Init.TransferAllocatedPort =
        DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
    hdma_usart3_tx.Init.TransferEventMode    = DMA_TCEM_BLOCK_TRANSFER;
    hdma_usart3_tx.Init.Mode                 = DMA_NORMAL;

    if (HAL_DMA_Init(&hdma_usart3_tx) != HAL_OK)
    {
        /* 초기화 실패 – 루프 (Error_Handler 가 정의된 경우 호출 가능) */
        while (1) {}
    }

    /* ── 4. DMA 핸들 ↔ UART TX 링크 ──────────────────────────────────── */
    __HAL_LINKDMA(&huart3, hdmatx, hdma_usart3_tx);

    /* ── 5. USART3 핸들 설정 ────────────────────────────────────────── */
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200U;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK)
        while (1) {}

    /* ── 6. NVIC 설정 ───────────────────────────────────────────────── */
    /* GPDMA1 Channel 0 IRQ (TX 완료/오류 처리) */
    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);

    /* USART3 IRQ (오류 처리용, TX 에는 불필요하지만 HAL 요구) */
    HAL_NVIC_SetPriority(USART3_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

uint8_t UART_WaitTxDone(void)
{
    uint32_t t0 = HAL_GetTick();
    while (!s_tx_done)
    {
        if ((HAL_GetTick() - t0) >= UART_TX_TIMEOUT_MS)
            return 1U;  /* 타임아웃 */
    }
    return 0U;  /* 정상 완료 */
}

void UART_SendStr(const char *str)
{
    if (str == NULL)
        return;

    uint16_t len = (uint16_t)strlen(str);
    if (len == 0U)
        return;

    /* 이전 전송 완료 대기 */
    UART_WaitTxDone();

    /* 내부 DMA 버퍼에 복사 (전송 중 원본 버퍼가 변경되어도 안전) */
    if (len >= UART_TX_BUF_SIZE)
        len = UART_TX_BUF_SIZE - 1U;

    memcpy(s_tx_buf, str, len);

    /* ── D-Cache 클린: CPU 기록 내용을 GPDMA 가 읽기 전 메모리로 반영 */
    SCB_CleanDCache_by_Addr((uint32_t *)s_tx_buf,
                            (int32_t)((len + UART_DCACHE_LINE_SZ - 1U)
                                      & ~(UART_DCACHE_LINE_SZ - 1U)));

    /* 비동기 GPDMA TX 시작 */
    s_tx_done = 0U;
    if (HAL_UART_Transmit_DMA(&huart3, s_tx_buf, len) != HAL_OK)
        s_tx_done = 1U;  /* 오류 시 완료 상태로 복구 */
}

void UART_Printf(const char *fmt, ...)
{
    char tmp[UART_TX_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    UART_SendStr(tmp);
}

void UART_GPDMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart3_tx);
}
