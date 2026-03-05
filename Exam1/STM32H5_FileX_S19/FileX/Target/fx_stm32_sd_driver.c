/**
 ******************************************************************************
 * @file    fx_stm32_sd_driver.c
 * @brief   FileX STM32H5 SDMMC1 DMA 미디어 드라이버
 *
 *  D-Cache 관리:
 *    읽기 전 SCB_InvalidateDCache_by_Addr  (DMA→메모리 반영)
 *    쓰기 전 SCB_CleanDCache_by_Addr       (메모리→DMA 플러시)
 *
 *  주의: HAL_SD_Rx/Tx/ErrorCallback 이 파일에서 유일하게 정의.
 ******************************************************************************
 */
#include "fx_stm32_sd_driver.h"
#include <string.h>

/* ── 내부 상수 / 버퍼 ──────────────────────────────────────────────────── */
#define DCACHE_LINE 32U

static UCHAR scratch[FX_SD_SECTOR_SIZE]
    __attribute__((aligned(DCACHE_LINE), section(".DMABufferSection")));

static volatile UINT g_rx_done, g_tx_done, g_sd_err;

/* ── HAL DMA 콜백 ──────────────────────────────────────────────────────── */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *h) { (void)h; g_rx_done = 1U; }
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *h) { (void)h; g_tx_done = 1U; }
void HAL_SD_ErrorCallback (SD_HandleTypeDef *h) { (void)h; g_sd_err  = 1U; }

/* ── 내부 헬퍼 ─────────────────────────────────────────────────────────── */
static HAL_StatusTypeDef WaitXfer(uint32_t ms)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
        if ((HAL_GetTick() - t0) >= ms) return HAL_TIMEOUT;
    return HAL_OK;
}

static UINT ReadSec(UCHAR *dst, ULONG sec)
{
    if (WaitXfer(FX_SD_TIMEOUT_MS) != HAL_OK) return FX_IO_ERROR;
    SCB_InvalidateDCache_by_Addr((uint32_t *)dst, FX_SD_SECTOR_SIZE);
    g_rx_done = g_sd_err = 0U;
    if (HAL_SD_ReadBlocks_DMA(&hsd1, dst, (uint32_t)sec, 1U) != HAL_OK)
        return FX_IO_ERROR;
    uint32_t t0 = HAL_GetTick();
    while (!g_rx_done && !g_sd_err)
        if ((HAL_GetTick() - t0) >= FX_SD_TIMEOUT_MS) return FX_IO_ERROR;
    if (g_sd_err) return FX_IO_ERROR;
    return (WaitXfer(FX_SD_TIMEOUT_MS) == HAL_OK) ? FX_SUCCESS : FX_IO_ERROR;
}

static UINT WriteSec(const UCHAR *src, ULONG sec)
{
    if (WaitXfer(FX_SD_TIMEOUT_MS) != HAL_OK) return FX_IO_ERROR;
    SCB_CleanDCache_by_Addr((uint32_t *)src, FX_SD_SECTOR_SIZE);
    g_tx_done = g_sd_err = 0U;
    if (HAL_SD_WriteBlocks_DMA(&hsd1, (UCHAR *)src, (uint32_t)sec, 1U) != HAL_OK)
        return FX_IO_ERROR;
    uint32_t t0 = HAL_GetTick();
    while (!g_tx_done && !g_sd_err)
        if ((HAL_GetTick() - t0) >= FX_SD_TIMEOUT_MS) return FX_IO_ERROR;
    if (g_sd_err) return FX_IO_ERROR;
    return (WaitXfer(FX_SD_TIMEOUT_MS) == HAL_OK) ? FX_SUCCESS : FX_IO_ERROR;
}

/* ── FileX 드라이버 진입점 ─────────────────────────────────────────────── */
VOID fx_stm32_sd_driver(FX_MEDIA *m)
{
    UCHAR *buf;  ULONG base, cnt;  UINT r = FX_SUCCESS;

    switch (m->fx_media_driver_request)
    {
    case FX_DRIVER_INIT:
        m->fx_media_driver_status =
            (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER)
            ? FX_SUCCESS : FX_IO_ERROR;
        return;
    case FX_DRIVER_UNINIT: case FX_DRIVER_FLUSH: case FX_DRIVER_ABORT:
        m->fx_media_driver_status = FX_SUCCESS; return;

    case FX_DRIVER_READ:
        buf  = (UCHAR *)m->fx_media_driver_buffer;
        base = m->fx_media_driver_logical_sector + m->fx_media_hidden_sectors;
        cnt  = m->fx_media_driver_sectors;
        for (ULONG i = 0; i < cnt && r == FX_SUCCESS; i++) {
            UCHAR *d = buf + i * FX_SD_SECTOR_SIZE;
            if ((uint32_t)d & (DCACHE_LINE-1U)) {
                r = ReadSec(scratch, base+i);
                if (r == FX_SUCCESS) memcpy(d, scratch, FX_SD_SECTOR_SIZE);
            } else r = ReadSec(d, base+i);
        }
        m->fx_media_driver_status = r; return;

    case FX_DRIVER_WRITE:
        buf  = (UCHAR *)m->fx_media_driver_buffer;
        base = m->fx_media_driver_logical_sector + m->fx_media_hidden_sectors;
        cnt  = m->fx_media_driver_sectors;
        for (ULONG i = 0; i < cnt && r == FX_SUCCESS; i++) {
            const UCHAR *s = buf + i * FX_SD_SECTOR_SIZE;
            if ((uint32_t)s & (DCACHE_LINE-1U)) {
                memcpy(scratch, s, FX_SD_SECTOR_SIZE);
                r = WriteSec(scratch, base+i);
            } else r = WriteSec(s, base+i);
        }
        m->fx_media_driver_status = r; return;

    case FX_DRIVER_BOOT_READ:
        r = ReadSec(scratch, 0U);
        if (r == FX_SUCCESS) memcpy(m->fx_media_driver_buffer, scratch, FX_SD_SECTOR_SIZE);
        m->fx_media_driver_status = r; return;
    case FX_DRIVER_BOOT_WRITE:
        memcpy(scratch, m->fx_media_driver_buffer, FX_SD_SECTOR_SIZE);
        m->fx_media_driver_status = WriteSec(scratch, 0U); return;

    default:
        m->fx_media_driver_status = FX_IO_ERROR; return;
    }
}
