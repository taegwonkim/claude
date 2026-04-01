/**
  ******************************************************************************
  * @file           : voltage_control.c
  * @brief          : Voltage feedback control using PI controller
  *
  * Each channel has an independent PI controller that:
  *   1. Reads actual voltage from MCP3465R CH0
  *   2. Compares with target voltage
  *   3. Adjusts DAC output to minimize error
  *   4. Monitors for convergence and stability
  ******************************************************************************
  */

#include "voltage_control.h"
#include "ad5641.h"
#include "mcp3465r.h"
#include "comm_protocol.h"
#include "cmsis_os2.h"
#include <math.h>
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static ChannelData_t   channel_data[NUM_CHANNELS];
static PI_Controller_t pi_ctrl[NUM_CHANNELS];
static osMutexId_t     data_mutex = NULL;

/* Mutex for protecting channel_data access */
static const osMutexAttr_t dataMutexAttr = {
    .name = "DataMutex",
    .attr_bits = osMutexPrioInherit,
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Clamp float value between min and max
  */
static inline float clampf(float val, float min_val, float max_val)
{
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

/**
  * @brief  PI controller update
  * @param  pi: PI controller state
  * @param  setpoint: Target value (voltage)
  * @param  measured: Measured value (voltage)
  * @retval PI output (DAC code as float)
  */
static float PI_Update(PI_Controller_t *pi, float setpoint, float measured)
{
    float error = setpoint - measured;

    /* Integral term with anti-windup */
    pi->integral += pi->ki * error;
    pi->integral = clampf(pi->integral, -(PI_INTEGRAL_MAX), PI_INTEGRAL_MAX);

    /* PI output */
    float output = pi->kp * error + pi->integral;

    /* Add feedforward: expected DAC code for target voltage */
    float feedforward = (setpoint / VREF_DAC) * (float)DAC_RESOLUTION;
    output += feedforward;

    /* Clamp to valid DAC range */
    output = clampf(output, (float)PI_OUTPUT_MIN, (float)PI_OUTPUT_MAX);

    pi->output = output;
    pi->prev_error = error;

    return output;
}

/* Public functions ----------------------------------------------------------*/

void VoltageControl_Init(void)
{
    /* Create data protection mutex */
    data_mutex = osMutexNew(&dataMutexAttr);

    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        /* Initialize channel data */
        channel_data[i].target_voltage = 0.0f;
        channel_data[i].actual_voltage = 0.0f;
        channel_data[i].current_ma     = 0.0f;
        channel_data[i].dac_code       = 0;
        channel_data[i].status         = CH_STATUS_OK;

        /* Initialize PI controllers */
        pi_ctrl[i].kp       = PI_KP_DEFAULT;
        pi_ctrl[i].ki       = PI_KI_DEFAULT;
        pi_ctrl[i].integral = 0.0f;
        pi_ctrl[i].prev_error = 0.0f;
        pi_ctrl[i].output   = 0.0f;
    }
}

void VoltageControl_SetTarget(uint8_t channel, float voltage)
{
    if (channel >= NUM_CHANNELS) return;

    voltage = clampf(voltage, VOLTAGE_MIN, VOLTAGE_MAX);

    osMutexAcquire(data_mutex, osWaitForever);
    channel_data[channel].target_voltage = voltage;
    osMutexRelease(data_mutex);

    /* Reset integral when target changes to avoid windup carry-over */
    pi_ctrl[channel].integral = 0.0f;

    Comm_SendDebug("[CTRL] CH%d target set to %.3fV\r\n", channel, voltage);
}

float VoltageControl_GetTarget(uint8_t channel)
{
    if (channel >= NUM_CHANNELS) return 0.0f;

    osMutexAcquire(data_mutex, osWaitForever);
    float target = channel_data[channel].target_voltage;
    osMutexRelease(data_mutex);

    return target;
}

void VoltageControl_Update(uint8_t channel, float measured_voltage)
{
    if (channel >= NUM_CHANNELS) return;

    osMutexAcquire(data_mutex, osWaitForever);
    float target = channel_data[channel].target_voltage;
    ChannelStatus_t current_status = channel_data[channel].status;
    osMutexRelease(data_mutex);

    /* Skip PI control if channel has fault */
    if (current_status == CH_STATUS_SHORT) {
        /* Short circuit: set DAC to 0 for safety */
        AD5641_SetCode(channel, 0);
        osMutexAcquire(data_mutex, osWaitForever);
        channel_data[channel].actual_voltage = measured_voltage;
        channel_data[channel].dac_code = 0;
        osMutexRelease(data_mutex);
        return;
    }

    /* Run PI controller */
    float pi_output = PI_Update(&pi_ctrl[channel], target, measured_voltage);
    uint16_t dac_code = (uint16_t)(pi_output + 0.5f);

    /* Apply DAC output */
    AD5641_SetCode(channel, dac_code);

    /* Update channel data */
    osMutexAcquire(data_mutex, osWaitForever);
    channel_data[channel].actual_voltage = measured_voltage;
    channel_data[channel].dac_code = dac_code;
    osMutexRelease(data_mutex);
}

void VoltageControl_UpdateAll(void)
{
    int32_t raw_value;

    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
        /* Read voltage from ADC CH0 */
        if (MCP3465R_ReadChannel(ch, MCP3465R_CH_VOLTAGE, &raw_value) == HAL_OK) {
            float measured_v = MCP3465R_RawToVoltage(raw_value);
            VoltageControl_Update(ch, measured_v);
        }
    }
}

uint8_t VoltageControl_IsStable(uint8_t channel)
{
    if (channel >= NUM_CHANNELS) return 0;

    osMutexAcquire(data_mutex, osWaitForever);
    float error = fabsf(channel_data[channel].target_voltage -
                        channel_data[channel].actual_voltage);
    osMutexRelease(data_mutex);

    return (error <= VOLTAGE_TOLERANCE) ? 1 : 0;
}

void VoltageControl_Reset(uint8_t channel)
{
    if (channel >= NUM_CHANNELS) return;

    pi_ctrl[channel].integral   = 0.0f;
    pi_ctrl[channel].prev_error = 0.0f;
    pi_ctrl[channel].output     = 0.0f;

    osMutexAcquire(data_mutex, osWaitForever);
    channel_data[channel].target_voltage = 0.0f;
    channel_data[channel].actual_voltage = 0.0f;
    channel_data[channel].dac_code       = 0;
    channel_data[channel].status         = CH_STATUS_OK;
    osMutexRelease(data_mutex);

    AD5641_SetCode(channel, 0);
}

void VoltageControl_GetChannelData(uint8_t channel, ChannelData_t *data)
{
    if (channel >= NUM_CHANNELS || data == NULL) return;

    osMutexAcquire(data_mutex, osWaitForever);
    *data = channel_data[channel];
    osMutexRelease(data_mutex);
}

void VoltageControl_GetReportData(ReportData_t *report)
{
    if (report == NULL) return;

    osMutexAcquire(data_mutex, osWaitForever);
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        report->channels[i] = channel_data[i];
    }
    osMutexRelease(data_mutex);

    report->timestamp_ms = HAL_GetTick();
}

void VoltageControl_SetFault(uint8_t channel, ChannelStatus_t status)
{
    if (channel >= NUM_CHANNELS) return;

    osMutexAcquire(data_mutex, osWaitForever);
    channel_data[channel].status = status;
    osMutexRelease(data_mutex);
}
