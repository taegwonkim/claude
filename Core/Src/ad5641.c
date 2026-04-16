/**
 * @file    ad5641.c
 * @brief   AD5641 14-bit DAC 드라이버 구현
 */
#include "ad5641.h"

/* ------------------------------------------------------------------ */
/*  내부 유틸                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief  채널 CS 어서트(LOW) 후 16비트 SPI 전송, CS 디어서트(HIGH)
 */
static HAL_StatusTypeDef ad5641_spi_write(AD5641_HandleTypeDef *hdev,
                                           uint8_t  channel,
                                           uint16_t word)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(word >> 8);   /* MSB */
    buf[1] = (uint8_t)(word & 0xFF); /* LSB */

    HAL_GPIO_WritePin(hdev->cs_port[channel],
                      hdev->cs_pin[channel],
                      GPIO_PIN_RESET);   /* /SYNC LOW  */

    HAL_StatusTypeDef ret = HAL_SPI_Transmit(hdev->hspi, buf, 2U, 100U);

    HAL_GPIO_WritePin(hdev->cs_port[channel],
                      hdev->cs_pin[channel],
                      GPIO_PIN_SET);     /* /SYNC HIGH */
    return ret;
}

/* ------------------------------------------------------------------ */
/*  공개 API                                                            */
/* ------------------------------------------------------------------ */

uint16_t AD5641_VoltageToCode(float voltage)
{
    if (voltage < AD5641_VOUT_MIN) voltage = AD5641_VOUT_MIN;
    if (voltage > AD5641_VOUT_MAX) voltage = AD5641_VOUT_MAX;

    uint32_t code = (uint32_t)((voltage / AD5641_VREF) * (float)(1UL << AD5641_RESOLUTION));
    if (code > AD5641_MAX_CODE) code = AD5641_MAX_CODE;
    return (uint16_t)code;
}

float AD5641_CodeToVoltage(uint16_t code)
{
    if (code > AD5641_MAX_CODE) code = AD5641_MAX_CODE;
    return ((float)code / (float)(1UL << AD5641_RESOLUTION)) * AD5641_VREF;
}

HAL_StatusTypeDef AD5641_Init(AD5641_HandleTypeDef *hdev)
{
    HAL_StatusTypeDef ret = HAL_OK;

    /* CS를 먼저 모두 비활성화 */
    for (uint8_t ch = 0; ch < AD5641_NUM_CH; ch++)
    {
        HAL_GPIO_WritePin(hdev->cs_port[ch], hdev->cs_pin[ch], GPIO_PIN_SET);
    }

    /* 모든 채널을 VOUT_MIN 으로 초기화 */
    for (uint8_t ch = 0; ch < AD5641_NUM_CH; ch++)
    {
        HAL_StatusTypeDef r = AD5641_SetVoltage(hdev, ch, AD5641_VOUT_MIN);
        if (r != HAL_OK) ret = r;
    }
    return ret;
}

HAL_StatusTypeDef AD5641_SetCode(AD5641_HandleTypeDef *hdev,
                                  uint8_t  channel,
                                  uint16_t code)
{
    if (channel >= AD5641_NUM_CH) return HAL_ERROR;
    if (code > AD5641_MAX_CODE)   code = AD5641_MAX_CODE;

    /*
     * 16-bit 전송 포맷:
     *  [15:14] PD1:PD0 = 00 (Normal operation)
     *  [13:0 ] D13:D0  = DAC code
     */
    uint16_t word = (uint16_t)((AD5641_PD_NORMAL << 14) | (code & 0x3FFFU));
    return ad5641_spi_write(hdev, channel, word);
}

HAL_StatusTypeDef AD5641_SetVoltage(AD5641_HandleTypeDef *hdev,
                                     uint8_t channel,
                                     float   voltage)
{
    return AD5641_SetCode(hdev, channel, AD5641_VoltageToCode(voltage));
}
