/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    fx_stm32_sd_driver.c
  * @brief   FileX ↔ STM32H5 SDMMC1(GPDMA1) 드라이버 구현
  *
  * 동작 원리:
  *  1. FileX가 fx_stm32_sd_driver()를 호출하여 읽기/쓰기/초기화를 요청
  *  2. 드라이버는 HAL_SD_ReadBlocks_DMA() / HAL_SD_WriteBlocks_DMA() 호출
  *  3. GPDMA1이 SDMMC1 ↔ 메모리 간 데이터 이동 (CPU 개입 없음)
  *  4. DMA 완료 인터럽트 → 콜백 → g_sd_rx/tx_state 플래그 갱신
  *  5. 드라이버는 플래그를 폴링하여 완료 확인 (RTOS 없음)
  *
  * DMA 버퍼 정렬:
  *  STM32H5 GPDMA는 Word(4바이트) 단위 전송.
  *  HAL_SD_ReadBlocks_DMA()에 전달되는 버퍼는 반드시 4바이트 정렬 필요.
  *  FileX의 내부 캐시 버퍼(fx_media_driver_buffer)가 정렬되지 않을 수 있으므로
  *  정렬된 임시 버퍼(sd_dma_buf)를 사용하고 memcpy로 복사.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "fx_stm32_sd_driver.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 *  전역 상태 변수 (DMA 콜백과 드라이버 폴링 루프 공유)
 * ============================================================================ */
volatile FX_SD_TransferState_t g_sd_rx_state = FX_SD_TRANSFER_IDLE;
volatile FX_SD_TransferState_t g_sd_tx_state = FX_SD_TRANSFER_IDLE;

/*
 * DMA 정렬 버퍼
 * 섹터 크기(512B) × 최대 연속 전송 섹터 수(최대 8개 = 4096B)
 * __attribute__((aligned(4))): GPDMA Word 전송 정렬 보장
 */
#define SD_DMA_BUF_SECTORS   8U
static uint8_t sd_dma_buf[FX_SD_SECTOR_SIZE * SD_DMA_BUF_SECTORS]
    __attribute__((aligned(FX_SD_DMA_BUFFER_ALIGN)));

/* ============================================================================
 *  내부 헬퍼 함수
 * ============================================================================ */

/**
 * @brief SD 카드가 전송 준비될 때까지 대기
 * @param timeout_ms  최대 대기 시간 (ms)
 * @retval HAL_OK / HAL_TIMEOUT
 */
static HAL_StatusTypeDef SD_WaitReady(uint32_t timeout_ms)
{
    uint32_t tick_start = HAL_GetTick();

    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
    {
        if ((HAL_GetTick() - tick_start) >= timeout_ms)
        {
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

/**
 * @brief DMA RX 완료 대기 (폴링, RTOS 없음)
 * @param timeout_ms  최대 대기 시간 (ms)
 * @retval FX_SUCCESS / FX_IO_ERROR / FX_NOT_IMPLEMENTED (timeout)
 */
static UINT SD_WaitRxDone(uint32_t timeout_ms)
{
    uint32_t tick_start = HAL_GetTick();

    while (g_sd_rx_state == FX_SD_TRANSFER_ONGOING)
    {
        if ((HAL_GetTick() - tick_start) >= timeout_ms)
        {
            return FX_NOT_IMPLEMENTED; /* 타임아웃 → IO 에러로 처리 */
        }
    }

    if (g_sd_rx_state == FX_SD_TRANSFER_ERROR)
    {
        g_sd_rx_state = FX_SD_TRANSFER_IDLE;
        return FX_IO_ERROR;
    }

    g_sd_rx_state = FX_SD_TRANSFER_IDLE;
    return FX_SUCCESS;
}

/**
 * @brief DMA TX 완료 대기 (폴링, RTOS 없음)
 * @param timeout_ms  최대 대기 시간 (ms)
 * @retval FX_SUCCESS / FX_IO_ERROR / FX_NOT_IMPLEMENTED (timeout)
 */
static UINT SD_WaitTxDone(uint32_t timeout_ms)
{
    uint32_t tick_start = HAL_GetTick();

    while (g_sd_tx_state == FX_SD_TRANSFER_ONGOING)
    {
        if ((HAL_GetTick() - tick_start) >= timeout_ms)
        {
            return FX_NOT_IMPLEMENTED;
        }
    }

    if (g_sd_tx_state == FX_SD_TRANSFER_ERROR)
    {
        g_sd_tx_state = FX_SD_TRANSFER_IDLE;
        return FX_IO_ERROR;
    }

    g_sd_tx_state = FX_SD_TRANSFER_IDLE;
    return FX_SUCCESS;
}

/* ============================================================================
 *  HAL SD 콜백 함수
 *  stm32h5xx_hal_sd.c에 weak로 정의된 콜백을 여기서 오버라이드
 * ============================================================================ */

/**
 * @brief SDMMC1 DMA RX 완료 콜백
 *        GPDMA1 CH1 인터럽트 → HAL_DMA_IRQHandler → 이 콜백
 */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1)
    {
        g_sd_rx_state = FX_SD_TRANSFER_IDLE;
    }
}

/**
 * @brief SDMMC1 DMA TX 완료 콜백
 *        GPDMA1 CH0 인터럽트 → HAL_DMA_IRQHandler → 이 콜백
 */
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1)
    {
        g_sd_tx_state = FX_SD_TRANSFER_IDLE;
    }
}

/**
 * @brief SDMMC1 에러 콜백 (DMA 에러, CRC 에러 등)
 */
void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1)
    {
        /* 진행 중인 RX/TX 모두 에러 처리 */
        if (g_sd_rx_state == FX_SD_TRANSFER_ONGOING)
        {
            g_sd_rx_state = FX_SD_TRANSFER_ERROR;
        }
        if (g_sd_tx_state == FX_SD_TRANSFER_ONGOING)
        {
            g_sd_tx_state = FX_SD_TRANSFER_ERROR;
        }
        printf("[SD ERROR] HAL_SD_ErrorCallback, ErrCode=0x%08lX\r\n",
               hsd->ErrorCode);
    }
}

/* ============================================================================
 *  fx_stm32_sd_driver
 *  FileX 미디어 드라이버 진입점
 * ============================================================================ */

/**
 * @brief FileX SD 카드 드라이버
 *
 *  FileX는 아래 요청(FX_MEDIA::fx_media_driver_request)을 발생시킨다:
 *    FX_DRIVER_INIT          : 드라이버/하드웨어 초기화
 *    FX_DRIVER_UNINIT        : 드라이버 해제
 *    FX_DRIVER_READ          : 섹터 읽기 (DMA)
 *    FX_DRIVER_WRITE         : 섹터 쓰기 (DMA)
 *    FX_DRIVER_FLUSH         : 캐시/버퍼 플러시
 *    FX_DRIVER_ABORT         : 현재 전송 중단
 *    FX_DRIVER_BOOT_READ     : 부트 섹터 읽기 (= 섹터 0 읽기)
 *    FX_DRIVER_BOOT_WRITE    : 부트 섹터 쓰기
 *    FX_DRIVER_GET_SECTOR_SIZE: 섹터 크기 반환
 *
 * @param media_ptr  FileX 미디어 제어 블록
 */
VOID fx_stm32_sd_driver(FX_MEDIA *media_ptr)
{
    UINT status = FX_SUCCESS;

    switch (media_ptr->fx_media_driver_request)
    {
    /* ------------------------------------------------------------------ */
    case FX_DRIVER_INIT:
    /* ------------------------------------------------------------------ */
    {
        /*
         * SDMMC1 초기화는 main.c의 MX_SDMMC1_SD_Init()에서 이미 완료됨.
         * 여기서는 FileX에 드라이버 파라미터만 설정.
         */
        SD_CardInfoTypeDef card_info = {0};

        if (HAL_SD_GetCardInfo(&hsd1, &card_info) != HAL_OK)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
            return;
        }

        /* 카드 상태 확인 */
        if (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
            return;
        }

        /* FileX에 섹터 크기 알림 */
        media_ptr->fx_media_driver_free_sector_update = FX_FALSE;

        printf("[SD] Card initialized: %lu MB\r\n",
               (uint32_t)(card_info.BlockNbr / 2048UL));

        media_ptr->fx_media_driver_status = FX_SUCCESS;
        break;
    }

    /* ------------------------------------------------------------------ */
    case FX_DRIVER_UNINIT:
    /* ------------------------------------------------------------------ */
        HAL_SD_DeInit(&hsd1);
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        break;

    /* ------------------------------------------------------------------ */
    case FX_DRIVER_GET_SECTOR_SIZE:
    /* ------------------------------------------------------------------ */
        media_ptr->fx_media_driver_sectors      = 1U;
        media_ptr->fx_media_driver_status       = FX_SUCCESS;
        media_ptr->fx_media_bytes_per_sector    = FX_SD_SECTOR_SIZE;
        break;

    /* ------------------------------------------------------------------ */
    case FX_DRIVER_READ:
    case FX_DRIVER_BOOT_READ:
    /* ------------------------------------------------------------------ */
    {
        uint32_t sector  = media_ptr->fx_media_driver_logical_sector;
        uint32_t sectors = media_ptr->fx_media_driver_sectors;
        uint8_t *buf     = (uint8_t *)media_ptr->fx_media_driver_buffer;

        /*
         * GPDMA는 Word(4B) 정렬된 버퍼 필요.
         * FileX 내부 버퍼가 비정렬일 수 있으므로:
         *   - 요청 섹터 수 ≤ SD_DMA_BUF_SECTORS → 임시 정렬 버퍼 사용
         *   - 4B 정렬된 버퍼이면 직접 전달
         */
        uint8_t *dma_buf;
        uint8_t  use_tmp = 0U;

        if (((uint32_t)buf & (FX_SD_DMA_BUFFER_ALIGN - 1U)) != 0U ||
            sectors > SD_DMA_BUF_SECTORS)
        {
            if (sectors > SD_DMA_BUF_SECTORS)
            {
                /* 섹터 수가 너무 많으면 청크 단위로 분할 */
                uint32_t remaining = sectors;
                uint32_t cur_sector = sector;
                uint8_t *cur_buf = buf;
                status = FX_SUCCESS;

                while (remaining > 0U)
                {
                    uint32_t chunk = (remaining > SD_DMA_BUF_SECTORS)
                                     ? SD_DMA_BUF_SECTORS : remaining;

                    /* 청크 읽기 (재귀 대신 직접 HAL 호출) */
                    if (SD_WaitReady(FX_SD_READY_TIMEOUT_MS) != HAL_OK)
                    {
                        status = FX_IO_ERROR;
                        break;
                    }

                    g_sd_rx_state = FX_SD_TRANSFER_ONGOING;
                    if (HAL_SD_ReadBlocks_DMA(&hsd1, sd_dma_buf,
                                              cur_sector, chunk) != HAL_OK)
                    {
                        g_sd_rx_state = FX_SD_TRANSFER_IDLE;
                        status = FX_IO_ERROR;
                        break;
                    }

                    status = SD_WaitRxDone(FX_SD_TRANSFER_TIMEOUT_MS);
                    if (status != FX_SUCCESS) break;

                    memcpy(cur_buf, sd_dma_buf, chunk * FX_SD_SECTOR_SIZE);

                    remaining  -= chunk;
                    cur_sector += chunk;
                    cur_buf    += chunk * FX_SD_SECTOR_SIZE;
                }

                media_ptr->fx_media_driver_status = status;
                return;
            }

            dma_buf  = sd_dma_buf;
            use_tmp  = 1U;
        }
        else
        {
            dma_buf = buf;
        }

        /* SD 카드 준비 대기 */
        if (SD_WaitReady(FX_SD_READY_TIMEOUT_MS) != HAL_OK)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
            return;
        }

        /* DMA 읽기 시작 */
        g_sd_rx_state = FX_SD_TRANSFER_ONGOING;
        if (HAL_SD_ReadBlocks_DMA(&hsd1, dma_buf, sector, sectors) != HAL_OK)
        {
            g_sd_rx_state = FX_SD_TRANSFER_IDLE;
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
            return;
        }

        /* DMA 완료 대기 (폴링) */
        status = SD_WaitRxDone(FX_SD_TRANSFER_TIMEOUT_MS);

        if (status == FX_SUCCESS && use_tmp)
        {
            memcpy(buf, dma_buf, sectors * FX_SD_SECTOR_SIZE);
        }

        media_ptr->fx_media_driver_status = status;
        break;
    }

    /* ------------------------------------------------------------------ */
    case FX_DRIVER_WRITE:
    case FX_DRIVER_BOOT_WRITE:
    /* ------------------------------------------------------------------ */
    {
        uint32_t sector  = media_ptr->fx_media_driver_logical_sector;
        uint32_t sectors = media_ptr->fx_media_driver_sectors;
        uint8_t *buf     = (uint8_t *)media_ptr->fx_media_driver_buffer;

        uint8_t *dma_buf;
        uint8_t  use_tmp = 0U;

        if (((uint32_t)buf & (FX_SD_DMA_BUFFER_ALIGN - 1U)) != 0U ||
            sectors > SD_DMA_BUF_SECTORS)
        {
            if (sectors > SD_DMA_BUF_SECTORS)
            {
                /* 청크 단위 쓰기 */
                uint32_t remaining = sectors;
                uint32_t cur_sector = sector;
                uint8_t *cur_buf = buf;
                status = FX_SUCCESS;

                while (remaining > 0U)
                {
                    uint32_t chunk = (remaining > SD_DMA_BUF_SECTORS)
                                     ? SD_DMA_BUF_SECTORS : remaining;

                    memcpy(sd_dma_buf, cur_buf, chunk * FX_SD_SECTOR_SIZE);

                    if (SD_WaitReady(FX_SD_READY_TIMEOUT_MS) != HAL_OK)
                    {
                        status = FX_IO_ERROR;
                        break;
                    }

                    g_sd_tx_state = FX_SD_TRANSFER_ONGOING;
                    if (HAL_SD_WriteBlocks_DMA(&hsd1, sd_dma_buf,
                                               cur_sector, chunk) != HAL_OK)
                    {
                        g_sd_tx_state = FX_SD_TRANSFER_IDLE;
                        status = FX_IO_ERROR;
                        break;
                    }

                    status = SD_WaitTxDone(FX_SD_TRANSFER_TIMEOUT_MS);
                    if (status != FX_SUCCESS) break;

                    /* 쓰기 완료 후 카드 ready 확인 */
                    if (SD_WaitReady(FX_SD_READY_TIMEOUT_MS) != HAL_OK)
                    {
                        status = FX_IO_ERROR;
                        break;
                    }

                    remaining  -= chunk;
                    cur_sector += chunk;
                    cur_buf    += chunk * FX_SD_SECTOR_SIZE;
                }

                media_ptr->fx_media_driver_status = status;
                return;
            }

            /* 임시 정렬 버퍼에 복사 후 쓰기 */
            memcpy(sd_dma_buf, buf, sectors * FX_SD_SECTOR_SIZE);
            dma_buf = sd_dma_buf;
            use_tmp = 1U;
        }
        else
        {
            dma_buf = buf;
        }

        /* SD 카드 준비 대기 */
        if (SD_WaitReady(FX_SD_READY_TIMEOUT_MS) != HAL_OK)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
            return;
        }

        /* DMA 쓰기 시작 */
        g_sd_tx_state = FX_SD_TRANSFER_ONGOING;
        if (HAL_SD_WriteBlocks_DMA(&hsd1, dma_buf, sector, sectors) != HAL_OK)
        {
            g_sd_tx_state = FX_SD_TRANSFER_IDLE;
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
            return;
        }

        /* DMA 완료 대기 */
        status = SD_WaitTxDone(FX_SD_TRANSFER_TIMEOUT_MS);

        /* 쓰기 후 카드 ready 재확인 */
        if (status == FX_SUCCESS)
        {
            if (SD_WaitReady(FX_SD_READY_TIMEOUT_MS) != HAL_OK)
            {
                status = FX_IO_ERROR;
            }
        }

        media_ptr->fx_media_driver_status = status;
        break;
    }

    /* ------------------------------------------------------------------ */
    case FX_DRIVER_FLUSH:
    /* ------------------------------------------------------------------ */
        /* SD 카드는 쓰기 시 즉시 플러시됨. 추가 작업 불필요. */
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        break;

    /* ------------------------------------------------------------------ */
    case FX_DRIVER_ABORT:
    /* ------------------------------------------------------------------ */
        HAL_SD_Abort(&hsd1);
        g_sd_rx_state = FX_SD_TRANSFER_IDLE;
        g_sd_tx_state = FX_SD_TRANSFER_IDLE;
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        break;

    /* ------------------------------------------------------------------ */
    default:
    /* ------------------------------------------------------------------ */
        media_ptr->fx_media_driver_status = FX_IO_ERROR;
        break;
    }
}
