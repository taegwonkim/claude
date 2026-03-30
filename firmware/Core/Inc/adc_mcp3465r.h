#ifndef __ADC_MCP3465R_H
#define __ADC_MCP3465R_H

#include "stm32l5xx_hal.h"
#include <stdint.h>

/* MCP3465R: 16-bit Delta-Sigma ADC, 4 differential / 8 single-ended channels */

#define MCP3465R_DEVICE_ADDR    0x01  /* Default device address (2-bit) */

/* Register Addresses (4-bit) */
#define MCP3465R_REG_ADCDATA    0x0
#define MCP3465R_REG_CONFIG0    0x1
#define MCP3465R_REG_CONFIG1    0x2
#define MCP3465R_REG_CONFIG2    0x3
#define MCP3465R_REG_CONFIG3    0x4
#define MCP3465R_REG_IRQ        0x5
#define MCP3465R_REG_MUX        0x6
#define MCP3465R_REG_SCAN       0x7
#define MCP3465R_REG_TIMER      0x8
#define MCP3465R_REG_OFFSETCAL  0x9
#define MCP3465R_REG_GAINCAL    0xA
#define MCP3465R_REG_LOCK       0xD
#define MCP3465R_REG_CRCCFG     0xF

/* Command Types (2-bit) */
#define MCP3465R_CMD_STATIC_READ    0x01
#define MCP3465R_CMD_INCREMENTAL_WRITE 0x02
#define MCP3465R_CMD_INCREMENTAL_READ  0x03
#define MCP3465R_CMD_FAST_CMD       0x00  /* Used with specific fast commands */

/* CONFIG0 bits */
#define MCP3465R_CFG0_VREF_SEL_INT  (0 << 7)  /* Internal VREF */
#define MCP3465R_CFG0_VREF_SEL_EXT  (1 << 7)  /* External VREF */
#define MCP3465R_CFG0_CLK_SEL_INT   (0 << 4)  /* Internal clock */
#define MCP3465R_CFG0_CLK_SEL_EXT   (3 << 4)  /* External clock */
#define MCP3465R_CFG0_CS_SEL_NONE   (0 << 2)  /* No current source */
#define MCP3465R_CFG0_ADC_MODE_CONV (3 << 0)  /* Continuous conversion */
#define MCP3465R_CFG0_ADC_MODE_ONESHOT (2 << 0) /* One-shot conversion */
#define MCP3465R_CFG0_ADC_MODE_STBY (1 << 0)  /* Standby */
#define MCP3465R_CFG0_ADC_MODE_SHUT (0 << 0)  /* Shutdown */

/* CONFIG1 bits - Prescaler and OSR */
#define MCP3465R_CFG1_PRE_1         (0 << 6)
#define MCP3465R_CFG1_PRE_2         (1 << 6)
#define MCP3465R_CFG1_OSR_256       (3 << 2)
#define MCP3465R_CFG1_OSR_512       (4 << 2)
#define MCP3465R_CFG1_OSR_1024      (5 << 2)
#define MCP3465R_CFG1_OSR_4096      (7 << 2)

/* CONFIG2 bits - Gain and boost */
#define MCP3465R_CFG2_BOOST_1X      (2 << 6)
#define MCP3465R_CFG2_GAIN_1X       (1 << 3)
#define MCP3465R_CFG2_GAIN_2X       (2 << 3)
#define MCP3465R_CFG2_AZ_MUX_EN     (1 << 2)

/* CONFIG3 bits */
#define MCP3465R_CFG3_CONV_MODE_CONT    (3 << 6)
#define MCP3465R_CFG3_CONV_MODE_ONESHOT (2 << 6)
#define MCP3465R_CFG3_DATA_FORMAT_32BIT (3 << 4) /* 32-bit with sign + ch ID */
#define MCP3465R_CFG3_DATA_FORMAT_DEF   (0 << 4) /* Default (24-bit) */
#define MCP3465R_CFG3_CRC_NONE          (0 << 2)
#define MCP3465R_CFG3_EN_OFFCAL         (1 << 1)
#define MCP3465R_CFG3_EN_GAINCAL        (1 << 0)

/* MUX register: VIN+ (bits 7:4), VIN- (bits 3:0) */
#define MCP3465R_MUX_CH(pos, neg)   (((pos) << 4) | (neg))
#define MCP3465R_MUX_VIN0    0x0
#define MCP3465R_MUX_VIN1    0x1
#define MCP3465R_MUX_VIN2    0x2
#define MCP3465R_MUX_VIN3    0x3
#define MCP3465R_MUX_VIN4    0x4
#define MCP3465R_MUX_VIN5    0x5
#define MCP3465R_MUX_VIN6    0x6
#define MCP3465R_MUX_VIN7    0x7
#define MCP3465R_MUX_AGND    0x8  /* Internal analog ground */
#define MCP3465R_MUX_AVDD    0x9
#define MCP3465R_MUX_REFIN_P 0xB
#define MCP3465R_MUX_REFIN_N 0xC
#define MCP3465R_MUX_TEMP_P  0xD
#define MCP3465R_MUX_TEMP_N  0xE

/* ADC parameters */
#define MCP3465R_RESOLUTION     16
#define MCP3465R_VREF           3.3f   /* External VREF voltage */
#define MCP3465R_VOLTAGE_DIVIDER_RATIO  (3.3f / 5.0f)  /* Resistor divider to scale 0-5V -> 0-3.3V */

/* Number of voltage and current channels */
#define MCP3465R_NUM_VOLT_CH    4   /* CH0~CH3: voltage feedback */
#define MCP3465R_NUM_CURR_CH    4   /* CH4~CH7: current sensing */

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
} MCP3465R_Handle_t;

/**
 * @brief Initialize MCP3465R with default configuration
 */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hadc);

/**
 * @brief Write a register
 */
HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hadc, uint8_t reg, uint8_t *data, uint8_t len);

/**
 * @brief Read a register
 */
HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *hadc, uint8_t reg, uint8_t *data, uint8_t len);

/**
 * @brief Set MUX channel for single-ended measurement (VIN+ = ch, VIN- = AGND)
 */
HAL_StatusTypeDef MCP3465R_SetChannel(MCP3465R_Handle_t *hadc, uint8_t vin_pos);

/**
 * @brief Trigger one-shot conversion and read raw result
 */
HAL_StatusTypeDef MCP3465R_ReadConversion(MCP3465R_Handle_t *hadc, int32_t *raw_value);

/**
 * @brief Read voltage from a specific single-ended channel (with mux switch + conversion)
 * @param ch Channel number (0-7)
 * @param voltage Output voltage in V (after divider compensation)
 */
HAL_StatusTypeDef MCP3465R_ReadChannelVoltage(MCP3465R_Handle_t *hadc, uint8_t ch, float *voltage);

/**
 * @brief Read current from a specific current sense channel
 * @param ch Current sense channel (4-7 mapped to load channels 0-3)
 * @param current_mA Output current in mA
 * @param shunt_ohm Shunt resistor value in ohms
 */
HAL_StatusTypeDef MCP3465R_ReadChannelCurrent(MCP3465R_Handle_t *hadc, uint8_t ch,
                                               float *current_mA, float shunt_ohm);

#endif /* __ADC_MCP3465R_H */
