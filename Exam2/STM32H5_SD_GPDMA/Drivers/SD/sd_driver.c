/**
 ******************************************************************************
 * @file    sd_driver.c
 * @brief   STM32H5 SDMMC1 IDMA 블록 읽기/쓰기 드라이버 구현
 *
 *  동작 원리:
 *    1. HAL_SD_ReadBlocks_DMA / HAL_SD_WriteBlocks_DMA 호출
 *       → SDMMC1 IDMA 가 AHB 버스를 통해 메모리↔SD 전송 수행
 *    2. 전송 완료 시 SDMMC1 인터럽트 → HAL 콜백 → g_rx_done / g_tx_done 세트
 *    3. 호출자는 플래그가 세트될 때까지 폴링 (타임아웃 보호)
 *
 *  D-Cache 정합:
 *    읽기 전 : SCB_InvalidateDCache_by_Addr  (D→메모리 방향: DMA 기록 후 CPU 읽기)
 *    쓰기 전 : SCB_CleanDCache_by_Addr       (메모리→D 방향: CPU 기록 후 DMA 읽기)
 *
 *  UART GPDMA 와의 충돌 방지:
 *    - SDMMC IDMA : SDMMC1 내장, AHB1 버스 마스터
 *    - UART GPDMA : GPDMA1 Channel 0, AHB1 버스 마스터
 *    - 두 DMA 가 동시에 다른 메모리 영역에 접근하더라도 AHB arbiter 가 중재.
 *    - 두 DMA 의 버퍼가 절대 겹치지 않아야 함 (각자 독립적 .DMABufferSection 사용).
 *    - SDMMC IRQ(preempt 4) > GPDMA IRQ(preempt 6) : SD 완료가 UART 완료보다 우선.
 ******************************************************************************
 */
#include "sd_driver.h"
#include <string.h>

/* ── 내부 플래그 (ISR ↔ 폴링 동기화) ─────────────────────────────────── */
static volatile uint8_t g_rx_done;   /* DMA 읽기 완료 */
static volatile uint8_t g_tx_done;   /* DMA 쓰기 완료 */
static volatile uint8_t g_sd_err;    /* DMA 오류      */

/* ── HAL SD 콜백 ─────────────────────────────────────────────────────── */

/**
 * @brief  HAL SD 수신 완료 콜백 (SDMMC1 인터럽트 컨텍스트)
 */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    g_rx_done = 1U;
}

/**
 * @brief  HAL SD 송신 완료 콜백 (SDMMC1 인터럽트 컨텍스트)
 */
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    g_tx_done = 1U;
}

/**
 * @brief  HAL SD 오류 콜백 (SDMMC1 인터럽트 컨텍스트)
 */
void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    g_sd_err = 1U;
}

/* ── 내부 헬퍼 ───────────────────────────────────────────────────────── */

/**
 * @brief  SD 카드가 TRANSFER 상태가 될 때까지 대기
 * @param  ms  최대 대기 시간 (밀리초)
 */
static SD_Status WaitReady(uint32_t ms)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
    {
        if ((HAL_GetTick() - t0) >= ms)
            return SD_ERR_BUSY;
    }
    return SD_OK;
}

/**
 * @brief  DMA 완료 플래그가 세트될 때까지 폴링 대기
 *
 * @param  done_flag  완료 플래그 포인터 (g_rx_done 또는 g_tx_done)
 * @param  ms         최대 대기 시간 (밀리초)
 */
static SD_Status WaitDMADone(volatile uint8_t *done_flag, uint32_t ms)
{
    uint32_t t0 = HAL_GetTick();
    while (!(*done_flag) && !g_sd_err)
    {
        if ((HAL_GetTick() - t0) >= ms)
            return SD_ERR_BUSY;
    }
    return g_sd_err ? SD_ERR_DMA : SD_OK;
}

/* ── 공개 API 구현 ───────────────────────────────────────────────────── */

SD_Status SD_Init(void)
{
    /* ── SDMMC1 클럭 및 GPIO 활성화 ───────────────────────────────── */
    __HAL_RCC_SDMMC1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* ── SDMMC1 핀 설정 (NUCLEO-H563ZI 기본 배선) ─────────────────── */
    GPIO_InitTypeDef gpio = {0};

    /* PC8:D0  PC9:D1  PC10:D2  PC11:D3  PC12:CK */
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10
                   | GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_SDMMC1;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* PD2:CMD */
    gpio.Pin       = GPIO_PIN_2;
    gpio.Pull      = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* ── HAL SD 핸들 설정 ──────────────────────────────────────────── */
    hsd1.Instance                 = SDMMC1;
    hsd1.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide             = SDMMC_BUS_WIDE_1B;         /* 초기 1-bit */
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv            = 4U;   /* PLL1Q(100MHz)/4 ≈ 12.5 MHz */

    if (HAL_SD_Init(&hsd1) != HAL_OK)
        return SD_ERR_BUSY;

    /* ── 4-bit 와이드 버스로 전환 ──────────────────────────────────── */
    if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) != HAL_OK)
        return SD_ERR_BUSY;

    /* ── SDMMC1 NVIC 설정 ──────────────────────────────────────────── */
    HAL_NVIC_SetPriority(SDMMC1_IRQn, 4U, 0U);
    HAL_NVIC_EnableIRQ(SDMMC1_IRQn);

    return SD_OK;
}

SD_Status SD_ReadBlocks(uint8_t *buf, uint32_t sector, uint32_t count)
{
    if (buf == NULL || count == 0U)
        return SD_ERR_PARAM;

    /* ── 1. 카드 TRANSFER 상태 대기 ─────────────────────────────────── */
    SD_Status st = WaitReady(SD_TIMEOUT_MS);
    if (st != SD_OK)
        return st;

    /* ── 2. 수신 버퍼 D-Cache 무효화 ────────────────────────────────── */
    /*      DMA 가 메모리에 기록한 새 데이터를 CPU 가 캐시 오염 없이 읽도록  */
    uint32_t byte_len = count * SD_SECTOR_SIZE;
    SCB_InvalidateDCache_by_Addr((uint32_t *)buf, (int32_t)byte_len);

    /* ── 3. DMA 전송 시작 ────────────────────────────────────────────── */
    g_rx_done = 0U;
    g_sd_err  = 0U;

    if (HAL_SD_ReadBlocks_DMA(&hsd1, buf, sector, count) != HAL_OK)
        return SD_ERR_DMA;

    /* ── 4. DMA 완료 대기 (인터럽트 콜백이 플래그 세트) ────────────── */
    st = WaitDMADone(&g_rx_done, SD_TIMEOUT_MS);
    if (st != SD_OK)
        return st;

    /* ── 5. 카드 내부 동작 완료 대기 ────────────────────────────────── */
    return WaitReady(SD_TIMEOUT_MS);
}

SD_Status SD_WriteBlocks(const uint8_t *buf, uint32_t sector, uint32_t count)
{
    if (buf == NULL || count == 0U)
        return SD_ERR_PARAM;

    /* ── 1. 카드 TRANSFER 상태 대기 ─────────────────────────────────── */
    SD_Status st = WaitReady(SD_TIMEOUT_MS);
    if (st != SD_OK)
        return st;

    /* ── 2. 송신 버퍼 D-Cache 클린 (CPU→DMA) ────────────────────────── */
    /*      CPU 가 캐시에만 기록한 데이터를 DMA 가 읽기 전 메모리로 플러시  */
    uint32_t byte_len = count * SD_SECTOR_SIZE;
    SCB_CleanDCache_by_Addr((uint32_t *)buf, (int32_t)byte_len);

    /* ── 3. DMA 전송 시작 ────────────────────────────────────────────── */
    g_tx_done = 0U;
    g_sd_err  = 0U;

    /* HAL 에서 const 캐스팅 필요 (HAL API 가 uint8_t* 요구) */
    if (HAL_SD_WriteBlocks_DMA(&hsd1, (uint8_t *)buf, sector, count) != HAL_OK)
        return SD_ERR_DMA;

    /* ── 4. DMA 완료 대기 ────────────────────────────────────────────── */
    st = WaitDMADone(&g_tx_done, SD_TIMEOUT_MS);
    if (st != SD_OK)
        return st;

    /* ── 5. 카드 내부 프로그램 완료 대기 ────────────────────────────── */
    return WaitReady(SD_TIMEOUT_MS);
}

uint8_t SD_IsReady(void)
{
    return (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) ? 1U : 0U;
}

void SD_IRQHandler(void)
{
    HAL_SD_IRQHandler(&hsd1);
}
