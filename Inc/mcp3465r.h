/**
 * MCP3465R 16/24-bit Delta-Sigma ADC Driver
 * Interface: SPI (Mode 0,0 — CPOL=0, CPHA=0)
 * MCU: STM32L552 (HAL)
 *
 * Device address default: 0x01 (A1=0, A0=1 pins)
 * VREF: external 5.0V → full-scale = ±5.0V differential (single-ended 0~5V)
 * Data format: 32-bit with channel ID + 24-bit signed ADC data
 */
#ifndef MCP3465R_H
#define MCP3465R_H

#include "stm32l5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Hardware configuration ─────────────────────────────────────────────── */
extern SPI_HandleTypeDef hspi1;          /* configured: CPOL=0, CPHA=0, 8-bit */

#define MCP3465R_SPI            hspi1
#define MCP3465R_CS_GPIO        GPIOA
#define MCP3465R_CS_PIN         GPIO_PIN_4
#define MCP3465R_SPI_TIMEOUT_MS 10U

/* ── Device address (set by A1,A0 pins) ─────────────────────────────────── */
#define MCP3465R_DEV_ADDR       0x01U    /* 0b01 → bits [7:6] of command byte */

/* ── Command byte: [ADDR(7:6) | REG(5:2) | TYPE(1:0)] ───────────────────── */
#define MCP3465R_CMD(reg, type) (uint8_t)(((MCP3465R_DEV_ADDR) << 6) | \
                                          ((reg) << 2) | (type))

/* Command types */
#define MCP3465R_TYPE_FAST      0x00U
#define MCP3465R_TYPE_SREAD     0x01U    /* static read */
#define MCP3465R_TYPE_IWRITE    0x02U    /* incremental write */
#define MCP3465R_TYPE_IREAD     0x03U    /* incremental read */

/* Fast commands (REG field encodes the command) */
#define MCP3465R_FCMD_START     0x08U    /* conversion start/restart */
#define MCP3465R_FCMD_STANDBY   0x0AU
#define MCP3465R_FCMD_RESET     0x0EU    /* full device reset */

/* ── Register addresses ─────────────────────────────────────────────────── */
#define MCP3465R_REG_ADCDATA    0x00U
#define MCP3465R_REG_CONFIG0    0x01U
#define MCP3465R_REG_CONFIG1    0x02U
#define MCP3465R_REG_CONFIG2    0x03U
#define MCP3465R_REG_CONFIG3    0x04U
#define MCP3465R_REG_IRQ        0x05U
#define MCP3465R_REG_MUX        0x06U
#define MCP3465R_REG_SCAN       0x07U
#define MCP3465R_REG_TIMER      0x08U
#define MCP3465R_REG_OFFSETCAL  0x09U
#define MCP3465R_REG_GAINCAL    0x0AU

/* ── CONFIG0 bits ───────────────────────────────────────────────────────── */
/* VREF_SEL[7:6]: 00=external VREF, 10=internal 2.4V, 11=AVDD              */
#define MCP3465R_CFG0_VREF_EXT  0x00U
#define MCP3465R_CFG0_VREF_INT  0x80U
/* CLK_SEL[5:4]: 00=ext digital, 10=ext analog, 11=internal oscillator      */
#define MCP3465R_CFG0_CLK_INT   0x30U
/* CS_SEL[3:2]: 00=no bias current                                           */
#define MCP3465R_CFG0_CS_NONE   0x00U
/* ADC_MODE[1:0]: 11=continuous conversion                                   */
#define MCP3465R_CFG0_MODE_CONT 0x03U

/* ── CONFIG1 bits ───────────────────────────────────────────────────────── */
/* PRE[7:6]: AMCLK prescaler; OSR[5:2]: oversampling ratio                  */
/* OSR=0101 → 1024 samples (good noise rejection for calibration)           */
#define MCP3465R_CFG1_PRE_1     0x00U
#define MCP3465R_CFG1_OSR_1024  0x14U   /* 0b0101 << 2 */

/* ── CONFIG2 bits ───────────────────────────────────────────────────────── */
/* BOOST[7:6]: 10=1× speed; GAIN[5:3]: 001=1V/V; AZ_MUX[2]: 0=off         */
#define MCP3465R_CFG2_BOOST_1X  0x80U
#define MCP3465R_CFG2_GAIN_1    0x08U   /* 1V/V, keep VREF=5V range intact */
#define MCP3465R_CFG2_AZMUX_OFF 0x00U

/* ── CONFIG3 bits ───────────────────────────────────────────────────────── */
/* CONV_MODE[7:6]: 11=continuous; DATA_FORMAT[5:4]: 10=32-bit+CH_ID+24-bit */
/* EN_GAINCAL[1]: enable gain cal register; EN_OFFCAL[0]: enable offset cal */
#define MCP3465R_CFG3_CONV_CONT 0xC0U
#define MCP3465R_CFG3_FMT_32CH  0x20U   /* 32-bit: [31:28]=CH_ID [23:0]=data */
#define MCP3465R_CFG3_EN_GCAL   0x02U
#define MCP3465R_CFG3_EN_OCAL   0x01U

/* ── MUX register ───────────────────────────────────────────────────────── */
/* VIN+[7:4] | VIN-[3:0] ; 0b0000=CH0, 0b1000=AGND                        */
#define MCP3465R_MUX_CH0_GND    0x08U   /* CH0 vs AGND (single-ended) */

/* ── Calibration constants ──────────────────────────────────────────────── */
#define MCP3465R_GAINCAL_UNITY  0x800000UL  /* 2^23, represents ×1.000 */
#define MCP3465R_FULL_SCALE_24  8388607L    /* 2^23 - 1 */

/* ── Public API ─────────────────────────────────────────────────────────── */
HAL_StatusTypeDef MCP3465R_Init(void);
HAL_StatusTypeDef MCP3465R_Reset(void);
HAL_StatusTypeDef MCP3465R_SetCalibrationEnable(bool gain_en, bool offset_en);
HAL_StatusTypeDef MCP3465R_ReadRaw24(int32_t *out_code);
HAL_StatusTypeDef MCP3465R_ReadAverage(uint16_t n_samples, int32_t *out_avg);
HAL_StatusTypeDef MCP3465R_WriteOffsetCal(int32_t offset_24);
HAL_StatusTypeDef MCP3465R_WriteGainCal(uint32_t gain_24);
HAL_StatusTypeDef MCP3465R_ReadOffsetCal(int32_t *offset_24);
HAL_StatusTypeDef MCP3465R_ReadGainCal(uint32_t *gain_24);
float             MCP3465R_Code24ToVoltage(int32_t code, float vref);

#endif /* MCP3465R_H */
