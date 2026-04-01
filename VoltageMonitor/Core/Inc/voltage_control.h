/**
  ******************************************************************************
  * @file           : voltage_control.h
  * @brief          : Voltage feedback control (PI controller) header
  ******************************************************************************
  */
#ifndef __VOLTAGE_CONTROL_H
#define __VOLTAGE_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* PI Controller parameters */
#define PI_KP_DEFAULT           0.5f    /* Proportional gain */
#define PI_KI_DEFAULT           0.1f    /* Integral gain */
#define PI_INTEGRAL_MAX         500.0f  /* Anti-windup limit */
#define PI_OUTPUT_MIN           0       /* Min DAC code */
#define PI_OUTPUT_MAX           16383   /* Max DAC code (14-bit) */

/* Feedback tolerance - considered "matched" if within this range */
#define VOLTAGE_TOLERANCE       0.01f   /* 10mV tolerance */

/* PI controller state per channel */
typedef struct {
    float kp;
    float ki;
    float integral;
    float prev_error;
    float output;          /* Current PI output (DAC code as float) */
} PI_Controller_t;

/* Initialize voltage control for all channels */
void VoltageControl_Init(void);

/* Set target voltage for a channel */
void VoltageControl_SetTarget(uint8_t channel, float voltage);

/* Get target voltage for a channel */
float VoltageControl_GetTarget(uint8_t channel);

/* Run one iteration of PI feedback control for a specific channel */
void VoltageControl_Update(uint8_t channel, float measured_voltage);

/* Run feedback control for all channels (reads ADC, adjusts DAC) */
void VoltageControl_UpdateAll(void);

/* Check if channel output is within tolerance */
uint8_t VoltageControl_IsStable(uint8_t channel);

/* Reset PI controller for a channel */
void VoltageControl_Reset(uint8_t channel);

/* Get current channel data snapshot */
void VoltageControl_GetChannelData(uint8_t channel, ChannelData_t *data);

/* Get all channel data for reporting */
void VoltageControl_GetReportData(ReportData_t *report);

/* Update channel fault status from current monitor */
void VoltageControl_SetFault(uint8_t channel, ChannelStatus_t status);

#ifdef __cplusplus
}
#endif

#endif /* __VOLTAGE_CONTROL_H */
