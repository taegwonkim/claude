/**
 ******************************************************************************
 * @file    dac_ad5641.h
 * @brief   AD5641 14비트 SPI DAC 드라이버
 *
 * AD5641 전송 포맷 (16비트):
 *   Bit15 : PD1  (Power-Down 모드 선택 MSB)
 *   Bit14 : PD0  (Power-Down 모드 선택 LSB)
 *   Bit13~0: D13~D0 (DAC 데이터, MSB first)
 *
 * Power-Down 모드:
 *   PD1=0, PD0=0 : Normal operation
 *   PD1=0, PD0=1 : 1kΩ to GND
 *   PD1=1, PD0=0 : 100kΩ to GND
 *   PD1=1, PD0=1 : Hi-Z (three-state)
 *
 * Vout = (Code / 16384) × Vref
 ******************************************************************************
 */

#ifndef __DAC_AD5641_H
#define __DAC_AD5641_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* -------------------------------------------------------------------------
 * AD5641 Power-Down 모드
 * ------------------------------------------------------------------------- */
typedef enum {
    DAC_PD_NORMAL  = 0x0000,   /* Normal operation */
    DAC_PD_1K      = 0x4000,   /* 1kΩ to GND */
    DAC_PD_100K    = 0x8000,   /* 100kΩ to GND */
    DAC_PD_HIZ     = 0xC000,   /* Hi-Z */
} DacPowerDown_t;

/* -------------------------------------------------------------------------
 * 채널별 CS 포트/핀 테이블 (main.h 핀 정의와 매칭)
 * ------------------------------------------------------------------------- */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} DacCsPin_t;

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * @brief  AD5641 초기화 (CS 핀 High 설정)
 */
void DAC_Init(void);

/**
 * @brief  특정 채널에 DAC 코드 출력
 * @param  channel  채널 인덱스 (0~3)
 * @param  code     14비트 코드 (0x0000 ~ 0x3FFF)
 * @param  pd_mode  Power-Down 모드
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef DAC_WriteCode(uint8_t channel, uint16_t code, DacPowerDown_t pd_mode);

/**
 * @brief  전압(mV)으로 DAC 출력 (내부적으로 코드 변환)
 * @param  channel    채널 인덱스 (0~3)
 * @param  voltage_mv 목표 전압 (0 ~ VREF_MV)
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef DAC_SetVoltage(uint8_t channel, uint16_t voltage_mv);

/**
 * @brief  특정 채널 Power-Down
 * @param  channel  채널 인덱스 (0~3)
 * @param  pd_mode  Power-Down 모드 (DAC_PD_NORMAL 제외)
 */
HAL_StatusTypeDef DAC_PowerDown(uint8_t channel, DacPowerDown_t pd_mode);

/**
 * @brief  mV → 14비트 DAC 코드 변환 유틸리티
 */
static inline uint16_t DAC_VoltageToCode(uint16_t mv)
{
    uint32_t code = ((uint32_t)mv * (DAC_MAX_CODE + 1U)) / VREF_MV;
    return (code > DAC_MAX_CODE) ? DAC_MAX_CODE : (uint16_t)code;
}

#ifdef __cplusplus
}
#endif

#endif /* __DAC_AD5641_H */
