#include "dac_ad5641.h"

void AD5641_Init(AD5641_Handle_t *hdac)
{
    /* Deselect all DAC chips (CS HIGH) */
    for (uint8_t i = 0; i < AD5641_NUM_CHANNELS; i++) {
        HAL_GPIO_WritePin(hdac->cs_port[i], hdac->cs_pin[i], GPIO_PIN_SET);
        hdac->current_code[i] = 0;
    }

    /* Set all DACs to 0V output */
    for (uint8_t i = 0; i < AD5641_NUM_CHANNELS; i++) {
        AD5641_SetCode(hdac, i, 0);
    }
}

HAL_StatusTypeDef AD5641_SetCode(AD5641_Handle_t *hdac, uint8_t ch, uint16_t code)
{
    if (ch >= AD5641_NUM_CHANNELS) return HAL_ERROR;
    if (code > AD5641_MAX_CODE) code = AD5641_MAX_CODE;

    /*
     * AD5641 16-bit input register format:
     *   Bit [15:14] = Power-down mode (00 = normal operation)
     *   Bit [13:0]  = 14-bit DAC data
     *
     * Data is clocked MSB first. Latched on CS rising edge.
     */
    uint16_t spi_word = AD5641_PD_NORMAL | (code & 0x3FFF);

    uint8_t tx_data[2];
    tx_data[0] = (uint8_t)(spi_word >> 8);    /* MSB */
    tx_data[1] = (uint8_t)(spi_word & 0xFF);  /* LSB */

    /* Assert CS (LOW) */
    HAL_GPIO_WritePin(hdac->cs_port[ch], hdac->cs_pin[ch], GPIO_PIN_RESET);

    HAL_StatusTypeDef status = HAL_SPI_Transmit(hdac->hspi, tx_data, 2, 10);

    /* De-assert CS (HIGH) -> data latched */
    HAL_GPIO_WritePin(hdac->cs_port[ch], hdac->cs_pin[ch], GPIO_PIN_SET);

    if (status == HAL_OK) {
        hdac->current_code[ch] = code;
    }

    return status;
}

HAL_StatusTypeDef AD5641_SetVoltage(AD5641_Handle_t *hdac, uint8_t ch, float voltage)
{
    uint16_t code = AD5641_VoltageToDacCode(voltage);
    return AD5641_SetCode(hdac, ch, code);
}

uint16_t AD5641_VoltageToDacCode(float voltage)
{
    if (voltage <= 0.0f) return 0;
    if (voltage >= AD5641_VREF) return AD5641_MAX_CODE;

    return (uint16_t)((voltage / AD5641_VREF) * (float)AD5641_MAX_CODE + 0.5f);
}

float AD5641_DacCodeToVoltage(uint16_t code)
{
    if (code > AD5641_MAX_CODE) code = AD5641_MAX_CODE;
    return ((float)code / (float)AD5641_MAX_CODE) * AD5641_VREF;
}

HAL_StatusTypeDef AD5641_PowerDown(AD5641_Handle_t *hdac, uint8_t ch, uint16_t pd_mode)
{
    if (ch >= AD5641_NUM_CHANNELS) return HAL_ERROR;

    uint16_t spi_word = pd_mode | (hdac->current_code[ch] & 0x3FFF);

    uint8_t tx_data[2];
    tx_data[0] = (uint8_t)(spi_word >> 8);
    tx_data[1] = (uint8_t)(spi_word & 0xFF);

    HAL_GPIO_WritePin(hdac->cs_port[ch], hdac->cs_pin[ch], GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hdac->hspi, tx_data, 2, 10);
    HAL_GPIO_WritePin(hdac->cs_port[ch], hdac->cs_pin[ch], GPIO_PIN_SET);

    return status;
}
