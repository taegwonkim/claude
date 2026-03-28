#ifndef CURRENT_MONITOR_H
#define CURRENT_MONITOR_H

#include "mcp3465r.h"
#include "voltage_control.h"
#include <stdint.h>
#include <stdbool.h>

#define CURRENT_SHUNT_MOHM      100    /* 0.1Ω = 100mΩ */
#define CURRENT_AMP_GAIN_X10    200    /* INA199A1 × 10 = 20.0V/V */
#define FAULT_SHORT_MA          900
#define FAULT_OPEN_ENABLED      false
#define FAULT_OPEN_MA_MAX       5
#define FAULT_CONFIRM_COUNT     3
#define FAULT_CLEAR_COUNT       10
#define CURRENT_AVG_SAMPLES     8
#define CURRENT_AVG_SHIFT       3

typedef enum { FAULT_NONE=0, FAULT_SHORT, FAULT_OPEN } FaultType_t;

typedef struct {
    int32_t  current_ma;
    int32_t  avg_buf[CURRENT_AVG_SAMPLES];
    uint8_t  avg_idx;
    int32_t  avg_current_ma;
    FaultType_t fault;
    uint8_t  fault_confirm;
    uint8_t  fault_clear;
} CurrentCh_t;

typedef struct {
    CurrentCh_t ch[VOLTAGE_NUM_CH];
    bool        enabled;
    uint32_t    fault_events;
} CurrentMonitor_t;

void        CurrMon_Init(CurrentMonitor_t *hmon);
void        CurrMon_Update(CurrentMonitor_t *hmon,
                           MCP3465R_Handle_t *hadc,
                           VoltageCtrl_t *hctrl);
int32_t     CurrMon_MvToMilliamps(int32_t adc_mv);
const char *CurrMon_FaultStr(FaultType_t f);

#endif /* CURRENT_MONITOR_H */
