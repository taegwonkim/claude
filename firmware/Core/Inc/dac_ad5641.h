#ifndef __DAC_AD5641_H
#define __DAC_AD5641_H

#include "stm32l5xx_hal.h"
#include <stdint.h>

#define AD5641_NUM_CHANNELS   4
#define AD5641_RESOLUTION     14
#define AD5641_MAX_CODE       ((1 << AD5641_RESOLUTION) - 1)  /* 16383 */
#define AD5641_VREF           5.0f   /* Reference voltage in V */

/* Power-down modes (bits 15:14 of 16-bit word) */
#define AD5641_PD_NORMAL      0x0000
#define AD5641_PD_1K_GND      0x4000
#define AD5641_PD_100K_GND    0x8000
#define AD5641_PD_TRISTATE    0xC000

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port[AD5641_NUM_CHANNELS];
    uint16_t           cs_pin[AD5641_NUM_CHANNELS];
    uint16_t           current_code[AD5641_NUM_CHANNELS];
} AD5641_Handle_t;

/**
 * @brief Initialize AD5641 driver (all outputs to 0V)
 */
void AD5641_Init(AD5641_Handle_t *hdac);

/**
 * @brief Set DAC output code for a specific channel
 * @param ch   Channel index (0-3)
 * @param code 14-bit DAC code (0 ~ 16383)
 * @return HAL_OK on success
 */
HAL_StatusTypeDef AD5641_SetCode(AD5641_Handle_t *hdac, uint8_t ch, uint16_t code);

/**
 * @brief Set DAC output voltage for a specific channel
 * @param ch      Channel index (0-3)
 * @param voltage Desired voltage (0.0 ~ 5.0 V)
 * @return HAL_OK on success
 */
HAL_StatusTypeDef AD5641_SetVoltage(AD5641_Handle_t *hdac, uint8_t ch, float voltage);

/**
 * @brief Convert voltage to 14-bit DAC code
 */
uint16_t AD5641_VoltageToDacCode(float voltage);

/**
 * @brief Convert 14-bit DAC code to voltage
 */
float AD5641_DacCodeToVoltage(uint16_t code);

/**
 * @brief Power down a specific channel
 */
HAL_StatusTypeDef AD5641_PowerDown(AD5641_Handle_t *hdac, uint8_t ch, uint16_t pd_mode);

#endif /* __DAC_AD5641_H */
