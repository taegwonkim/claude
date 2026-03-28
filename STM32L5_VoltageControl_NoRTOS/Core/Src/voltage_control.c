/* =========================================================
 * 전압 PI 제어기 - Non-RTOS (RTOS 버전과 로직 동일)
 * =========================================================*/
#include "voltage_control.h"
#include <string.h>

static const uint32_t pwm_ch_map[VOLTAGE_NUM_CH] = {
    TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
};

void VoltageCtrl_Init(VoltageCtrl_t *hctrl)
{
    memset(hctrl, 0, sizeof(VoltageCtrl_t));
    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        hctrl->ch[i].fault_flags = CH_STATUS_DISABLED;
    }
    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        HAL_TIM_PWM_Start(&htim2, pwm_ch_map[i]);
        VoltageCtrl_SetPwm(i, 0);
    }
}

void VoltageCtrl_SetTarget(VoltageCtrl_t *hctrl, uint8_t ch, int32_t mv)
{
    if (ch >= VOLTAGE_NUM_CH) return;
    if (mv < VOLTAGE_MIN_MV) mv = VOLTAGE_MIN_MV;
    if (mv > VOLTAGE_MAX_MV) mv = VOLTAGE_MAX_MV;
    hctrl->ch[ch].target_mv = mv;
    hctrl->ch[ch].integral  = 0.0f; /* 목표 변경 시 적분 초기화 */
}

void VoltageCtrl_Enable(VoltageCtrl_t *hctrl, uint8_t ch, bool en)
{
    if (ch >= VOLTAGE_NUM_CH) return;
    hctrl->ch[ch].enabled = en;
    if (!en) {
        hctrl->ch[ch].fault_flags |= CH_STATUS_DISABLED;
        hctrl->ch[ch].integral = 0.0f;
        VoltageCtrl_SetPwm(ch, 0);
    } else {
        hctrl->ch[ch].fault_flags &= ~CH_STATUS_DISABLED;
    }
}

void VoltageCtrl_EnableAll(VoltageCtrl_t *hctrl, bool en)
{
    hctrl->system_enabled = en;
    for (int i = 0; i < VOLTAGE_NUM_CH; i++) VoltageCtrl_Enable(hctrl, i, en);
}

void VoltageCtrl_SetPwm(uint8_t ch, uint16_t duty)
{
    if (ch >= VOLTAGE_NUM_CH) return;
    if (duty > VOLTAGE_PWM_MAX) duty = VOLTAGE_PWM_MAX;
    switch (ch) {
        case 0: htim2.Instance->CCR1 = duty; break;
        case 1: htim2.Instance->CCR2 = duty; break;
        case 2: htim2.Instance->CCR3 = duty; break;
        case 3: htim2.Instance->CCR4 = duty; break;
    }
}

void VoltageCtrl_Update(VoltageCtrl_t *hctrl, MCP3465R_Handle_t *hadc)
{
    if (!hctrl->system_enabled) return;
    hctrl->ctrl_tick++;

    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        VoltageCh_t *ch = &hctrl->ch[i];
        if (!ch->enabled || (ch->fault_flags & (CH_STATUS_SHORT|CH_STATUS_OVERVOLT))) continue;

        /* ADC 측정값 반영 */
        if (hadc->results[i].is_valid)
            ch->measured_mv = hadc->results[i].voltage_mv;

        /* PI 계산 */
        float error = (float)(ch->target_mv - ch->measured_mv);
        ch->error_mv = error;

        if (error > -(float)CTRL_DEAD_BAND_MV && error < (float)CTRL_DEAD_BAND_MV)
            error = 0.0f;

        ch->integral += error;
        if (ch->integral >  CTRL_INTEGRAL_MAX) ch->integral =  CTRL_INTEGRAL_MAX;
        if (ch->integral <  CTRL_INTEGRAL_MIN) ch->integral =  CTRL_INTEGRAL_MIN;

        float pi_out = CTRL_KP * error + CTRL_KI * ch->integral;

        /* 증분 PWM 업데이트 */
        float new_duty = (float)ch->pwm_duty
                       + pi_out * ((float)VOLTAGE_PWM_MAX / (float)VOLTAGE_MAX_MV);
        if (new_duty < 0.0f)                    new_duty = 0.0f;
        if (new_duty > (float)VOLTAGE_PWM_MAX)  new_duty = (float)VOLTAGE_PWM_MAX;
        ch->pwm_duty = (uint16_t)new_duty;

        /* 과전압 보호 */
        if (ch->measured_mv > (VOLTAGE_MAX_MV + 200)) {
            ch->fault_flags |= CH_STATUS_OVERVOLT;
            VoltageCtrl_SetPwm(i, 0);
            ch->pwm_duty = 0;
            continue;
        }

        VoltageCtrl_SetPwm(i, ch->pwm_duty);
    }
}

void VoltageCtrl_Reset(VoltageCtrl_t *hctrl, uint8_t ch)
{
    if (ch >= VOLTAGE_NUM_CH) return;
    VoltageCh_t *c = &hctrl->ch[ch];
    c->fault_flags = CH_STATUS_OK;
    c->fault_count = 0;
    c->integral    = 0.0f;
    c->pwm_duty    = 0;
    c->error_mv    = 0.0f;
    VoltageCtrl_SetPwm(ch, 0);
}
