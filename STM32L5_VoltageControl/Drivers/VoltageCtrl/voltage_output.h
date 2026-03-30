/**
 ******************************************************************************
 * @file    voltage_output.h
 * @brief   4-Channel Voltage Output Driver (DAC + PWM)
 ******************************************************************************
 * Channels 0-1: Internal DAC (DAC1 CH1, CH2)
 * Channels 2-3: Timer PWM + external RC filter (TIM2_CH1, TIM3_CH1)
 ******************************************************************************
 */
#ifndef __VOLTAGE_OUTPUT_H
#define __VOLTAGE_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"
#include <stdint.h>

/* ── Handle ───────────────────────────────────────────────────────────── */
typedef struct {
    DAC_HandleTypeDef   *hdac;
    TIM_HandleTypeDef   *htim_ch2;      /* TIM2 for channel 2 PWM */
    TIM_HandleTypeDef   *htim_ch3;      /* TIM3 for channel 3 PWM */
    uint16_t            vref_mv;        /* Reference voltage (mV) */
} VoltageOutput_Handle_t;

/* ── API ──────────────────────────────────────────────────────────────── */

/**
 * @brief Initialize all voltage output channels
 */
HAL_StatusTypeDef VoltageOutput_Init(VoltageOutput_Handle_t *vout);

/**
 * @brief Set voltage for a specific channel
 * @param channel  0-3
 * @param mv       Target voltage in millivolts (0 ~ VREF)
 */
HAL_StatusTypeDef VoltageOutput_SetVoltage(VoltageOutput_Handle_t *vout,
                                           uint8_t channel, uint16_t mv);

/**
 * @brief Set raw DAC/PWM value for a specific channel
 * @param channel  0-3
 * @param raw      Raw 12-bit value (0-4095)
 */
HAL_StatusTypeDef VoltageOutput_SetRaw(VoltageOutput_Handle_t *vout,
                                       uint8_t channel, uint16_t raw);

/**
 * @brief Get current raw output value for a channel
 */
uint16_t VoltageOutput_GetRaw(VoltageOutput_Handle_t *vout, uint8_t channel);

/**
 * @brief Disable output (set to 0V)
 */
HAL_StatusTypeDef VoltageOutput_Disable(VoltageOutput_Handle_t *vout,
                                        uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* __VOLTAGE_OUTPUT_H */
