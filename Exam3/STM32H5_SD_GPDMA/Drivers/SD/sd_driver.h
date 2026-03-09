/**
 ******************************************************************************
 * @file    sd_driver.h
 * @brief   STM32H5 SDMMC1 IDMA 블록 읽기/쓰기 드라이버
 *
 *  특징:
 *    ① SDMMC1 내장 IDMA 사용 (STM32H5에서 GPDMA→SDMMC 요청 라인 없음)
 *    ② 인터럽트 구동 완료 대기 (HAL 콜백 플래그 방식)
 *    ③ D-Cache 정합은 MPU Non-Cacheable 영역으로 해결 (SCB 호출 불필요)
 *    ④ High-Speed 25 MHz 전환 지원 (SD_SetHighSpeed)
 *    ⑤ RTOS/ThreadX 미사용 – 순수 베어메탈
 *
 *  버퍼 규칙:
 *    - buf는 반드시 .dma_buf 섹션 (SRAM2 Non-Cacheable 영역) 에 배치
 *    - 32바이트 정렬 권장 (향후 캐시 전환 시를 위해)
 *
 *  대상: NUCLEO-H563ZI (STM32H563ZI)
 ******************************************************************************
 */
#ifndef SD_DRIVER_H
#define SD_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"
#include <stdint.h>

/* ── 설정 ────────────────────────────────────────────────────────────────── */
#define SD_SECTOR_SIZE   512U       /**< 섹터 크기 (바이트)               */
#define SD_TIMEOUT_MS    5000U      /**< DMA 완료 대기 최대 시간 (ms)     */

/* ── 상태 코드 ──────────────────────────────────────────────────────────── */
typedef enum {
    SD_OK         = 0,  /**< 성공                          */
    SD_ERR_INIT   = 1,  /**< HAL_SD_Init 실패              */
    SD_ERR_WIDE   = 2,  /**< 4-bit 전환 실패               */
    SD_ERR_BUSY   = 3,  /**< 카드 busy / 타임아웃          */
    SD_ERR_TX     = 4,  /**< 쓰기 DMA 오류                 */
    SD_ERR_RX     = 5,  /**< 읽기 DMA 오류                 */
    SD_ERR_PARAM  = 6,  /**< 잘못된 파라미터               */
} SD_Status;

/* ── 카드 정보 ──────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t block_count;   /**< 총 블록 수                */
    uint32_t block_size;    /**< 블록 크기 (바이트, 보통 512) */
    uint64_t capacity_mb;   /**< 용량 (MB)                 */
    uint8_t  card_type;     /**< HAL_SD_CardTypeTypeDef    */
} SD_CardInfo;

/* ── HAL 핸들 (main.c 에서 정의) ────────────────────────────────────────── */
extern SD_HandleTypeDef hsd1;

/* ── 공개 API ────────────────────────────────────────────────────────────── */

/**
 * @brief  SDMMC1 초기화 (1-bit 감지 → 4-bit 전환)
 * @note   MPU_ConfigDMARegion() 및 SystemClock_Config() 이후 호출
 * @retval SD_OK / SD_ERR_INIT / SD_ERR_WIDE
 */
SD_Status SD_Init(void);

/**
 * @brief  High-Speed 모드로 전환 (25 MHz, 초기화 이후 호출)
 * @retval SD_OK / SD_ERR_BUSY
 */
SD_Status SD_SetHighSpeed(void);

/**
 * @brief  카드 정보 조회
 * @param  info  결과를 저장할 구조체 포인터
 * @retval SD_OK / SD_ERR_BUSY
 */
SD_Status SD_GetInfo(SD_CardInfo *info);

/**
 * @brief  섹터 읽기 (IDMA, 인터럽트 완료 대기)
 *
 * @param  buf     수신 버퍼 (.dma_buf 섹션, 32바이트 정렬)
 * @param  sector  시작 LBA 섹터 번호
 * @param  count   읽을 섹터 수
 * @retval SD_OK / SD_ERR_BUSY / SD_ERR_RX / SD_ERR_PARAM
 */
SD_Status SD_ReadBlocks(uint8_t *buf, uint32_t sector, uint32_t count);

/**
 * @brief  섹터 쓰기 (IDMA, 인터럽트 완료 대기)
 *
 * @param  buf     송신 버퍼 (.dma_buf 섹션, 32바이트 정렬)
 * @param  sector  시작 LBA 섹터 번호
 * @param  count   쓸 섹터 수
 * @retval SD_OK / SD_ERR_BUSY / SD_ERR_TX / SD_ERR_PARAM
 */
SD_Status SD_WriteBlocks(const uint8_t *buf, uint32_t sector, uint32_t count);

/**
 * @brief  카드가 TRANSFER 상태(준비)인지 확인
 * @retval 1 = 준비, 0 = 미준비
 */
uint8_t SD_IsReady(void);

/**
 * @brief  SDMMC1 IRQ 핸들러 진입점 (stm32h5xx_it.c 에서 호출)
 */
void SD_IRQHandler(void);

#ifdef __cplusplus
}
#endif
#endif /* SD_DRIVER_H */
