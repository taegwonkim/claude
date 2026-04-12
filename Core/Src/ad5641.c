/**
 * @file    ad5641.c
 * @brief   AD5641 14-bit DAC 드라이버 구현
 *
 * 프로토콜:
 *   1. SYNC(CS) LOW
 *   2. 16-bit 전송 MSB 우선: [PD1 | PD0 | D13 | D12 | ... | D0]
 *   3. SYNC(CS) HIGH
 *
 * 전압 공식:
 *   VOUT = (CODE / 2^14) * (Vref * gain)
 */
#include "ad5641.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  내부 함수                                                            */
/* ------------------------------------------------------------------ */
static inline void cs_assert(AD5641_HandleTypeDef *hdev, uint8_t ch)
{
    HAL_GPIO_WritePin(hdev->cs_port[ch], hdev->cs_pin[ch], GPIO_PIN_RESET);
}

static inline void cs_deassert(AD5641_HandleTypeDef *hdev, uint8_t ch)
{
    HAL_GPIO_WritePin(hdev->cs_port[ch], hdev->cs_pin[ch], GPIO_PIN_SET);
}

/* ------------------------------------------------------------------ */
/*  공개 API                                                            */
/* ------------------------------------------------------------------ */

HAL_StatusTypeDef AD5641_Init(AD5641_HandleTypeDef *hdev,
                               SPI_HandleTypeDef    *hspi,
                               float vref, float gain)
{
    if (hdev == NULL || hspi == NULL) return HAL_ERROR;
    if (gain != 1.0f && gain != 2.0f)  return HAL_ERROR;

    hdev->hspi = hspi;
    hdev->vref = vref;
    hdev->gain = gain;

    for (uint8_t i = 0; i < AD5641_MAX_CHANNELS; i++) {
        hdev->cs_port[i]       = NULL;
        hdev->cs_pin[i]        = 0;
        hdev->current_code[i]  = 0;
    }
    return HAL_OK;
}

HAL_StatusTypeDef AD5641_SetCS(AD5641_HandleTypeDef *hdev,
                                uint8_t channel,
                                GPIO_TypeDef *port, uint16_t pin)
{
    if (channel >= AD5641_MAX_CHANNELS) return HAL_ERROR;
    if (port == NULL)                   return HAL_ERROR;

    hdev->cs_port[channel] = port;
    hdev->cs_pin[channel]  = pin;

    /* 초기 비활성 상태 (HIGH) */
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
    return HAL_OK;
}

HAL_StatusTypeDef AD5641_WriteCode(AD5641_HandleTypeDef *hdev,
                                    uint8_t channel, uint16_t code)
{
    if (channel >= AD5641_MAX_CHANNELS)       return HAL_ERROR;
    if (hdev->cs_port[channel] == NULL)       return HAL_ERROR;

    if (code > AD5641_MAX_CODE) code = AD5641_MAX_CODE;

    /* 16-bit 프레임: PD[15:14]=00 (정상동작) + CODE[13:0] */
    uint16_t frame = AD5641_PD_NORMAL | (code & AD5641_MAX_CODE);
    uint8_t  buf[2];
    buf[0] = (uint8_t)((frame >> 8) & 0xFFU);  /* MSB 먼저 */
    buf[1] = (uint8_t)( frame       & 0xFFU);

    cs_assert(hdev, channel);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hdev->hspi, buf, 2U, HAL_MAX_DELAY);
    cs_deassert(hdev, channel);

    if (status == HAL_OK) {
        hdev->current_code[channel] = code;
    }
    return status;
}

HAL_StatusTypeDef AD5641_SetVoltage(AD5641_HandleTypeDef *hdev,
                                     uint8_t channel, float voltage)
{
    uint16_t code = AD5641_VoltageToCode(hdev, voltage);
    return AD5641_WriteCode(hdev, channel, code);
}

HAL_StatusTypeDef AD5641_PowerDown(AD5641_HandleTypeDef *hdev,
                                    uint8_t channel, uint16_t pd_mode)
{
    if (channel >= AD5641_MAX_CHANNELS)  return HAL_ERROR;
    if (hdev->cs_port[channel] == NULL)  return HAL_ERROR;

    /* PD 모드만 담긴 2바이트 전송 (코드 비트는 0) */
    uint16_t frame = pd_mode & 0xC000U;
    uint8_t  buf[2];
    buf[0] = (uint8_t)((frame >> 8) & 0xFFU);
    buf[1] = (uint8_t)( frame       & 0xFFU);

    cs_assert(hdev, channel);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hdev->hspi, buf, 2U, HAL_MAX_DELAY);
    cs_deassert(hdev, channel);
    return status;
}

/* ------------------------------------------------------------------ */
/*  변환 유틸                                                            */
/* ------------------------------------------------------------------ */

float AD5641_CodeToVoltage(AD5641_HandleTypeDef *hdev, uint16_t code)
{
    float vout_max = hdev->vref * hdev->gain;
    return ((float)code / (float)(AD5641_MAX_CODE + 1U)) * vout_max;
}

uint16_t AD5641_VoltageToCode(AD5641_HandleTypeDef *hdev, float voltage)
{
    float vout_max = hdev->vref * hdev->gain;
    if (voltage <= 0.0f)        return 0U;
    if (voltage >= vout_max)    return AD5641_MAX_CODE;

    uint16_t code = (uint16_t)((voltage / vout_max) * (float)(AD5641_MAX_CODE + 1U));
    if (code > AD5641_MAX_CODE) code = AD5641_MAX_CODE;
    return code;
}
