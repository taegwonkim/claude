/**
 ******************************************************************************
 * @file    sd_driver.c
 * @brief   STM32H5 SDMMC1 IDMA 드라이버 구현
 *
 *  SDMMC1 IDMA 동작 흐름:
 *
 *    [읽기]
 *    SD_ReadBlocks()
 *      → HAL_SD_ReadBlocks_DMA()   ← SDMMC 내장 IDMA 시작
 *        → SDMMC1_IRQHandler()     ← 전송 완료/오류 인터럽트
 *          → HAL_SD_RxCpltCallback() ← g_rxDone = 1
 *      → WaitFlag(&g_rxDone)       ← 폴링 대기
 *      → WaitReady()               ← 카드 프로그램 완료 대기
 *
 *    [쓰기]
 *    SD_WriteBlocks()
 *      → HAL_SD_WriteBlocks_DMA()
 *        → SDMMC1_IRQHandler()
 *          → HAL_SD_TxCpltCallback() ← g_txDone = 1
 *      → WaitFlag(&g_txDone)
 *      → WaitReady()
 *
 *  D-Cache 문제 해결:
 *    buf 가 MPU Non-Cacheable 영역 (.dma_buf, SRAM2)에 있으므로
 *    SCB_CleanDCache / SCB_InvalidateDCache 호출 불필요.
 *
 *  UART GPDMA 와의 공존:
 *    SDMMC IDMA : SDMMC1 전용 AHB 마스터 (GPDMA와 별개)
 *    UART GPDMA : GPDMA1 Channel 0
 *    → 두 전송이 동시에 달라도 AHB arbiter 가 중재, 충돌 없음.
 ******************************************************************************
 */
#include "sd_driver.h"

/* ── HAL SD 핸들 정의 ───────────────────────────────────────────────────── */
SD_HandleTypeDef hsd1;

/* ── 완료 플래그 (ISR ↔ 메인 루프) ─────────────────────────────────────── */
static volatile uint8_t g_rxDone;
static volatile uint8_t g_txDone;
static volatile uint8_t g_sdErr;

/* ── HAL 콜백 ───────────────────────────────────────────────────────────── */

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    g_rxDone = 1U;
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    g_txDone = 1U;
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    g_sdErr = 1U;
}

/* ── 내부 헬퍼 ──────────────────────────────────────────────────────────── */

/** 카드가 TRANSFER 상태가 될 때까지 대기 */
static SD_Status WaitReady(void)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
    {
        if ((HAL_GetTick() - t0) >= SD_TIMEOUT_MS)
            return SD_ERR_BUSY;
    }
    return SD_OK;
}

/** DMA 완료 플래그 폴링 대기 */
static SD_Status WaitFlag(volatile uint8_t *flag, SD_Status err_code)
{
    uint32_t t0 = HAL_GetTick();
    while (!(*flag) && !g_sdErr)
    {
        if ((HAL_GetTick() - t0) >= SD_TIMEOUT_MS)
            return SD_ERR_BUSY;
    }
    return g_sdErr ? err_code : SD_OK;
}

/* ── 공개 API 구현 ──────────────────────────────────────────────────────── */

SD_Status SD_Init(void)
{
    /* ── GPIO 클럭 활성화 ───────────────────────────────────────────── */
    __HAL_RCC_SDMMC1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* ── 핀 설정 ────────────────────────────────────────────────────── */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_SDMMC1;

    /* PC8~PC12 (D0~D3, CK) */
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10
             | GPIO_PIN_11 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* PD2 (CMD) */
    gpio.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* ── SDMMC1 핸들 – 초기화 속도 (DS, ClockDiv=4 → 12.5 MHz) ────── */
    hsd1.Instance                 = SDMMC1;
    hsd1.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide             = SDMMC_BUS_WIDE_1B;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv            = 4U;   /* PLL1Q(100MHz)/4 = 12.5 MHz */

    if (HAL_SD_Init(&hsd1) != HAL_OK)
        return SD_ERR_INIT;

    /* ── 4-bit 와이드 버스 전환 ─────────────────────────────────────── */
    if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) != HAL_OK)
        return SD_ERR_WIDE;

    /* ── SDMMC1 NVIC 활성화 ─────────────────────────────────────────── */
    HAL_NVIC_SetPriority(SDMMC1_IRQn, 4U, 0U);
    HAL_NVIC_EnableIRQ(SDMMC1_IRQn);

    return SD_OK;
}

SD_Status SD_SetHighSpeed(void)
{
    /* HS 모드: ClockDiv=2 → PLL1Q(100MHz)/2 = 25 MHz (SD HS 최대값) */
    if (HAL_SD_ConfigSpeedBusOperation(&hsd1, SDMMC_SPEED_MODE_HIGH) != HAL_OK)
        return SD_ERR_BUSY;
    return SD_OK;
}

SD_Status SD_GetInfo(SD_CardInfo *info)
{
    if (info == NULL)
        return SD_ERR_PARAM;

    HAL_SD_CardInfoTypeDef ci;
    if (HAL_SD_GetCardInfo(&hsd1, &ci) != HAL_OK)
        return SD_ERR_BUSY;

    info->block_count  = ci.LogBlockNbr;
    info->block_size   = ci.LogBlockSize;
    info->capacity_mb  = ((uint64_t)ci.LogBlockNbr * ci.LogBlockSize)
                         / (1024UL * 1024UL);
    info->card_type    = (uint8_t)ci.CardType;
    return SD_OK;
}

SD_Status SD_ReadBlocks(uint8_t *buf, uint32_t sector, uint32_t count)
{
    if (buf == NULL || count == 0U)
        return SD_ERR_PARAM;

    SD_Status st = WaitReady();
    if (st != SD_OK)
        return st;

    g_rxDone = 0U;
    g_sdErr  = 0U;

    if (HAL_SD_ReadBlocks_DMA(&hsd1, buf, sector, count) != HAL_OK)
        return SD_ERR_RX;

    st = WaitFlag(&g_rxDone, SD_ERR_RX);
    if (st != SD_OK)
        return st;

    return WaitReady();
}

SD_Status SD_WriteBlocks(const uint8_t *buf, uint32_t sector, uint32_t count)
{
    if (buf == NULL || count == 0U)
        return SD_ERR_PARAM;

    SD_Status st = WaitReady();
    if (st != SD_OK)
        return st;

    g_txDone = 0U;
    g_sdErr  = 0U;

    /* HAL API 가 uint8_t* 를 요구하므로 const 캐스팅 필요.
     * buf 내용은 IDMA 가 읽기만 하므로 실제 수정 없음.            */
    if (HAL_SD_WriteBlocks_DMA(&hsd1, (uint8_t *)buf, sector, count) != HAL_OK)
        return SD_ERR_TX;

    st = WaitFlag(&g_txDone, SD_ERR_TX);
    if (st != SD_OK)
        return st;

    return WaitReady();
}

uint8_t SD_IsReady(void)
{
    return (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) ? 1U : 0U;
}

void SD_IRQHandler(void)
{
    HAL_SD_IRQHandler(&hsd1);
}
