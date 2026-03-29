/**
 ******************************************************************************
 * @file    app_types.h
 * @brief   Application data types and structures
 ******************************************************************************
 */
#ifndef __APP_TYPES_H
#define __APP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* ── UART Command Types ───────────────────────────────────────────────── */
typedef enum {
    CMD_SET_VOLTAGE     = 0x01,  /* Set target voltage for a channel */
    CMD_GET_VOLTAGE     = 0x02,  /* Query measured voltage */
    CMD_GET_ALL         = 0x03,  /* Query all channels */
    CMD_GET_CURRENT     = 0x04,  /* Query load current */
    CMD_GET_FAULT       = 0x05,  /* Query fault status */
    CMD_ENABLE_CH       = 0x06,  /* Enable channel output */
    CMD_DISABLE_CH      = 0x07,  /* Disable channel output */
    CMD_SET_PID         = 0x08,  /* Set PI controller gains */
    CMD_SYSTEM_STATUS   = 0x09,  /* Query system status */
} CmdType_t;

/* ── UART Command Structure ──────────────────────────────────────────── */
typedef struct {
    CmdType_t   cmd;
    uint8_t     channel;           /* 0~3 */
    uint16_t    value;             /* mV for voltage, raw for others */
    float       param1;            /* Optional: Kp */
    float       param2;            /* Optional: Ki */
} UartCmd_t;

/* ── Channel State ────────────────────────────────────────────────────── */
typedef enum {
    CH_STATE_DISABLED = 0,
    CH_STATE_ENABLED,
    CH_STATE_FAULT_SHORT,
    CH_STATE_FAULT_OPEN,
} ChannelState_t;

/* ── ADC Data from External ADC (MCP3465R) ────────────────────────────── */
typedef struct {
    uint16_t    voltage_mv[NUM_CHANNELS];      /* Measured voltage per channel (mV) */
    uint32_t    timestamp;                      /* OS tick when sampled */
} AdcData_t;

/* ── Current Measurement Data ─────────────────────────────────────────── */
typedef struct {
    uint16_t    current_ma[NUM_CHANNELS];      /* Load current per channel (mA) */
    uint32_t    timestamp;
} CurrentData_t;

/* ── Fault Event ──────────────────────────────────────────────────────── */
typedef enum {
    FAULT_NONE = 0,
    FAULT_SHORT_CIRCUIT,
    FAULT_OPEN_CIRCUIT,
    FAULT_OVER_VOLTAGE,
    FAULT_CLEARED,
} FaultType_t;

typedef struct {
    uint8_t     channel;
    FaultType_t fault;
    uint16_t    measured_value;                /* mV or mA depending on context */
    uint32_t    timestamp;
} FaultEvent_t;

/* ── Communication Message ────────────────────────────────────────────── */
typedef enum {
    MSG_VOLTAGE_REPORT = 0,
    MSG_CURRENT_REPORT,
    MSG_FAULT_REPORT,
    MSG_STATUS_REPORT,
    MSG_CMD_RESPONSE,
} MsgType_t;

typedef struct {
    MsgType_t   type;
    uint8_t     channel;
    uint16_t    target_mv[NUM_CHANNELS];
    uint16_t    measured_mv[NUM_CHANNELS];
    uint16_t    current_ma[NUM_CHANNELS];
    ChannelState_t state[NUM_CHANNELS];
    uint8_t     data[32];
    uint8_t     data_len;
} CommMsg_t;

/* ── PI Controller State ──────────────────────────────────────────────── */
typedef struct {
    float       kp;
    float       ki;
    float       integral;
    float       output;
    float       error_prev;
    float       output_min;
    float       output_max;
    float       integral_limit;
} PiController_t;

/* ── Per-Channel Voltage Control State ────────────────────────────────── */
typedef struct {
    uint16_t        target_mv;                 /* Desired output voltage (mV) */
    uint16_t        measured_mv;               /* Feedback from MCP3465R (mV) */
    uint16_t        current_ma;                /* Load current (mA) */
    uint16_t        dac_raw;                   /* Current DAC/PWM raw value */
    ChannelState_t  state;
    PiController_t  pi;
    uint8_t         fault_count;               /* Debounce counter */
    bool            enabled;
} ChannelCtrl_t;

#ifdef __cplusplus
}
#endif

#endif /* __APP_TYPES_H */
