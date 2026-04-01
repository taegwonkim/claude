/**
  ******************************************************************************
  * @file           : ad5641.c
  * @brief          : AD5641 14-bit DAC driver implementation
  *
  * AD5641 SPI Frame (16-bit, MSB first):
  *   Bit[15:14] = PD1:PD0 (Power-Down mode, 00 = Normal)
  *   Bit[13:0]  = D13~D0  (14-bit data)
  *
  * Vout = Vref × D / 16384
  * SPI Mode 3 (CPOL=1, CPHA=1), Max 30MHz
  ******************************************************************************
  */

#include "ad5641.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static AD5641_Device_t dac_devices[NUM_CHANNELS];
static osMutexId_t     spi2_mutex = NULL;

/* CS pin configuration table */
static const struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} dac_cs_pins[NUM_CHANNELS] = {
    { DAC_CS1_GPIO_Port, DAC_CS1_Pin },
    { DAC_CS2_GPIO_Port, DAC_CS2_Pin },
    { DAC_CS3_GPIO_Port, DAC_CS3_Pin },
    { DAC_CS4_GPIO_Port, DAC_CS4_Pin },
};

/* Private functions ---------------------------------------------------------*/
static inline void DAC_CS_Low(uint8_t ch)
{
    HAL_GPIO_WritePin(dac_devices[ch].cs_port, dac_devices[ch].cs_pin, GPIO_PIN_RESET);
}

static inline void DAC_CS_High(uint8_t ch)
{
    HAL_GPIO_WritePin(dac_devices[ch].cs_port, dac_devices[ch].cs_pin, GPIO_PIN_SET);
}

static HAL_StatusTypeDef DAC_SPI_Write16(uint8_t ch, uint16_t data)
{
    HAL_StatusTypeDef status;
    uint8_t tx_buf[2];

    tx_buf[0] = (uint8_t)(data >> 8);   /* MSB */
    tx_buf[1] = (uint8_t)(data & 0xFF); /* LSB */

    /* Acquire SPI mutex */
    if (spi2_mutex != NULL) {
        osMutexAcquire(spi2_mutex, osWaitForever);
    }

    DAC_CS_Low(ch);
    status = HAL_SPI_Transmit(dac_devices[ch].hspi, tx_buf, 2, 10);
    DAC_CS_High(ch);

    if (spi2_mutex != NULL) {
        osMutexRelease(spi2_mutex);
    }

    return status;
}

/* Public functions ----------------------------------------------------------*/

void AD5641_Init(AD5641_Device_t *devs, SPI_HandleTypeDef *hspi, osMutexId_t mutex)
{
    spi2_mutex = mutex;

    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        dac_devices[i].hspi    = hspi;
        dac_devices[i].cs_port = dac_cs_pins[i].port;
        dac_devices[i].cs_pin  = dac_cs_pins[i].pin;
        dac_devices[i].current_code = 0;

        /* Ensure CS is high (deselected) */
        DAC_CS_High(i);

        /* Set output to 0V */
        AD5641_SetCode(i, 0);
    }

    /* Copy back to caller if needed */
    if (devs != NULL) {
        memcpy(devs, dac_devices, sizeof(dac_devices));
    }
}

HAL_StatusTypeDef AD5641_SetCode(uint8_t channel, uint16_t code)
{
    if (channel >= NUM_CHANNELS) return HAL_ERROR;
    if (code > (DAC_RESOLUTION - 1)) code = DAC_RESOLUTION - 1;

    /* Frame: [00 | D13..D0] = Normal mode with 14-bit data */
    uint16_t frame = AD5641_MODE_NORMAL | (code & 0x3FFF);

    HAL_StatusTypeDef status = DAC_SPI_Write16(channel, frame);
    if (status == HAL_OK) {
        dac_devices[channel].current_code = code;
    }
    return status;
}

HAL_StatusTypeDef AD5641_SetVoltage(uint8_t channel, float voltage)
{
    if (voltage < VOLTAGE_MIN) voltage = VOLTAGE_MIN;
    if (voltage > VOLTAGE_MAX) voltage = VOLTAGE_MAX;

    uint16_t code = AD5641_VoltageToDacCode(voltage);
    return AD5641_SetCode(channel, code);
}

uint16_t AD5641_GetCode(uint8_t channel)
{
    if (channel >= NUM_CHANNELS) return 0;
    return dac_devices[channel].current_code;
}

HAL_StatusTypeDef AD5641_PowerDown(uint8_t channel, uint16_t mode)
{
    if (channel >= NUM_CHANNELS) return HAL_ERROR;

    uint16_t frame = mode | 0x0000;  /* Power-down mode with 0 data */
    return DAC_SPI_Write16(channel, frame);
}

uint16_t AD5641_VoltageToDacCode(float voltage)
{
    if (voltage <= 0.0f) return 0;
    if (voltage >= VREF_DAC) return DAC_RESOLUTION - 1;

    float code_f = (voltage / VREF_DAC) * (float)DAC_RESOLUTION;
    uint16_t code = (uint16_t)(code_f + 0.5f);  /* Round */
    if (code > (DAC_RESOLUTION - 1)) code = DAC_RESOLUTION - 1;
    return code;
}

float AD5641_DacCodeToVoltage(uint16_t code)
{
    return (VREF_DAC * (float)code) / (float)DAC_RESOLUTION;
}
