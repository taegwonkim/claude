#include "voltage_control.h"
#include <math.h>

void VoltCtrl_Init(VoltCtrl_Handle_t *hctrl, AD5641_Handle_t *hdac,
                   MCP3465R_Handle_t *hadc[VCTRL_NUM_CHANNELS])
{
    hctrl->hdac = hdac;
    for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
        hctrl->hadc[ch] = hadc[ch];
    }

    for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
        hctrl->channels[ch].target_voltage = 0.0f;
        hctrl->channels[ch].actual_voltage = 0.0f;
        hctrl->channels[ch].current_mA = 0.0f;
        hctrl->channels[ch].dac_code = 0;
        hctrl->channels[ch].state = CH_STATE_DISABLED;
        hctrl->channels[ch].enabled = false;
        hctrl->channels[ch].offset_code = 0;
    }
}

HAL_StatusTypeDef VoltCtrl_SetTarget(VoltCtrl_Handle_t *hctrl, uint8_t ch, float voltage)
{
    if (ch >= VCTRL_NUM_CHANNELS) return HAL_ERROR;
    if (voltage < VCTRL_VOLTAGE_MIN) voltage = VCTRL_VOLTAGE_MIN;
    if (voltage > VCTRL_VOLTAGE_MAX) voltage = VCTRL_VOLTAGE_MAX;

    hctrl->channels[ch].target_voltage = voltage;
    hctrl->channels[ch].enabled = true;
    hctrl->channels[ch].state = CH_STATE_OK;

    /* Set initial DAC code from target voltage */
    uint16_t code = AD5641_VoltageToDacCode(voltage);
    int32_t adjusted = (int32_t)code + hctrl->channels[ch].offset_code;
    if (adjusted < 0) adjusted = 0;
    if (adjusted > AD5641_MAX_CODE) adjusted = AD5641_MAX_CODE;

    hctrl->channels[ch].dac_code = (uint16_t)adjusted;
    return AD5641_SetCode(hctrl->hdac, ch, hctrl->channels[ch].dac_code);
}

void VoltCtrl_EnableChannel(VoltCtrl_Handle_t *hctrl, uint8_t ch, bool enable)
{
    if (ch >= VCTRL_NUM_CHANNELS) return;

    hctrl->channels[ch].enabled = enable;

    if (!enable) {
        hctrl->channels[ch].state = CH_STATE_DISABLED;
        hctrl->channels[ch].target_voltage = 0.0f;
        hctrl->channels[ch].dac_code = 0;
        AD5641_SetCode(hctrl->hdac, ch, 0);
    }
}

void VoltCtrl_ControlLoop(VoltCtrl_Handle_t *hctrl)
{
    for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
        ChannelData_t *cd = &hctrl->channels[ch];

        if (!cd->enabled || cd->state == CH_STATE_DISABLED) continue;

        /* Skip control if fault detected */
        if (cd->state == CH_STATE_SHORT_CIRCUIT) continue;

        /* 1. Read actual voltage from this channel's dedicated ADC (CH0) */
        float measured_v = 0.0f;
        if (MCP3465R_ReadVoltage(hctrl->hadc[ch], &measured_v) == HAL_OK) {
            cd->actual_voltage = measured_v;
        } else {
            continue;  /* Skip this channel on ADC error */
        }

        /* 2. Read current from this channel's dedicated ADC (CH1) */
        float measured_i = 0.0f;
        if (MCP3465R_ReadCurrent(hctrl->hadc[ch], &measured_i,
                                  VCTRL_SHUNT_RESISTOR_OHM) == HAL_OK) {
            cd->current_mA = measured_i;
        }

        /* 3. Calculate error */
        float error_v = cd->target_voltage - cd->actual_voltage;
        float error_mv = error_v * 1000.0f;

        /* 4. If error exceeds tolerance, adjust DAC code */
        if (fabsf(error_mv) > VCTRL_TOLERANCE_MV) {
            /*
             * Proportional correction:
             *   1 DAC LSB = VREF / 16383 = ~0.305 mV
             *   correction_codes = error_mV / 0.305
             *
             * We apply a scaled correction with limiting to prevent oscillation.
             */
            float lsb_mv = (AD5641_VREF * 1000.0f) / (float)AD5641_MAX_CODE;
            int32_t correction = (int32_t)(error_mv / lsb_mv);

            /* Limit correction step size */
            if (correction > VCTRL_MAX_CORRECTION) {
                correction = VCTRL_MAX_CORRECTION;
            } else if (correction < -VCTRL_MAX_CORRECTION) {
                correction = -VCTRL_MAX_CORRECTION;
            }

            /* Apply correction */
            int32_t new_code = (int32_t)cd->dac_code + correction;
            if (new_code < 0) new_code = 0;
            if (new_code > AD5641_MAX_CODE) new_code = AD5641_MAX_CODE;

            cd->dac_code = (uint16_t)new_code;

            /* Update DAC output */
            AD5641_SetCode(hctrl->hdac, ch, cd->dac_code);

            /* Update calibration offset for future reference */
            uint16_t ideal_code = AD5641_VoltageToDacCode(cd->target_voltage);
            cd->offset_code = (int16_t)((int32_t)cd->dac_code - (int32_t)ideal_code);
        }
    }
}

void VoltCtrl_CheckFaults(VoltCtrl_Handle_t *hctrl)
{
    for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
        ChannelData_t *cd = &hctrl->channels[ch];

        if (!cd->enabled || cd->state == CH_STATE_DISABLED) continue;

        /* Short circuit detection: current too high */
        if (cd->current_mA > VCTRL_SHORT_CIRCUIT_MA) {
            cd->state = CH_STATE_SHORT_CIRCUIT;
            /* Safety: set DAC output to 0 */
            cd->dac_code = 0;
            AD5641_SetCode(hctrl->hdac, ch, 0);
            continue;
        }

        /* Open circuit detection: voltage set but negligible current */
        if (cd->target_voltage > 0.5f && cd->current_mA < VCTRL_OPEN_CIRCUIT_MA) {
            cd->state = CH_STATE_OPEN_CIRCUIT;
            continue;
        }

        /* If previously faulted but now OK, clear fault */
        if (cd->state == CH_STATE_OPEN_CIRCUIT || cd->state == CH_STATE_SHORT_CIRCUIT) {
            /* Only clear if conditions are normal now */
            if (cd->current_mA <= VCTRL_SHORT_CIRCUIT_MA && cd->current_mA >= VCTRL_OPEN_CIRCUIT_MA) {
                cd->state = CH_STATE_OK;
            }
        }
    }
}

void VoltCtrl_GetChannelData(VoltCtrl_Handle_t *hctrl, uint8_t ch, ChannelData_t *data)
{
    if (ch >= VCTRL_NUM_CHANNELS) return;
    *data = hctrl->channels[ch];
}

void VoltCtrl_GetAllChannelData(VoltCtrl_Handle_t *hctrl, ChannelData_t data[VCTRL_NUM_CHANNELS])
{
    for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
        data[i] = hctrl->channels[i];
    }
}
