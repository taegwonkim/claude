/**
 * @file    dac_drv.h
 * @brief   DAC 드라이버 (내부 DAC + 외부 MCP4922)
 *
 * 채널 매핑:
 *   CH1 (idx=0) : 내부 DAC1_OUT1  (PA4)
 *   CH2 (idx=1) : 내부 DAC1_OUT2  (PA5)
 *   CH3 (idx=2) : 외부 MCP4922_A  (SPI2)
 *   CH4 (idx=3) : 외부 MCP4922_B  (SPI2, 동일 칩)
 *
 * MCP4922 SPI 프레임 (16-bit):
 *  [15]   A/B     : 0=ChannelA, 1=ChannelB
 *  [14]   BUF     : 1=Buffered VREF
 *  [13]   GA      : 1=1x gain (VOUT = VREF * D/4096)
 *  [12]   SHDN    : 1=Active, 0=Shutdown
 *  [11:0] D11~D0  : 12-bit DAC data
 */

#ifndef DAC_DRV_H
#define DAC_DRV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"

/* =========================================================
 * MCP4922 SPI 명령 비트 정의
 * ========================================================= */
#define MCP4922_SEL_CHA     (0U << 15)
#define MCP4922_SEL_CHB     (1U << 15)
#define MCP4922_BUF_ON      (1U << 14)
#define MCP4922_BUF_OFF     (0U << 14)
#define MCP4922_GAIN_1X     (1U << 13)  /* VOUT = VREF * D/4096 */
#define MCP4922_GAIN_2X     (0U << 13)  /* VOUT = 2*VREF * D/4096 */
#define MCP4922_SHDN_ON     (1U << 12)
#define MCP4922_SHDN_OFF    (0U << 12)

/* SPI 타임아웃 (ms) */
#define DAC_SPI_TIMEOUT_MS  10U

/* =========================================================
 * 함수 프로토타입
 * ========================================================= */

/**
 * @brief DAC 드라이버 초기화
 *   - 내부 DAC1 활성화
 *   - MCP4922 초기화 (출력=0)
 */
void DAC_Drv_Init(void);

/**
 * @brief 채널 DAC 값 설정
 * @param ch_idx     채널 인덱스 (0~3)
 * @param value_12b  DAC 코드 (0~4095)
 */
void DAC_Drv_SetChannel(uint8_t ch_idx, uint32_t value_12b);

/**
 * @brief 전압(mV)을 DAC 코드로 변환
 * @param v_mv  목표 전압 (mV, 0~3300)
 * @return DAC 코드 (0~4095)
 */
uint32_t DAC_Drv_VoltageMvToCode(float v_mv);

/**
 * @brief 전체 채널 출력 0으로 설정 (긴급 차단)
 */
void DAC_Drv_DisableAll(void);

#ifdef __cplusplus
}
#endif

#endif /* DAC_DRV_H */
