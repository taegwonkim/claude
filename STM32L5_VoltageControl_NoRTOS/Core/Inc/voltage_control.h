#ifndef VOLTAGE_CONTROL_H
#define VOLTAGE_CONTROL_H

#include "main.h"
#include "mcp3465r.h"
#include <stdint.h>
#include <stdbool.h>

#define VOLTAGE_NUM_CH       4
#define VOLTAGE_MAX_MV       3300
#define VOLTAGE_MIN_MV       0
#define VOLTAGE_PWM_MAX      4095

/* PI 파라미터 */
#define CTRL_KP              5.0f
#define CTRL_KI              0.5f
#define CTRL_INTEGRAL_MAX    2000.0f
#define CTRL_INTEGRAL_MIN   -2000.0f
#define CTRL_DEAD_BAND_MV    5

/* 폴트 임계 */
#define CURRENT_MAX_MA       900
#define CURRENT_FAULT_CNT    3

/* 채널 상태 플래그 */
#define CH_STATUS_OK         0x00
#define CH_STATUS_SHORT      0x01
#define CH_STATUS_OPEN       0x02
#define CH_STATUS_OVERVOLT   0x04
#define CH_STATUS_DISABLED   0x08

typedef struct {
    int32_t  target_mv;
    bool     enabled;
    int32_t  measured_mv;
    int32_t  current_ma;
    float    error_mv;
    float    integral;
    uint16_t pwm_duty;
    uint8_t  fault_flags;
    uint8_t  fault_count;
} VoltageCh_t;

typedef struct {
    VoltageCh_t ch[VOLTAGE_NUM_CH];
    bool        system_enabled;
    uint32_t    ctrl_tick;
} VoltageCtrl_t;

extern TIM_HandleTypeDef htim2;

void  VoltageCtrl_Init(VoltageCtrl_t *hctrl);
void  VoltageCtrl_SetTarget(VoltageCtrl_t *hctrl, uint8_t ch, int32_t mv);
void  VoltageCtrl_Enable(VoltageCtrl_t *hctrl, uint8_t ch, bool en);
void  VoltageCtrl_EnableAll(VoltageCtrl_t *hctrl, bool en);
void  VoltageCtrl_Update(VoltageCtrl_t *hctrl, MCP3465R_Handle_t *hadc);
void  VoltageCtrl_SetPwm(uint8_t ch, uint16_t duty);
void  VoltageCtrl_Reset(VoltageCtrl_t *hctrl, uint8_t ch);

#endif /* VOLTAGE_CONTROL_H */
