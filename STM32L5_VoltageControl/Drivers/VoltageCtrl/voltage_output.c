/**
 ******************************************************************************
 * @file    voltage_output.c
 * @brief   4-Channel Voltage Output Driver Implementation
 ******************************************************************************
 */
#include "voltage_output.h"
#include "app_config.h"

/* ── Initialization ───────────────────────────────────────────────────── */

HAL_StatusTypeDef VoltageOutput_Init(VoltageOutput_Handle_t *vout)
{
    /* Start DAC Channel 1 (Channel 0 output) */
    HAL_DAC_Start(vout->hdac, DAC_CHANNEL_1);
    HAL_DAC_SetValue(vout->hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);

    /* Start DAC Channel 2 (Channel 1 output) */
    HAL_DAC_Start(vout->hdac, DAC_CHANNEL_2);
    HAL_DAC_SetValue(vout->hdac, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 0);

    /* Start TIM2 PWM (Channel 2 output) */
    HAL_TIM_PWM_Start(vout->htim_ch2, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(vout->htim_ch2, TIM_CHANNEL_1, 0);

    /* Start TIM3 PWM (Channel 3 output) */
    HAL_TIM_PWM_Start(vout->htim_ch3, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(vout->htim_ch3, TIM_CHANNEL_1, 0);

    return HAL_OK;
}

/* ── Set Voltage (mV) ─────────────────────────────────────────────────── */

HAL_StatusTypeDef VoltageOutput_SetVoltage(VoltageOutput_Handle_t *vout,
                                           uint8_t channel, uint16_t mv)
{
    if (channel >= NUM_CHANNELS) return HAL_ERROR;
    if (mv > vout->vref_mv) mv = vout->vref_mv;

    /* Convert mV to 12-bit raw value: raw = (mv * 4095) / VREF_mV */
    uint32_t raw = ((uint32_t)mv * (DAC_RESOLUTION - 1)) / vout->vref_mv;
    return VoltageOutput_SetRaw(vout, channel, (uint16_t)raw);
}

/* ── Set Raw Value ────────────────────────────────────────────────────── */

HAL_StatusTypeDef VoltageOutput_SetRaw(VoltageOutput_Handle_t *vout,
                                       uint8_t channel, uint16_t raw)
{
    if (channel >= NUM_CHANNELS) return HAL_ERROR;
    if (raw > (DAC_RESOLUTION - 1)) raw = DAC_RESOLUTION - 1;

    switch (channel) {
    case 0:
        HAL_DAC_SetValue(vout->hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, raw);
        break;
    case 1:
        HAL_DAC_SetValue(vout->hdac, DAC_CHANNEL_2, DAC_ALIGN_12B_R, raw);
        break;
    case 2:
        __HAL_TIM_SET_COMPARE(vout->htim_ch2, TIM_CHANNEL_1, raw);
        break;
    case 3:
        __HAL_TIM_SET_COMPARE(vout->htim_ch3, TIM_CHANNEL_1, raw);
        break;
    default:
        return HAL_ERROR;
    }

    return HAL_OK;
}

/* ── Get Current Raw Value ────────────────────────────────────────────── */

uint16_t VoltageOutput_GetRaw(VoltageOutput_Handle_t *vout, uint8_t channel)
{
    switch (channel) {
    case 0:
        return (uint16_t)HAL_DAC_GetValue(vout->hdac, DAC_CHANNEL_1);
    case 1:
        return (uint16_t)HAL_DAC_GetValue(vout->hdac, DAC_CHANNEL_2);
    case 2:
        return (uint16_t)__HAL_TIM_GET_COMPARE(vout->htim_ch2, TIM_CHANNEL_1);
    case 3:
        return (uint16_t)__HAL_TIM_GET_COMPARE(vout->htim_ch3, TIM_CHANNEL_1);
    default:
        return 0;
    }
}

/* ── Disable Channel ──────────────────────────────────────────────────── */

HAL_StatusTypeDef VoltageOutput_Disable(VoltageOutput_Handle_t *vout,
                                        uint8_t channel)
{
    return VoltageOutput_SetRaw(vout, channel, 0);
}
