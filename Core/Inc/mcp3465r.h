/**
 ******************************************************************************
 * @file    mcp3465r.h
 * @brief   MCP3465R 4채널 16-bit Delta-Sigma ADC 드라이버 헤더
 *
 * MCP3465R 특성:
 *   - 4채널 (싱글엔디드) / 2채널 (차동) 입력
 *   - 16-bit resolution
 *   - 내부 2.4V 기준 전압 (R variant)
 *   - SPI 인터페이스
 *   - 프로그래머블 게인: 1/3x ~ 64x
 *   - 프로그래머블 오버샘플링 비율 (OSR)
 *   - 프로그래머블 데이터 레이트
 *
 * SPI Command Byte Format:
 *   Bit[7:6] = Device Address (default: 01)
 *   Bit[5:2] = Register Address
 *   Bit[1:0] = Command Type
 *     00 = Fast Command (ADCDATA static read for addr=0)
 *     01 = Static Read
 *     10 = Incremental Write
 *     11 = Incremental Read
 ******************************************************************************
 */

#ifndef __MCP3465R_H
#define __MCP3465R_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Defines -------------------------------------------------------------------*/

/* Device address (default for MCP3465R) */
#define MCP3465R_DEV_ADDR           0x01

/* Register addresses */
#define MCP3465R_REG_ADCDATA        0x00
#define MCP3465R_REG_CONFIG0        0x01
#define MCP3465R_REG_CONFIG1        0x02
#define MCP3465R_REG_CONFIG2        0x03
#define MCP3465R_REG_CONFIG3        0x04
#define MCP3465R_REG_IRQ            0x05
#define MCP3465R_REG_MUX            0x06
#define MCP3465R_REG_SCAN           0x07
#define MCP3465R_REG_TIMER          0x08
#define MCP3465R_REG_OFFSETCAL      0x09
#define MCP3465R_REG_GAINCAL        0x0A
#define MCP3465R_REG_LOCK           0x0D
#define MCP3465R_REG_CRCCFG         0x0F

/* Command types */
#define MCP3465R_CMD_STATIC_READ    0x01
#define MCP3465R_CMD_INC_WRITE      0x02
#define MCP3465R_CMD_INC_READ       0x03

/* Fast commands (bits [5:2] of command byte when [1:0]=11) */
#define MCP3465R_FAST_CMD_START     0x0A  /* ADC Conversion Start */
#define MCP3465R_FAST_CMD_STANDBY   0x0B  /* ADC Standby */
#define MCP3465R_FAST_CMD_SHUTDOWN  0x0C  /* ADC Shutdown */
#define MCP3465R_FAST_CMD_FULL_SHUT 0x0D  /* Full Shutdown */
#define MCP3465R_FAST_CMD_RESET     0x0E  /* Full Reset */

/* CONFIG0 register bits */
#define MCP3465R_CFG0_VREF_INT      (0x01 << 7)  /* Internal Vref select */
#define MCP3465R_CFG0_CLK_INT       (0x03 << 4)  /* Internal clock, no output */
#define MCP3465R_CFG0_CS_NONE       (0x00 << 2)  /* No current source */
#define MCP3465R_CFG0_ADC_STANDBY   (0x02 << 0)  /* ADC Standby mode */
#define MCP3465R_CFG0_ADC_CONV      (0x03 << 0)  /* ADC Conversion mode */

/* CONFIG1 register bits */
#define MCP3465R_CFG1_PRE_1         (0x00 << 6)  /* Prescaler: AMCLK = MCLK */
#define MCP3465R_CFG1_OSR_256       (0x03 << 2)  /* OSR = 256 */
#define MCP3465R_CFG1_OSR_1024      (0x05 << 2)  /* OSR = 1024 */
#define MCP3465R_CFG1_OSR_4096      (0x07 << 2)  /* OSR = 4096 */

/* CONFIG2 register bits */
#define MCP3465R_CFG2_BOOST_1X      (0x02 << 6)  /* Boost current: 1x */
#define MCP3465R_CFG2_GAIN_1X       (0x01 << 3)  /* Gain: 1x */
#define MCP3465R_CFG2_GAIN_033X     (0x00 << 3)  /* Gain: 1/3x */
#define MCP3465R_CFG2_AZ_MUX_EN     (0x01 << 2)  /* Auto-zero MUX enabled */

/* CONFIG3 register bits */
#define MCP3465R_CFG3_CONV_ONESHOT  (0x00 << 6)  /* One-shot conversion */
#define MCP3465R_CFG3_CONV_CONT     (0x03 << 6)  /* Continuous conversion */
#define MCP3465R_CFG3_DATA_16BIT    (0x00 << 4)  /* 16-bit data format */
#define MCP3465R_CFG3_DATA_32BIT_ZP (0x03 << 4)  /* 32-bit with zero padding */
#define MCP3465R_CFG3_CRC_OFF       (0x00 << 3)  /* CRC disabled */
#define MCP3465R_CFG3_EN_OFFCAL     (0x01 << 1)  /* Enable offset calibration */
#define MCP3465R_CFG3_EN_GAINCAL    (0x01 << 0)  /* Enable gain calibration */

/* IRQ register bits */
#define MCP3465R_IRQ_EN_FASTCMD     (0x01 << 6)  /* Enable fast commands */
#define MCP3465R_IRQ_EN_STP         (0x01 << 5)  /* Enable STP in scan */
#define MCP3465R_IRQ_DR_STATUS      (0x01 << 2)  /* Data ready status (active-low) */

/* MUX register - Input channel selections */
#define MCP3465R_MUX_VIN_CH0        0x00
#define MCP3465R_MUX_VIN_CH1        0x01
#define MCP3465R_MUX_VIN_CH2        0x02
#define MCP3465R_MUX_VIN_CH3        0x03
#define MCP3465R_MUX_VIN_AGND       0x08
#define MCP3465R_MUX_VIN_AVDD       0x09
#define MCP3465R_MUX_VIN_REFIN_P    0x0B
#define MCP3465R_MUX_VIN_REFIN_N    0x0C
#define MCP3465R_MUX_VIN_TEMP_P     0x0D
#define MCP3465R_MUX_VIN_TEMP_N     0x0E
#define MCP3465R_MUX_VIN_VCM        0x0F

/* ADC resolution */
#define MCP3465R_RESOLUTION         16
#define MCP3465R_MAX_CODE           ((1 << MCP3465R_RESOLUTION) - 1)  /* 65535 */

/* SPI timeout (ms) */
#define MCP3465R_SPI_TIMEOUT        10

/* Conversion timeout (ms) */
#define MCP3465R_CONV_TIMEOUT       50

/* Number of ADC channels */
#define MCP3465R_NUM_CHANNELS       4

/* Types ---------------------------------------------------------------------*/

/**
 * @brief ADC 채널 열거형
 */
typedef enum {
    MCP3465R_CH0 = 0,
    MCP3465R_CH1 = 1,
    MCP3465R_CH2 = 2,
    MCP3465R_CH3 = 3
} MCP3465R_Channel_t;

/**
 * @brief ADC 게인 설정
 */
typedef enum {
    MCP3465R_GAIN_033X = 0,  /**< 1/3x */
    MCP3465R_GAIN_1X   = 1,  /**< 1x */
    MCP3465R_GAIN_2X   = 2,  /**< 2x */
    MCP3465R_GAIN_4X   = 3,  /**< 4x */
    MCP3465R_GAIN_8X   = 4,  /**< 8x */
    MCP3465R_GAIN_16X  = 5,  /**< 16x */
    MCP3465R_GAIN_32X  = 6,  /**< 32x */
    MCP3465R_GAIN_64X  = 7   /**< 64x */
} MCP3465R_Gain_t;

/**
 * @brief MCP3465R ADC 인스턴스 구조체
 */
typedef struct {
    SPI_HandleTypeDef *hspi;          /**< SPI 핸들 포인터 */
    GPIO_TypeDef      *cs_port;       /**< CS 핀 GPIO 포트 */
    uint16_t           cs_pin;        /**< CS 핀 번호 */
    GPIO_TypeDef      *irq_port;      /**< IRQ 핀 GPIO 포트 (NULL이면 폴링) */
    uint16_t           irq_pin;       /**< IRQ 핀 번호 */
    float              vref;          /**< 기준 전압 (V) */
    MCP3465R_Gain_t    gain;          /**< 현재 게인 설정 */
    float              divider_scale; /**< 외부 전압 분배기 스케일 팩터 */
    int16_t            raw_data[MCP3465R_NUM_CHANNELS]; /**< 최근 원시 데이터 */
} MCP3465R_Handle_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  MCP3465R 초기화
 * @param  hadc: ADC 핸들 포인터
 * @param  hspi: SPI 핸들 포인터
 * @param  cs_port: CS 핀 포트
 * @param  cs_pin: CS 핀 번호
 * @param  vref: 기준 전압
 * @param  divider_scale: 전압 분배기 스케일 팩터
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hadc,
                                 SPI_HandleTypeDef *hspi,
                                 GPIO_TypeDef *cs_port, uint16_t cs_pin,
                                 float vref, float divider_scale);

/**
 * @brief  IRQ 핀 설정 (옵션)
 * @param  hadc: ADC 핸들
 * @param  irq_port: IRQ 핀 포트
 * @param  irq_pin: IRQ 핀 번호
 */
void MCP3465R_SetIRQPin(MCP3465R_Handle_t *hadc,
                         GPIO_TypeDef *irq_port, uint16_t irq_pin);

/**
 * @brief  레지스터 쓰기
 * @param  hadc: ADC 핸들
 * @param  reg: 레지스터 주소
 * @param  data: 쓸 데이터 (8-bit)
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hadc, uint8_t reg,
                                     uint8_t data);

/**
 * @brief  레지스터 읽기
 * @param  hadc: ADC 핸들
 * @param  reg: 레지스터 주소
 * @param  data: 읽은 데이터 저장 포인터
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *hadc, uint8_t reg,
                                    uint8_t *data);

/**
 * @brief  게인 설정
 * @param  hadc: ADC 핸들
 * @param  gain: 게인 값
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef MCP3465R_SetGain(MCP3465R_Handle_t *hadc,
                                    MCP3465R_Gain_t gain);

/**
 * @brief  단일 채널 변환 및 원시 값 읽기
 * @param  hadc: ADC 핸들
 * @param  channel: 채널 번호
 * @param  raw_value: 원시 값 저장 포인터
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_Handle_t *hadc,
                                        MCP3465R_Channel_t channel,
                                        int16_t *raw_value);

/**
 * @brief  단일 채널 전압 읽기 (분배기 보정 포함)
 * @param  hadc: ADC 핸들
 * @param  channel: 채널 번호
 * @param  voltage: 전압 값 저장 포인터
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef MCP3465R_ReadVoltage(MCP3465R_Handle_t *hadc,
                                        MCP3465R_Channel_t channel,
                                        float *voltage);

/**
 * @brief  4채널 전체 전압 읽기
 * @param  hadc: ADC 핸들
 * @param  voltages: 4채널 전압 배열 포인터
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_Handle_t *hadc,
                                            float voltages[MCP3465R_NUM_CHANNELS]);

/**
 * @brief  Fast Command 전송
 * @param  hadc: ADC 핸들
 * @param  cmd: Fast command code
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef MCP3465R_FastCommand(MCP3465R_Handle_t *hadc, uint8_t cmd);

/**
 * @brief  ADC 소프트 리셋
 * @param  hadc: ADC 핸들
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef MCP3465R_Reset(MCP3465R_Handle_t *hadc);

#ifdef __cplusplus
}
#endif

#endif /* __MCP3465R_H */
