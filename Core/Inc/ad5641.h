/**
 ******************************************************************************
 * @file    ad5641.h
 * @brief   AD5641 14-bit DAC 드라이버 헤더
 *
 * AD5641 특성:
 *   - 14-bit resolution (16384 steps)
 *   - SPI 인터페이스 (SCLK, DIN, SYNC)
 *   - 단일 채널 출력, 4개 칩 사용 (채널당 1개)
 *   - Vout = Vref × D / 16384
 *   - 전원 차단 모드 지원
 *
 * Data Format (16-bit, MSB first):
 *   Bit[15:14] = Operating Mode (00=Normal, 01=1kΩ GND, 10=100kΩ GND, 11=Hi-Z)
 *   Bit[13:0]  = 14-bit DAC Data
 ******************************************************************************
 */

#ifndef __AD5641_H
#define __AD5641_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Defines -------------------------------------------------------------------*/

/* AD5641 resolution */
#define AD5641_RESOLUTION           14
#define AD5641_MAX_CODE             ((1 << AD5641_RESOLUTION) - 1)  /* 16383 */

/* Operating modes (bit[15:14]) */
#define AD5641_MODE_NORMAL          0x0000
#define AD5641_MODE_PWDN_1K         0x4000
#define AD5641_MODE_PWDN_100K       0x8000
#define AD5641_MODE_PWDN_HIZ        0xC000

/* SPI timeout (ms) */
#define AD5641_SPI_TIMEOUT          10

/* Types ---------------------------------------------------------------------*/

/**
 * @brief AD5641 DAC 인스턴스 구조체
 */
typedef struct {
    SPI_HandleTypeDef *hspi;        /**< SPI 핸들 포인터 */
    GPIO_TypeDef      *cs_port;     /**< CS(SYNC) 핀 GPIO 포트 */
    uint16_t           cs_pin;      /**< CS(SYNC) 핀 번호 */
    float              vref;        /**< 기준 전압 (V) */
    uint16_t           current_code;/**< 현재 설정된 DAC 코드 */
} AD5641_Handle_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  AD5641 초기화
 * @param  hdac: DAC 핸들 포인터
 * @param  hspi: SPI 핸들 포인터
 * @param  cs_port: CS 핀 GPIO 포트
 * @param  cs_pin: CS 핀 번호
 * @param  vref: 기준 전압 (V)
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AD5641_Init(AD5641_Handle_t *hdac, SPI_HandleTypeDef *hspi,
                               GPIO_TypeDef *cs_port, uint16_t cs_pin,
                               float vref);

/**
 * @brief  DAC 원시 코드 값 설정 (0 ~ 16383)
 * @param  hdac: DAC 핸들 포인터
 * @param  code: 14-bit DAC 코드
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AD5641_SetRaw(AD5641_Handle_t *hdac, uint16_t code);

/**
 * @brief  DAC 출력 전압 설정
 * @param  hdac: DAC 핸들 포인터
 * @param  voltage: 출력 전압 (V), 범위: 0 ~ Vref
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AD5641_SetVoltage(AD5641_Handle_t *hdac, float voltage);

/**
 * @brief  DAC 전원 차단 모드 설정
 * @param  hdac: DAC 핸들 포인터
 * @param  mode: 전원 차단 모드 (AD5641_MODE_PWDN_xxx)
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AD5641_PowerDown(AD5641_Handle_t *hdac, uint16_t mode);

/**
 * @brief  전압 값을 DAC 코드로 변환
 * @param  hdac: DAC 핸들 포인터
 * @param  voltage: 전압 값 (V)
 * @retval 14-bit DAC 코드
 */
uint16_t AD5641_VoltageToCod(AD5641_Handle_t *hdac, float voltage);

/**
 * @brief  DAC 코드를 전압 값으로 변환
 * @param  hdac: DAC 핸들 포인터
 * @param  code: 14-bit DAC 코드
 * @retval 전압 값 (V)
 */
float AD5641_CodeToVoltage(AD5641_Handle_t *hdac, uint16_t code);

#ifdef __cplusplus
}
#endif

#endif /* __AD5641_H */
