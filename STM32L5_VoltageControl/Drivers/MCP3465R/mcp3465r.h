/**
 ******************************************************************************
 * @file    mcp3465r.h
 * @brief   MCP3465R 4-Channel 16-bit Delta-Sigma ADC Driver (SPI)
 ******************************************************************************
 * MCP3465R is a 16-bit delta-sigma ADC with 4 single-ended or 2 differential
 * input channels. Communicates via SPI (Mode 0,0, MSB first).
 *
 * Register Map (8-bit command byte):
 *   [7:6] Device Address (default 01)
 *   [5:2] Register Address
 *   [1:0] Command Type: 01=Static Read, 10=Incremental Write, 11=Incremental Read
 ******************************************************************************
 */
#ifndef __MCP3465R_H
#define __MCP3465R_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Device Address ───────────────────────────────────────────────────── */
#define MCP3465R_DEV_ADDR           (0x01U << 6)

/* ── Register Addresses ───────────────────────────────────────────────── */
#define MCP3465R_REG_ADCDATA        (0x00U << 2)
#define MCP3465R_REG_CONFIG0        (0x01U << 2)
#define MCP3465R_REG_CONFIG1        (0x02U << 2)
#define MCP3465R_REG_CONFIG2        (0x03U << 2)
#define MCP3465R_REG_CONFIG3        (0x04U << 2)
#define MCP3465R_REG_IRQ            (0x05U << 2)
#define MCP3465R_REG_MUX            (0x06U << 2)
#define MCP3465R_REG_SCAN           (0x07U << 2)
#define MCP3465R_REG_TIMER          (0x08U << 2)
#define MCP3465R_REG_OFFSETCAL      (0x09U << 2)
#define MCP3465R_REG_GAINCAL        (0x0AU << 2)
#define MCP3465R_REG_LOCK           (0x0DU << 2)
#define MCP3465R_REG_CRCCFG         (0x0FU << 2)

/* ── Command Types ────────────────────────────────────────────────────── */
#define MCP3465R_CMD_STATIC_READ    0x01U
#define MCP3465R_CMD_INC_WRITE      0x02U
#define MCP3465R_CMD_INC_READ       0x03U

/* ── Fast Commands ────────────────────────────────────────────────────── */
#define MCP3465R_FAST_CMD_CONVERSION    0x28U  /* Start conversion */
#define MCP3465R_FAST_CMD_STANDBY       0x2CU
#define MCP3465R_FAST_CMD_SHUTDOWN      0x30U
#define MCP3465R_FAST_CMD_RESET         0x38U

/* ── CONFIG0 Bits ─────────────────────────────────────────────────────── */
#define MCP3465R_CFG0_VREF_SEL_INT      (0x00U << 7)   /* Internal VREF */
#define MCP3465R_CFG0_VREF_SEL_EXT      (0x01U << 7)   /* External VREF */
#define MCP3465R_CFG0_CLK_SEL_INT       (0x02U << 4)   /* Internal clock */
#define MCP3465R_CFG0_CS_SEL_NONE       (0x00U << 2)   /* No current source */
#define MCP3465R_CFG0_ADC_MODE_CONV     (0x03U << 0)   /* Conversion mode */
#define MCP3465R_CFG0_ADC_MODE_STANDBY  (0x02U << 0)
#define MCP3465R_CFG0_ADC_MODE_SHUTDOWN (0x01U << 0)

/* ── CONFIG1 Bits ─────────────────────────────────────────────────────── */
#define MCP3465R_CFG1_OSR_256           (0x03U << 2)   /* OSR = 256 */
#define MCP3465R_CFG1_OSR_1024          (0x05U << 2)   /* OSR = 1024 */
#define MCP3465R_CFG1_OSR_4096          (0x07U << 2)   /* OSR = 4096 */
#define MCP3465R_CFG1_PRESCALE_1        (0x00U << 0)

/* ── CONFIG2 Bits ─────────────────────────────────────────────────────── */
#define MCP3465R_CFG2_BOOST_1X          (0x02U << 6)
#define MCP3465R_CFG2_GAIN_1X           (0x01U << 3)
#define MCP3465R_CFG2_AZ_MUX_EN        (0x01U << 2)   /* Auto-zero MUX */
#define MCP3465R_CFG2_AZ_REF_DIS       (0x00U << 1)

/* ── CONFIG3 Bits ─────────────────────────────────────────────────────── */
#define MCP3465R_CFG3_CONV_MODE_ONE_SHOT    (0x00U << 6)
#define MCP3465R_CFG3_CONV_MODE_CONT        (0x03U << 6)
#define MCP3465R_CFG3_DATA_FORMAT_32BIT_SGN (0x03U << 4)
#define MCP3465R_CFG3_CRC_DIS               (0x00U << 3)

/* ── MUX Register: Channel Selection ──────────────────────────────────── */
#define MCP3465R_MUX_VIN_POS(ch)    ((ch) << 4)     /* VIN+ = CH0..CH3 */
#define MCP3465R_MUX_VIN_NEG_AGND   (0x08U)         /* VIN- = AGND */

/* ── Driver Handle ────────────────────────────────────────────────────── */
typedef struct {
    SPI_HandleTypeDef   *hspi;
    GPIO_TypeDef        *cs_port;
    uint16_t            cs_pin;
    uint16_t            vref_mv;        /* Reference voltage in mV */
} MCP3465R_Handle_t;

/* ── API Functions ────────────────────────────────────────────────────── */

/**
 * @brief Initialize MCP3465R ADC
 * @param dev   Device handle
 * @retval HAL_OK on success
 */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *dev);

/**
 * @brief Read voltage from a single channel (blocking)
 * @param dev       Device handle
 * @param channel   Channel number (0-3)
 * @param voltage_mv Output: measured voltage in millivolts
 * @retval HAL_OK on success
 */
HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_Handle_t *dev,
                                       uint8_t channel,
                                       uint16_t *voltage_mv);

/**
 * @brief Read all 4 channels sequentially
 * @param dev       Device handle
 * @param voltages_mv Output array [4] of measured voltages in mV
 * @retval HAL_OK on success
 */
HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_Handle_t *dev,
                                           uint16_t voltages_mv[4]);

/**
 * @brief Write a register
 */
HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *dev,
                                    uint8_t reg_addr, uint8_t data);

/**
 * @brief Read a register
 */
HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *dev,
                                   uint8_t reg_addr, uint8_t *data);

/**
 * @brief Send a fast command
 */
HAL_StatusTypeDef MCP3465R_FastCommand(MCP3465R_Handle_t *dev, uint8_t cmd);

/**
 * @brief Read raw ADC data (32-bit signed format)
 */
HAL_StatusTypeDef MCP3465R_ReadAdcData(MCP3465R_Handle_t *dev, int32_t *raw_data);

#ifdef __cplusplus
}
#endif

#endif /* __MCP3465R_H */
