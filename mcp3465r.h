/*
 * mcp3465r.h – STM32L5 HAL driver for MCP3465R (24-bit delta-sigma ADC)
 *
 * MCP3465R CONFIG registers (CONFIG0–CONFIG3, MUX) are global to the chip.
 * Per-channel configuration is achieved by reconfiguring OSR, Gain, and MUX
 * before each conversion via MCP3465R_ReadChannel().
 *
 * SPI requirements:
 *   - Mode 0,0 (CPOL=0, CPHA=0) or Mode 1,1 (CPOL=1, CPHA=1)
 *   - Max clock: 20 MHz
 *   - 8-bit data size, MSB first
 */

#ifndef MCP3465R_H
#define MCP3465R_H

#include "stm32l5xx_hal.h"
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Oversampling ratio (CONFIG1 bits [5:2])
 * Higher OSR → lower noise, slower conversion
 * ------------------------------------------------------------------------- */
typedef enum {
    MCP3465R_OSR_32    = 0x00,
    MCP3465R_OSR_64    = 0x01,
    MCP3465R_OSR_128   = 0x02,
    MCP3465R_OSR_256   = 0x03,
    MCP3465R_OSR_512   = 0x04,
    MCP3465R_OSR_1024  = 0x05,
    MCP3465R_OSR_2048  = 0x06,
    MCP3465R_OSR_4096  = 0x07,
    MCP3465R_OSR_8192  = 0x08,
    MCP3465R_OSR_16384 = 0x09,
} MCP3465R_OSR;

/* -------------------------------------------------------------------------
 * PGA gain (CONFIG2 bits [5:3])
 * ------------------------------------------------------------------------- */
typedef enum {
    MCP3465R_GAIN_1_3 = 0x00,   /* 1/3x – use when VREF < input range */
    MCP3465R_GAIN_1   = 0x01,   /* 1x   */
    MCP3465R_GAIN_2   = 0x02,   /* 2x   */
    MCP3465R_GAIN_4   = 0x03,   /* 4x   */
    MCP3465R_GAIN_8   = 0x04,   /* 8x   */
    MCP3465R_GAIN_16  = 0x05,   /* 16x  */
    MCP3465R_GAIN_32  = 0x06,   /* 32x  */
} MCP3465R_Gain;

/* -------------------------------------------------------------------------
 * MUX input channel selection (MUX register bits [7:4] / [3:0])
 * ------------------------------------------------------------------------- */
typedef enum {
    MCP3465R_MUX_CH0  = 0x00,
    MCP3465R_MUX_CH1  = 0x01,
    MCP3465R_MUX_CH2  = 0x02,
    MCP3465R_MUX_CH3  = 0x03,
    MCP3465R_MUX_CH4  = 0x04,
    MCP3465R_MUX_CH5  = 0x05,
    MCP3465R_MUX_CH6  = 0x06,
    MCP3465R_MUX_CH7  = 0x07,
    MCP3465R_MUX_AGND = 0x08,
    MCP3465R_MUX_AVDD = 0x09,
    MCP3465R_MUX_TEMP_P = 0x0C,
    MCP3465R_MUX_TEMP_M = 0x0D,
} MCP3465R_Mux;

/* -------------------------------------------------------------------------
 * Per-channel configuration
 *
 * Example: single-ended CH0, high accuracy
 *   .vin_pos = MCP3465R_MUX_CH0, .vin_neg = MCP3465R_MUX_AGND,
 *   .osr = MCP3465R_OSR_4096, .gain = MCP3465R_GAIN_1
 *
 * Example: differential CH0–CH1, high gain
 *   .vin_pos = MCP3465R_MUX_CH0, .vin_neg = MCP3465R_MUX_CH1,
 *   .osr = MCP3465R_OSR_1024, .gain = MCP3465R_GAIN_16
 * ------------------------------------------------------------------------- */
typedef struct {
    MCP3465R_Mux  vin_pos;
    MCP3465R_Mux  vin_neg;
    MCP3465R_OSR  osr;
    MCP3465R_Gain gain;
} MCP3465R_ChConfig;

/* -------------------------------------------------------------------------
 * Driver handle – one instance per physical MCP3465R device
 * ------------------------------------------------------------------------- */
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
} MCP3465R_Handle;

/* -------------------------------------------------------------------------
 * API
 *
 * MCP3465R_Init         – reset chip and apply base configuration
 * MCP3465R_ReadChannel  – apply cfg, trigger one-shot conversion, return result
 *
 * result: 24-bit signed ADC value stored in int32_t
 *   range: -8388608 to +8388607
 *   voltage = ((float)result / 8388608.0f) * VREF / gain
 * ------------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle *dev);

HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_Handle *dev,
                                        const MCP3465R_ChConfig *cfg,
                                        int32_t *result);

#endif /* MCP3465R_H */
