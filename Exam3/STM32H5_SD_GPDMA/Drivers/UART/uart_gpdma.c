/**
 ******************************************************************************
 * @file    uart_gpdma.c
 * @brief   USART3 + GPDMA1 Channel 0 비동기 TX – 링 버퍼 구현
 *
 *  링 버퍼 구조 및 GPDMA 청크 전송:
 *
 *   s_ring[]  (UART_RING_SIZE = 1024 B)
 *   ┌──────────────────────────────────────────────┐
 *   │ [r_head ... 전송완료 ... r_tail ... 누적 중] │
 *   └──────────────────────────────────────────────┘
 *            r_head                 r_tail
 *              ↓                      ↓
 *   UART_Printf() → s_ring에 쓰기 (r_tail 전진)
 *   TxCpltCallback → 남은 데이터 있으면 HAL_UART_Transmit_DMA 재호출
 *
 *  이점:
 *    SD 전송이 길어도 UART 메시지를 버리지 않고 링 버퍼에 누적.
 *    전송 완료 콜백에서 자동으로 다음 청크를 시작.
 *
 *  주의:
 *    s_ring 과 s_dma_buf 는 .dma_buf 섹션(SRAM2 Non-Cacheable) 에 배치.
 *    MPU 덕분에 SCB_CleanDCache 불필요.
 ******************************************************************************
 */
#include "uart_gpdma.h"
#include <string.h>
#include <stdio.h>

/* ── HAL 핸들 정의 ──────────────────────────────────────────────────────── */
UART_HandleTypeDef huart3;
DMA_HandleTypeDef  hdma_usart3_tx;

/* ── DMA 전용 버퍼 (.dma_buf 섹션 = SRAM2 Non-Cacheable) ──────────────── */
/*
 * s_dma_buf: GPDMA가 실제로 읽어가는 선형 전송 버퍼.
 *            링 버퍼에서 한 청크씩 복사 후 DMA 시작.
 * s_ring:    애플리케이션이 쓰는 순환 큐.
 */
static uint8_t s_dma_buf[UART_DMA_CHUNK]
    __attribute__((aligned(32), section(".dma_buf")));

static uint8_t s_ring[UART_RING_SIZE]
    __attribute__((aligned(32), section(".dma_buf")));

/* ── 링 버퍼 인덱스 및 상태 ─────────────────────────────────────────────── */
static volatile uint32_t s_head;       /* 전송 완료된 위치 (GPDMA가 읽어간 곳) */
static volatile uint32_t s_tail;       /* 다음 쓰기 위치 (UART_Printf가 채우는 곳) */
static volatile uint8_t  s_dma_busy;  /* 1 = GPDMA 전송 진행 중 */

/* ── 내부 헬퍼 ──────────────────────────────────────────────────────────── */

/** 링 버퍼에 데이터 추가 (tail 전진) – 오버플로 시 버림 */
static void RingPush(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0U; i < len; i++)
    {
        uint32_t next = (s_tail + 1U) % UART_RING_SIZE;
        if (next == s_head)
            break;                   /* 링 버퍼 풀 → 버림 */
        s_ring[s_tail] = data[i];
        s_tail = next;
    }
}

/** 링 버퍼에서 최대 UART_DMA_CHUNK 바이트를 s_dma_buf 로 복사하고 DMA 시작 */
static void StartNextChunk(void)
{
    if (s_head == s_tail)
    {
        s_dma_busy = 0U;            /* 버퍼 비어 있음 → 전송 없음 */
        return;
    }

    /* 연속 복사 가능한 바이트 수 계산 (wrap-around 고려) */
    uint32_t avail;
    if (s_tail > s_head)
        avail = s_tail - s_head;
    else
        avail = UART_RING_SIZE - s_head;   /* head 부터 끝까지 */

    uint32_t chunk = (avail > UART_DMA_CHUNK) ? UART_DMA_CHUNK : avail;

    memcpy(s_dma_buf, &s_ring[s_head], chunk);
    s_head = (s_head + chunk) % UART_RING_SIZE;

    s_dma_busy = 1U;
    if (HAL_UART_Transmit_DMA(&huart3, s_dma_buf, (uint16_t)chunk) != HAL_OK)
        s_dma_busy = 0U;            /* DMA 시작 실패 시 busy 해제 */
}

/* ── HAL UART TX 완료 콜백 ──────────────────────────────────────────────── */

/**
 * @brief  GPDMA1 CH0 전송 완료 → 다음 청크 자동 시작
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
        StartNextChunk();           /* 링 버퍼에 남은 데이터 있으면 계속 전송 */
}

/* ── 공개 API 구현 ──────────────────────────────────────────────────────── */

void UART_GPDMA_Init(void)
{
    /* ── 1. 클럭 활성화 ─────────────────────────────────────────────── */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPDMA1_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* ── 2. GPIO 핀 설정 (PD8=TX, PD9=RX, AF7) ──────────────────────── */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* ── 3. GPDMA1 Channel 0 – USART3 TX ────────────────────────────── */
    hdma_usart3_tx.Instance                    = GPDMA1_Channel0;
    hdma_usart3_tx.Init.Request                = GPDMA1_REQUEST_USART3_TX;
    hdma_usart3_tx.Init.BlkHWRequest           = DMA_BREQ_SINGLE_BURST;
    hdma_usart3_tx.Init.Direction              = DMA_MEMORY_TO_PERIPH;
    hdma_usart3_tx.Init.SrcInc                = DMA_SINC_INCREMENTED;
    hdma_usart3_tx.Init.DestInc               = DMA_DINC_FIXED;
    hdma_usart3_tx.Init.SrcDataWidth          = DMA_SRC_DATAWIDTH_BYTE;
    hdma_usart3_tx.Init.DestDataWidth         = DMA_DEST_DATAWIDTH_BYTE;
    hdma_usart3_tx.Init.Priority              = DMA_LOW_PRIORITY_LOW_WEIGHT;
    hdma_usart3_tx.Init.SrcBurstLength        = 1U;
    hdma_usart3_tx.Init.DestBurstLength       = 1U;
    hdma_usart3_tx.Init.TransferAllocatedPort =
        DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
    hdma_usart3_tx.Init.TransferEventMode     = DMA_TCEM_BLOCK_TRANSFER;
    hdma_usart3_tx.Init.Mode                  = DMA_NORMAL;

    if (HAL_DMA_Init(&hdma_usart3_tx) != HAL_OK)
        while (1) {}

    /* ── 4. DMA ↔ UART TX 핸들 연결 ─────────────────────────────────── */
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

    /* ── 6. NVIC ─────────────────────────────────────────────────────── */
    /* GPDMA1 CH0 (TX DMA 완료/오류 처리) */
    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);

    /* USART3 (UART 오류 처리) */
    HAL_NVIC_SetPriority(USART3_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(USART3_IRQn);

    /* ── 7. 링 버퍼 초기화 ──────────────────────────────────────────── */
    s_head     = 0U;
    s_tail     = 0U;
    s_dma_busy = 0U;
}

void UART_SendStr(const char *str)
{
    if (str == NULL)
        return;

    uint32_t len = (uint32_t)strlen(str);
    if (len == 0U)
        return;

    /* 인터럽트 잠금 없이 접근하는 경우 head/tail 레이스 가능.
     * 베어메탈 단일 코어에서는 tail 쓰기가 word 단위이므로 안전.
     * 필요 시 __disable_irq() / __enable_irq() 감쌀 것.          */
    RingPush((const uint8_t *)str, len);

    /* DMA 가 놀고 있으면 즉시 전송 시작 */
    if (!s_dma_busy)
        StartNextChunk();
}

void UART_Printf(const char *fmt, ...)
{
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    UART_SendStr(tmp);
}

uint8_t UART_Flush(void)
{
    uint32_t t0 = HAL_GetTick();
    while (s_dma_busy || (s_head != s_tail))
    {
        if ((HAL_GetTick() - t0) >= UART_TX_TIMEOUT_MS)
            return 1U;   /* 타임아웃 */
    }
    return 0U;
}

void UART_GPDMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart3_tx);
}
