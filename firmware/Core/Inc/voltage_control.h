#ifndef __VOLTAGE_CONTROL_H
#define __VOLTAGE_CONTROL_H

#include "stm32l5xx_hal.h"
#include "dac_ad5641.h"
#include "adc_mcp3465r.h"
#include <stdint.h>
#include <stdbool.h>

#define VCTRL_NUM_CHANNELS       4
#define VCTRL_VOLTAGE_MIN        0.0f
#define VCTRL_VOLTAGE_MAX        5.0f
#define VCTRL_TOLERANCE_MV       20.0f    /* Acceptable error: 20mV */
#define VCTRL_MAX_CORRECTION     50       /* Max DAC code correction per iteration */

/* Fault thresholds */
#define VCTRL_SHORT_CIRCUIT_MA   500.0f   /* Current above this = short circuit */
#define VCTRL_OPEN_CIRCUIT_MA    0.1f     /* Current below this (with voltage set) = open */
#define VCTRL_SHUNT_RESISTOR_OHM 0.1f    /* 100 mOhm shunt resistor */

typedef enum {
    CH_STATE_OK = 0,
    CH_STATE_SHORT_CIRCUIT,
    CH_STATE_OPEN_CIRCUIT,
    CH_STATE_DISABLED,
    CH_STATE_CALIBRATING
} ChannelState_t;

typedef struct {
    float    target_voltage;       /* Desired output voltage (V) */
    float    actual_voltage;       /* Measured voltage (V) */
    float    current_mA;           /* Measured current (mA) */
    uint16_t dac_code;             /* Current DAC code */
    ChannelState_t state;          /* Channel fault state */
    bool     enabled;              /* Channel enable flag */
    int16_t  offset_code;          /* Calibration offset for DAC */
} ChannelData_t;

typedef struct {
    AD5641_Handle_t    *hdac;
    MCP3465R_Handle_t  *hadc[VCTRL_NUM_CHANNELS];  /* One ADC per channel */
    ChannelData_t       channels[VCTRL_NUM_CHANNELS];
} VoltCtrl_Handle_t;

/**
 * @brief Initialize voltage control system
 * @param hadc Array of 4 MCP3465R handles (one per channel)
 */
void VoltCtrl_Init(VoltCtrl_Handle_t *hctrl, AD5641_Handle_t *hdac,
                   MCP3465R_Handle_t *hadc[VCTRL_NUM_CHANNELS]);

/**
 * @brief Set target voltage for a channel
 */
HAL_StatusTypeDef VoltCtrl_SetTarget(VoltCtrl_Handle_t *hctrl, uint8_t ch, float voltage);

/**
 * @brief Enable/disable a channel
 */
void VoltCtrl_EnableChannel(VoltCtrl_Handle_t *hctrl, uint8_t ch, bool enable);

/**
 * @brief Execute one control loop iteration (call periodically)
 * - Reads actual voltage from ADC
 * - Compares with target
 * - Adjusts DAC code to minimize error
 */
void VoltCtrl_ControlLoop(VoltCtrl_Handle_t *hctrl);

/**
 * @brief Check for fault conditions on all channels
 */
void VoltCtrl_CheckFaults(VoltCtrl_Handle_t *hctrl);

/**
 * @brief Get channel data (thread-safe copy)
 */
void VoltCtrl_GetChannelData(VoltCtrl_Handle_t *hctrl, uint8_t ch, ChannelData_t *data);

/**
 * @brief Get all channels data
 */
void VoltCtrl_GetAllChannelData(VoltCtrl_Handle_t *hctrl, ChannelData_t data[VCTRL_NUM_CHANNELS]);

#endif /* __VOLTAGE_CONTROL_H */
