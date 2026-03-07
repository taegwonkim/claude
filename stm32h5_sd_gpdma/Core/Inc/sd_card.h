#ifndef __SD_CARD_H
#define __SD_CARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* -----------------------------------------------------------------------
 * 상수 정의
 * ----------------------------------------------------------------------- */
#define SD_BLOCK_SIZE           512U          /* SD 표준 블록 크기 (바이트) */
#define SD_TIMEOUT_MS           5000U         /* DMA 전송 최대 대기 시간 */
#define SD_MAX_RETRY            3U            /* 오류 시 재시도 횟수 */

/* DMA 전송 완료 이벤트 비트 (osEventFlags 또는 직접 플래그로 사용) */
#define SD_DMA_RX_CPLT_FLAG     (1U << 0)
#define SD_DMA_TX_CPLT_FLAG     (1U << 1)
#define SD_DMA_ERROR_FLAG       (1U << 2)

/* -----------------------------------------------------------------------
 * 열거형
 * ----------------------------------------------------------------------- */
typedef enum {
    SD_OK           = 0x00,
    SD_ERROR        = 0x01,
    SD_BUSY         = 0x02,
    SD_TIMEOUT      = 0x03,
    SD_PARAM_ERROR  = 0x04,
    SD_NOT_PRESENT  = 0x05,
} SD_StatusTypeDef;

/* -----------------------------------------------------------------------
 * SD 카드 정보 구조체
 * ----------------------------------------------------------------------- */
typedef struct {
    uint32_t CardCapacityMB;   /* 카드 총 용량 (MB) */
    uint32_t BlockSize;        /* 블록 크기 (보통 512) */
    uint32_t BlockCount;       /* 총 블록 수 */
    uint8_t  CardType;         /* SD_CARD_SDSC / SD_CARD_SDHC / SD_CARD_SDXC */
    bool     IsPresent;        /* 카드 감지 상태 */
} SD_CardInfoTypeDef;

/* -----------------------------------------------------------------------
 * 전송 상태 (volatile — ISR과 공유)
 * ----------------------------------------------------------------------- */
typedef struct {
    volatile bool RxComplete;
    volatile bool TxComplete;
    volatile bool Error;
} SD_TransferStateTypeDef;

/* -----------------------------------------------------------------------
 * 공개 API
 * ----------------------------------------------------------------------- */

/**
 * @brief  SD 카드 및 DMA 초기화
 * @retval SD_OK / SD_ERROR
 */
SD_StatusTypeDef SD_Init(void);

/**
 * @brief  SD 카드 존재 여부 확인
 * @retval true: 카드 있음, false: 카드 없음
 */
bool SD_IsPresent(void);

/**
 * @brief  카드 정보 조회
 * @param  info  결과를 저장할 구조체 포인터
 * @retval SD_OK / SD_ERROR
 */
SD_StatusTypeDef SD_GetCardInfo(SD_CardInfoTypeDef *info);

/**
 * @brief  DMA를 이용한 블록 읽기 (512바이트 단위)
 * @param  pBuf        수신 버퍼 (4바이트 정렬 필수)
 * @param  blockAddr   시작 블록 주소
 * @param  numBlocks   읽을 블록 수
 * @retval SD_OK / SD_ERROR / SD_TIMEOUT
 */
SD_StatusTypeDef SD_ReadBlocks_DMA(uint8_t *pBuf, uint32_t blockAddr, uint32_t numBlocks);

/**
 * @brief  DMA를 이용한 블록 쓰기 (512바이트 단위)
 * @param  pBuf        송신 버퍼 (4바이트 정렬 필수)
 * @param  blockAddr   시작 블록 주소
 * @param  numBlocks   쓸 블록 수
 * @retval SD_OK / SD_ERROR / SD_TIMEOUT
 */
SD_StatusTypeDef SD_WriteBlocks_DMA(const uint8_t *pBuf, uint32_t blockAddr, uint32_t numBlocks);

/**
 * @brief  SD 카드 내부 쓰기 완료 대기 (write-after-write 사이 필수)
 * @param  timeoutMs  최대 대기 ms
 * @retval SD_OK / SD_TIMEOUT
 */
SD_StatusTypeDef SD_WaitForWriteComplete(uint32_t timeoutMs);

/**
 * @brief  SD 카드 디마운트 / 전원 절약 모드 진입
 */
void SD_DeInit(void);

/* -----------------------------------------------------------------------
 * 내부 콜백 (HAL_SD_*Callback에서 호출 — 사용자 직접 호출 금지)
 * ----------------------------------------------------------------------- */
void SD_RxCpltCallback(SD_HandleTypeDef *hsd);
void SD_TxCpltCallback(SD_HandleTypeDef *hsd);
void SD_ErrorCallback(SD_HandleTypeDef *hsd);

#ifdef __cplusplus
}
#endif

#endif /* __SD_CARD_H */
