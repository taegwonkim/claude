/**
 * DAC80501 16-bit DAC Driver
 * Interface: SPI (Mode 1 — CPOL=0, CPHA=1)
 * MCU: STM32L552 (HAL)
 *
 * Internal reference: 2.5V
 * Output configuration: REF_DIV=0 (no divide), BUFF_GAIN=1 (×2) → 0~5V range
 *
 * NOTE: DAC80501 uses SPI Mode 1 (CPOL=0, CPHA=1), which differs from the
 * MCP3465R (Mode 0).  Use a separate SPI peripheral (hspi2) for this device,
 * or reconfigure hspi1 before each transaction if only one SPI is available.
 */
#ifndef DAC80501_H
#define DAC80501_H

#include "stm32l5xx_hal.h"
#include <stdint.h>

/* ── Hardware configuration ─────────────────────────────────────────────── */
extern SPI_HandleTypeDef hspi2;          /* configured: CPOL=0, CPHA=1, 8-bit */

#define DAC80501_SPI            hspi2
#define DAC80501_CS_GPIO        GPIOA
#define DAC80501_CS_PIN         GPIO_PIN_8
#define DAC80501_SPI_TIMEOUT_MS 10U

/* ── Register addresses (4-bit field in SPI command) ────────────────────── */
#define DAC80501_REG_NOOP       0x00U
#define DAC80501_REG_DEVID      0x01U
#define DAC80501_REG_SYNC       0x02U
#define DAC80501_REG_CONFIG     0x03U
#define DAC80501_REG_GAIN       0x04U
#define DAC80501_REG_TRIGGER    0x05U
#define DAC80501_REG_STATUS     0x07U
#define DAC80501_REG_DAC        0x08U

/* ── GAIN register bits (16-bit) ────────────────────────────────────────── */
/* Bit 8: REF_DIV  — 0=VREF/1 (no divide), 1=VREF/2                       */
/* Bit 0: BUFF_GAIN — 0=×1, 1=×2                                            */
/* For 0~5V: REF_DIV=0, BUFF_GAIN=1 → output = code/65535 × 5.0V          */
#define DAC80501_GAIN_REF_DIV0  0x0000U
#define DAC80501_GAIN_BUF_2X    0x0001U

/* ── CONFIG register bits (16-bit) ─────────────────────────────────────── */
/* Bit 8: DAC_PWDWN — 0=normal, 1=DAC power-down                           */
/* Bit 0: REF_PWDWN — 0=internal ref on, 1=internal ref off                */
#define DAC80501_CFG_DAC_ON     0x0000U
#define DAC80501_CFG_REF_ON     0x0000U

/* ── Output parameters ──────────────────────────────────────────────────── */
#define DAC80501_VOUT_MAX       5.0f     /* volts, with ×2 gain on 2.5V ref */
#define DAC80501_CODE_MAX       65535U   /* 16-bit full scale */

/* ── Public API ─────────────────────────────────────────────────────────── */
HAL_StatusTypeDef DAC80501_Init(void);
HAL_StatusTypeDef DAC80501_SetCode(uint16_t code);
HAL_StatusTypeDef DAC80501_SetVoltage(float voltage_v);
HAL_StatusTypeDef DAC80501_Reset(void);

#endif /* DAC80501_H */
