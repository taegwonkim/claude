/**
 * @file    sd_card.c
 * @brief   STM32H5 SDMMC1 + GPDMA DMA 기반 SD 카드 드라이버
 *
 * 설계 원칙
 * ----------
 * - HAL_SD_ReadBlocks_DMA / HAL_SD_WriteBlocks_DMA 사용
 * - 전송 완료/에러는 HAL 콜백(ISR 컨텍스트)에서 volatile 플래그로 통보
 * - 메인 컨텍스트에서 폴링(플래그 대기) + HAL_Delay 타임아웃으로 안전 처리
 * - 버퍼는 반드시 4바이트 정렬 & D-Cache flush/invalidate 필요
 *   (STM32H5는 D-Cache 기본 활성화 → SCB_CleanDCache_by_Addr 사용)
 */

#include "sd_card.h"
#include "main.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * 내부 상태 (ISR과 공유 — volatile 필수)
 * ----------------------------------------------------------------------- */
static volatile SD_TransferStateTypeDef s_Transfer = {false, false, false};

/* -----------------------------------------------------------------------
 * 내부 헬퍼
 * ----------------------------------------------------------------------- */
/**
 * @brief D-Cache를 클린(Write-Back)하여 DMA가 최신 데이터를 읽게 함
 *        쓰기 전 호출
 */
static inline void SD_CacheClean(uint8_t *buf, uint32_t sizeBytes)
{
#if defined(__DCACHE_PRESENT) && __DCACHE_PRESENT
    /* 주소는 32바이트(캐시 라인) 정렬 필요 → 호출 전 정렬 보장 */
    SCB_CleanDCache_by_Addr((uint32_t *)buf, (int32_t)sizeBytes);
#else
    (void)buf;
    (void)sizeBytes;
#endif
}

/**
 * @brief D-Cache를 인벨리데이트(무효화)하여 DMA가 채운 최신 데이터를 읽음
 *        읽기 후 호출
 */
static inline void SD_CacheInvalidate(uint8_t *buf, uint32_t sizeBytes)
{
#if defined(__DCACHE_PRESENT) && __DCACHE_PRESENT
    SCB_InvalidateDCache_by_Addr((uint32_t *)buf, (int32_t)sizeBytes);
#else
    (void)buf;
    (void)sizeBytes;
#endif
}

/**
 * @brief 전송 완료/에러 플래그를 타임아웃까지 대기
 * @param flag  대기할 플래그 포인터 (volatile bool)
 * @param timeoutMs  최대 대기 ms
 * @retval SD_OK / SD_TIMEOUT
 */
static SD_StatusTypeDef SD_WaitFlag(volatile bool *flag, uint32_t timeoutMs)
{
    uint32_t tickStart = HAL_GetTick();
    while (!(*flag) && !s_Transfer.Error) {
        if ((HAL_GetTick() - tickStart) >= timeoutMs) {
            return SD_TIMEOUT;
        }
    }
    return s_Transfer.Error ? SD_ERROR : SD_OK;
}

/* -----------------------------------------------------------------------
 * 공개 API 구현
 * ----------------------------------------------------------------------- */

SD_StatusTypeDef SD_Init(void)
{
    /* hsd1은 main.c의 MX_SDMMC1_SD_Init()에서 이미 초기화됨
     * 여기서는 드라이버 레이어 상태만 초기화 */
    memset((void *)&s_Transfer, 0, sizeof(s_Transfer));

    if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) {
        return SD_OK;
    }
    return SD_ERROR;
}

bool SD_IsPresent(void)
{
    /* SD_DETECT 핀 Active-Low: LOW = 카드 있음 */
    return (HAL_GPIO_ReadPin(SD_DETECT_GPIO_Port, SD_DETECT_Pin) == GPIO_PIN_RESET);
}

SD_StatusTypeDef SD_GetCardInfo(SD_CardInfoTypeDef *info)
{
    if (info == NULL) return SD_PARAM_ERROR;

    HAL_SD_CardInfoTypeDef halInfo;
    if (HAL_SD_GetCardInfo(&hsd1, &halInfo) != HAL_OK) {
        return SD_ERROR;
    }

    info->BlockSize       = halInfo.LogBlockSize;
    info->BlockCount      = halInfo.LogBlockNbr;
    info->CardCapacityMB  = (uint32_t)((uint64_t)halInfo.LogBlockNbr *
                                        halInfo.LogBlockSize / (1024UL * 1024UL));
    info->CardType        = (uint8_t)halInfo.CardType;
    info->IsPresent       = SD_IsPresent();
    return SD_OK;
}

/* -----------------------------------------------------------------------
 * DMA 읽기
 * ----------------------------------------------------------------------- */
SD_StatusTypeDef SD_ReadBlocks_DMA(uint8_t *pBuf, uint32_t blockAddr, uint32_t numBlocks)
{
    if (pBuf == NULL || numBlocks == 0) return SD_PARAM_ERROR;
    if (!SD_IsPresent()) return SD_NOT_PRESENT;

    /* 4바이트 정렬 확인 */
    if ((uintptr_t)pBuf & 0x3U) return SD_PARAM_ERROR;

    /* DMA 수신 전 캐시 무효화 (이전 캐시 데이터가 새 데이터 덮어쓰지 않도록) */
    SD_CacheInvalidate(pBuf, numBlocks * SD_BLOCK_SIZE);

    /* 상태 초기화 */
    s_Transfer.RxComplete = false;
    s_Transfer.Error      = false;

    uint32_t sizeBytes = numBlocks * SD_BLOCK_SIZE;

    /* HAL DMA 읽기 시작 */
    if (HAL_SD_ReadBlocks_DMA(&hsd1, pBuf, blockAddr, numBlocks) != HAL_OK) {
        return SD_ERROR;
    }

    /* 완료 대기 */
    SD_StatusTypeDef st = SD_WaitFlag(&s_Transfer.RxComplete, SD_TIMEOUT_MS);

    if (st == SD_OK) {
        /* 수신 완료 후 캐시 재무효화 (DMA가 직접 SRAM 쓴 데이터 CPU가 읽게 함) */
        SD_CacheInvalidate(pBuf, sizeBytes);
    }

    return st;
}

/* -----------------------------------------------------------------------
 * DMA 쓰기
 * ----------------------------------------------------------------------- */
SD_StatusTypeDef SD_WriteBlocks_DMA(const uint8_t *pBuf, uint32_t blockAddr, uint32_t numBlocks)
{
    if (pBuf == NULL || numBlocks == 0) return SD_PARAM_ERROR;
    if (!SD_IsPresent()) return SD_NOT_PRESENT;

    if ((uintptr_t)pBuf & 0x3U) return SD_PARAM_ERROR;

    /* 쓰기 전 캐시 클린 (CPU 캐시 → SRAM 반영, DMA가 최신 데이터 읽도록) */
    SD_CacheClean((uint8_t *)pBuf, numBlocks * SD_BLOCK_SIZE);

    s_Transfer.TxComplete = false;
    s_Transfer.Error      = false;

    if (HAL_SD_WriteBlocks_DMA(&hsd1, (uint8_t *)pBuf, blockAddr, numBlocks) != HAL_OK) {
        return SD_ERROR;
    }

    /* DMA 전송 완료 대기 */
    SD_StatusTypeDef st = SD_WaitFlag(&s_Transfer.TxComplete, SD_TIMEOUT_MS);
    if (st != SD_OK) return st;

    /* SD 카드 내부 프로그래밍 완료 대기 */
    return SD_WaitForWriteComplete(SD_TIMEOUT_MS);
}

/* -----------------------------------------------------------------------
 * 쓰기 완료 대기 (SD 카드 내부 플래시 프로그래밍)
 * ----------------------------------------------------------------------- */
SD_StatusTypeDef SD_WaitForWriteComplete(uint32_t timeoutMs)
{
    uint32_t tickStart = HAL_GetTick();
    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
        if ((HAL_GetTick() - tickStart) >= timeoutMs) {
            return SD_TIMEOUT;
        }
    }
    return SD_OK;
}

void SD_DeInit(void)
{
    HAL_SD_DeInit(&hsd1);
}

/* -----------------------------------------------------------------------
 * HAL 콜백 구현 (ISR 컨텍스트 — 최소 처리만)
 * HAL_SD_RxCpltCallback / HAL_SD_TxCpltCallback / HAL_SD_ErrorCallback은
 * stm32h5xx_hal_sd.c 에서 weak 선언됨 → 여기서 override
 * ----------------------------------------------------------------------- */

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1) {
        s_Transfer.RxComplete = true;
    }
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1) {
        s_Transfer.TxComplete = true;
    }
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1) {
        s_Transfer.Error = true;
    }
}

/* 외부(user_diskio.c)에서 호출되는 래퍼 — sd_card.h에 선언된 버전 */
void SD_RxCpltCallback(SD_HandleTypeDef *hsd) { HAL_SD_RxCpltCallback(hsd); }
void SD_TxCpltCallback(SD_HandleTypeDef *hsd) { HAL_SD_TxCpltCallback(hsd); }
void SD_ErrorCallback(SD_HandleTypeDef *hsd)  { HAL_SD_ErrorCallback(hsd);  }
