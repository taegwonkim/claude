/**
 ******************************************************************************
 * @file    mpu_config.h
 * @brief   MPU 비캐시 영역 설정 – DMA 버퍼 D-Cache 충돌 원천 차단
 *
 *  문제:
 *    STM32H563 은 Cortex-M33 D-Cache(32B 라인)를 가짐.
 *    CPU가 캐시에 쓴 데이터를 DMA가 메모리에서 읽으면 구버전 데이터를 읽음.
 *    DMA가 메모리에 쓴 데이터를 CPU가 캐시에서 읽으면 구버전 데이터를 읽음.
 *
 *  해결 (MPU 방식, Exam2의 SCB 수동 호출 방식보다 안전):
 *    SRAM2 (0x20030000, 8KB) 영역 전체를 MPU Region 0으로 지정:
 *      Normal Memory / Non-Cacheable / Non-Bufferable / Shareable
 *    → 이 영역에 있는 모든 변수는 캐시를 거치지 않음
 *    → SDMMC IDMA / GPDMA1이 항상 최신 메모리 내용을 읽고 씀
 *    → SCB_CleanDCache / SCB_InvalidateDCache 호출 불필요
 *
 *  버퍼 배치 방법:
 *    __attribute__((section(".dma_buf"))) 로 선언하면
 *    링커 스크립트가 SRAM2(>RAM2) 에 배치.
 *
 *  대상: NUCLEO-H563ZI (STM32H563ZI)
 *  SRAM2: 0x20030000 – 0x2003FFFF (64KB)
 *  MPU 영역: Region 0 = 0x20030000, 8KB
 ******************************************************************************
 */
#ifndef MPU_CONFIG_H
#define MPU_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"

/* ── DMA 버퍼 영역 정의 ─────────────────────────────────────────────────── */

/** DMA 버퍼가 배치되는 SRAM2 시작 주소 */
#define DMA_BUF_BASE     0x20030000UL

/** MPU가 커버하는 크기 (8 KB = MPU_REGION_SIZE_8KB) */
#define DMA_BUF_SIZE_KB  8U

/**
 * @brief  MPU 초기화 – DMA 버퍼 영역을 Non-Cacheable로 설정
 *
 *  MPU Region 0:
 *    Base    = 0x20030000 (SRAM2)
 *    Size    = 8 KB
 *    Access  = Full access (Priv/Unpriv R/W)
 *    TEX     = 1, C=0, B=0 → Normal, Non-Cacheable, Non-Bufferable
 *    S       = 1 → Shareable (DMA 공유 허용)
 *    XN      = 1 → 코드 실행 금지
 *
 * @note  HAL_Init() 이전에 호출 권장 (SystemInit에서 호출 가능).
 *        MPU 재설정 중 인터럽트를 잠깐 비활성화함.
 */
void MPU_ConfigDMARegion(void);

#ifdef __cplusplus
}
#endif
#endif /* MPU_CONFIG_H */
