/**
  ******************************************************************************
  * @file           : ad5641.h
  * @brief          : AD5641 14-bit DAC driver header (SPI)
  *                   4 channels, each with separate CS pin on SPI2
  ******************************************************************************
  */
#ifndef __AD5641_H
#define __AD5641_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os2.h"

/* AD5641 Power-Down Modes (upper 2 bits of 16-bit frame) */
#define AD5641_MODE_NORMAL      0x0000  /* Normal operation */
#define AD5641_MODE_1K_GND      0x4000  /* 1kΩ to GND */
#define AD5641_MODE_100K_GND    0x8000  /* 100kΩ to GND */
#define AD5641_MODE_HIZ         0xC000  /* Three-state (Hi-Z) */

/* AD5641 device structure */
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    uint16_t           current_code;
} AD5641_Device_t;

/* Initialize AD5641 device array */
void AD5641_Init(AD5641_Device_t *devs, SPI_HandleTypeDef *hspi, osMutexId_t spi_mutex);

/* Set DAC output by raw code (0~16383) */
HAL_StatusTypeDef AD5641_SetCode(uint8_t channel, uint16_t code);

/* Set DAC output by voltage (0.0 ~ 5.0 V) */
HAL_StatusTypeDef AD5641_SetVoltage(uint8_t channel, float voltage);

/* Get current DAC code for a channel */
uint16_t AD5641_GetCode(uint8_t channel);

/* Power down a channel */
HAL_StatusTypeDef AD5641_PowerDown(uint8_t channel, uint16_t mode);

/* Convert voltage to DAC code */
uint16_t AD5641_VoltageToDacCode(float voltage);

/* Convert DAC code to voltage */
float AD5641_DacCodeToVoltage(uint16_t code);

#ifdef __cplusplus
}
#endif

#endif /* __AD5641_H */
