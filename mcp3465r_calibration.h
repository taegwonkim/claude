#ifndef MCP3465R_CALIBRATION_H
#define MCP3465R_CALIBRATION_H

#include "stm32l5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* -----------------------------------------------------------------------
 * MCP3465R 레지스터 주소
 * ----------------------------------------------------------------------- */
#define MCP3465R_REG_ADCDATA     0x0
#define MCP3465R_REG_CONFIG0     0x1
#define MCP3465R_REG_CONFIG1     0x2
#define MCP3465R_REG_CONFIG2     0x3
#define MCP3465R_REG_CONFIG3     0x4
#define MCP3465R_REG_IRQ         0x5
#define MCP3465R_REG_MUX         0x6
#define MCP3465R_REG_SCAN        0x7
#define MCP3465R_REG_TIMER       0x8
#define MCP3465R_REG_OFFSETCAL   0x9
#define MCP3465R_REG_GAINCAL     0xA
#define MCP3465R_REG_LOCK        0xD
#define MCP3465R_REG_CRCCFG      0xF

/* SPI 명령 타입 (하위 2비트) */
#define MCP3465R_CMD_STATIC_READ  0x01
#define MCP3465R_CMD_INC_WRITE    0x02
#define MCP3465R_CMD_INC_READ     0x03

/* 명령 바이트: [DEV_ADDR(7:6) | REG_ADDR(5:2) | CMD(1:0)] */
#define MCP3465R_CMD_BYTE(dev_addr, reg, cmd) \
    (((dev_addr) << 6) | ((reg) << 2) | (cmd))

/* CONFIG3: 출력 포맷 설정값 (24-bit + channel ID 없음, 2's complement) */
#define CONFIG3_DATA_FORMAT_24BIT  0x00
#define CONFIG3_DATA_FORMAT_32BIT  0x30   /* 상위 8비트: channel ID / 부호 */

/* GAINCAL 기본값 = 1.0 (1.23 fixed-point) */
#define GAINCAL_UNITY  0x800000UL

/* 교정에 사용할 기준 전압 및 VREF */
#define CAL_VREF       5.0f
#define CAL_V_OFFSET   0.5f   /* Offset 교정 인가 전압 */
#define CAL_V_GAIN     4.5f   /* Gain 교정 인가 전압   */
#define ADC_FULL_SCALE 8388608.0f  /* 2^23 (24-bit, 단극성 기준) */

/* 평균 샘플 횟수 */
#define CAL_SAMPLE_COUNT  32

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    uint8_t            dev_addr;  /* 0x01 (기본) */
} MCP3465R_Handle_t;

typedef struct {
    int32_t  offset_code;   /* OFFSETCAL에 쓸 값 */
    uint32_t gain_code;     /* GAINCAL에 쓸 값    */
    float    offset_voltage;
    float    gain_error;
} MCP3465R_CalResult_t;

/* 함수 선언 */
HAL_StatusTypeDef MCP3465R_WriteReg24(MCP3465R_Handle_t *dev, uint8_t reg, uint32_t val);
HAL_StatusTypeDef MCP3465R_ReadReg24(MCP3465R_Handle_t *dev, uint8_t reg, uint32_t *val);
int32_t           MCP3465R_ReadADC_Single(MCP3465R_Handle_t *dev);
int32_t           MCP3465R_ReadADC_Average(MCP3465R_Handle_t *dev, uint16_t n);
HAL_StatusTypeDef MCP3465R_ApplyCalibration(MCP3465R_Handle_t *dev,
                                             const MCP3465R_CalResult_t *cal);
HAL_StatusTypeDef MCP3465R_RunOffsetCal(MCP3465R_Handle_t *dev,
                                         MCP3465R_CalResult_t *cal);
HAL_StatusTypeDef MCP3465R_RunGainCal(MCP3465R_Handle_t *dev,
                                       MCP3465R_CalResult_t *cal);
HAL_StatusTypeDef MCP3465R_RunFullCal(MCP3465R_Handle_t *dev,
                                       MCP3465R_CalResult_t *cal);

#endif /* MCP3465R_CALIBRATION_H */
