/**
 ******************************************************************************
 * @file    sd_driver.h
 * @brief   STM32H5 SDMMC1 IDMA 블록 읽기/쓰기 드라이버
 *
 *  특징:
 *    - SDMMC1 내장 IDMA 사용 (별도 GPDMA 채널 불필요)
 *    - 인터럽트 구동 완료 대기 (비동기 DMA + 콜백 플래그)
 *    - D-Cache 정합 관리 (SCB_CleanDCache / SCB_InvalidateDCache)
 *    - 버퍼는 반드시 32바이트 정렬, .DMABufferSection 배치 권장
 *
 *  충돌 방지:
 *    - UART GPDMA 와 분리된 버스/채널 사용
 *    - SDMMC NVIC 우선순위 > GPDMA UART 우선순위
 *
 *  대상 보드: NUCLEO-H563ZI (STM32H563ZI)
 ******************************************************************************
 */
#ifndef SD_DRIVER_H
#define SD_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"
#include <stdint.h>

/* ── 설정 상수 ──────────────────────────────────────────────────────────── */
#define SD_SECTOR_SIZE      512U        /**< SD 섹터 크기 (바이트)          */
#define SD_TIMEOUT_MS       5000U       /**< DMA 전송 완료 대기 최대 시간   */
#define SD_DCACHE_LINE_SZ   32U         /**< Cortex-M33 D-Cache 라인 크기   */

/* ── HAL 핸들 (main.c 에서 정의) ────────────────────────────────────────── */
extern SD_HandleTypeDef hsd1;

/* ── 상태 코드 ──────────────────────────────────────────────────────────── */
typedef enum
{
    SD_OK        = 0,   /**< 성공                    */
    SD_ERR_BUSY  = 1,   /**< 카드 busy 상태 타임아웃 */
    SD_ERR_DMA   = 2,   /**< DMA 전송 오류           */
    SD_ERR_PARAM = 3,   /**< 잘못된 파라미터         */
} SD_Status;

/* ── 공개 API ────────────────────────────────────────────────────────────── */

/**
 * @brief  SDMMC1 초기화 (HAL_SD_Init + 4-bit 와이드 버스 설정)
 *
 * @note   ClockDiv=4 → SDMMC_CK ≈ 12.5 MHz (PLL1Q=100 MHz 기준)
 *         이 함수를 호출하기 전 HAL_Init() 및 SystemClock_Config() 완료 필요.
 *
 * @retval SD_OK / SD_ERR_BUSY
 */
SD_Status SD_Init(void);

/**
 * @brief  SD 카드 섹터 읽기 (DMA, 인터럽트 완료 대기)
 *
 * @param  buf       수신 버퍼 포인터
 *                   반드시 32바이트 정렬, .DMABufferSection 배치 권장
 * @param  sector    읽기 시작 LBA 섹터 번호
 * @param  count     읽을 섹터 수
 *
 * @note   함수 내부에서 SCB_InvalidateDCache_by_Addr 호출.
 *         buf 를 캐시 라인 경계에 맞추지 않으면 인접 데이터 손상 가능.
 *
 * @retval SD_OK / SD_ERR_BUSY / SD_ERR_DMA
 */
SD_Status SD_ReadBlocks(uint8_t *buf, uint32_t sector, uint32_t count);

/**
 * @brief  SD 카드 섹터 쓰기 (DMA, 인터럽트 완료 대기)
 *
 * @param  buf       송신 버퍼 포인터 (const, 내용 변경 없음)
 *                   반드시 32바이트 정렬, .DMABufferSection 배치 권장
 * @param  sector    쓰기 시작 LBA 섹터 번호
 * @param  count     쓸 섹터 수
 *
 * @note   함수 내부에서 SCB_CleanDCache_by_Addr 호출.
 *
 * @retval SD_OK / SD_ERR_BUSY / SD_ERR_DMA
 */
SD_Status SD_WriteBlocks(const uint8_t *buf, uint32_t sector, uint32_t count);

/**
 * @brief  SD 카드 준비 여부 확인
 * @retval 1=준비완료, 0=미준비 or 오류
 */
uint8_t SD_IsReady(void);

/**
 * @brief  SDMMC1 HAL IRQ 핸들러 – stm32h5xx_it.c 에서 호출
 */
void SD_IRQHandler(void);

#ifdef __cplusplus
}
#endif
#endif /* SD_DRIVER_H */
