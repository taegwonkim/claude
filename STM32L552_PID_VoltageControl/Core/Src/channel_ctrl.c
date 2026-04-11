/* channel_ctrl.c - 4채널 전압 제어 구현 */
#include "channel_ctrl.h"
#include "pwm_out.h"
#include <string.h>

/* ==========================================================
 * 전역 채널 배열
 * ========================================================== */
ChannelCtrl_t g_channels[NUM_CHANNELS];

/* ==========================================================
 * ChannelCtrl_Init
 * ========================================================== */
void ChannelCtrl_Init(void)
{
    memset(g_channels, 0, sizeof(g_channels));

    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        g_channels[i].ch_index          = i;
        g_channels[i].voltage_setpoint_V = 0.0f;
        g_channels[i].voltage_meas_V     = 0.0f;
        g_channels[i].current_meas_A     = 0.0f;
        g_channels[i].pwm_duty           = 0.0f;
        g_channels[i].status             = 0U;

        /* PID 초기화: 출력 범위 0.0 ~ 1.0 (PWM 듀티) */
        PID_Init(&g_channels[i].pid,
                 PID_DEFAULT_KP,
                 PID_DEFAULT_KI,
                 PID_DEFAULT_KD,
                 0.0f,          /* 출력 하한 (듀티 0%) */
                 1.0f,          /* 출력 상한 (듀티 100%) */
                 PID_SAMPLE_TIME_S);

        /* 기본값: 채널 비활성화 */
        PID_SetEnabled(&g_channels[i].pid, 0);
    }
}

/* ==========================================================
 * ChannelCtrl_SetMeasured
 * ========================================================== */
void ChannelCtrl_SetMeasured(uint8_t ch,
                              uint32_t voltage_mV,
                              uint32_t current_mA)
{
    if (ch >= NUM_CHANNELS) return;

    g_channels[ch].voltage_meas_V = (float)voltage_mV / 1000.0f;
    g_channels[ch].current_meas_A = (float)current_mA / 1000.0f;
}

/* ==========================================================
 * ChannelCtrl_Update
 *   - 보호 점검 → PID 계산 → PWM 출력 갱신
 * ========================================================== */
void ChannelCtrl_Update(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return;

    ChannelCtrl_t *c = &g_channels[ch];

    /* 채널이 비활성화 상태면 PWM 출력 0 */
    if (!(c->status & CH_STATUS_ENABLED)) {
        c->pwm_duty = 0.0f;
        PWMOut_Disable(ch);
        return;
    }

    /* 보호 회로 점검 */
    ChannelCtrl_CheckProtection(ch);

    /* 보호 발동 후에도 비활성화 상태이면 종료 */
    if (!(c->status & CH_STATUS_ENABLED)) {
        return;
    }

    /* PID 계산 */
    float duty = PID_Compute(&c->pid, c->voltage_meas_V);
    c->pwm_duty = duty;

    /* PWM 출력 갱신 */
    PWMOut_SetDuty(ch, duty);
}

/* ==========================================================
 * ChannelCtrl_SetVoltage
 * ========================================================== */
void ChannelCtrl_SetVoltage(uint8_t ch, uint32_t setpoint_mV)
{
    if (ch >= NUM_CHANNELS) return;

    /* 범위 제한 */
    if (setpoint_mV > OUTPUT_VOLT_MAX_MV) {
        setpoint_mV = OUTPUT_VOLT_MAX_MV;
    }

    float setpoint_V = (float)setpoint_mV / 1000.0f;
    g_channels[ch].voltage_setpoint_V = setpoint_V;
    PID_SetSetpoint(&g_channels[ch].pid, setpoint_V);

    /* 세트포인트 변경 시 적분 리셋으로 오버슈트 완화 */
    PID_Reset(&g_channels[ch].pid);
}

/* ==========================================================
 * ChannelCtrl_Enable
 * ========================================================== */
void ChannelCtrl_Enable(uint8_t ch, uint8_t enable)
{
    if (ch >= NUM_CHANNELS) return;

    if (enable) {
        /* 보호 플래그 클리어 후 활성화 */
        g_channels[ch].status &= ~(CH_STATUS_OVP | CH_STATUS_OCP | CH_STATUS_FAULT);
        g_channels[ch].status |= CH_STATUS_ENABLED;
        PID_SetEnabled(&g_channels[ch].pid, 1);
        PID_Reset(&g_channels[ch].pid);
    } else {
        g_channels[ch].status &= ~CH_STATUS_ENABLED;
        PID_SetEnabled(&g_channels[ch].pid, 0);
        g_channels[ch].pwm_duty = 0.0f;
        PWMOut_Disable(ch);
    }
}

/* ==========================================================
 * ChannelCtrl_SetPIDGains
 * ========================================================== */
void ChannelCtrl_SetPIDGains(uint8_t ch, float Kp, float Ki, float Kd)
{
    if (ch >= NUM_CHANNELS) return;
    PID_SetTunings(&g_channels[ch].pid, Kp, Ki, Kd);
}

/* ==========================================================
 * ChannelCtrl_CheckProtection
 *   과전압(OVP) 또는 과전류(OCP) 발생 시 채널 비활성화
 * ========================================================== */
void ChannelCtrl_CheckProtection(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return;

    ChannelCtrl_t *c = &g_channels[ch];

    uint32_t v_mV = (uint32_t)(c->voltage_meas_V * 1000.0f);
    uint32_t i_mA = (uint32_t)(c->current_meas_A * 1000.0f);

    /* 과전압 보호 */
    if (v_mV >= OVP_THRESHOLD_MV) {
        c->status |= CH_STATUS_OVP;
        c->status &= ~CH_STATUS_ENABLED;
        PID_SetEnabled(&c->pid, 0);
        PWMOut_Disable(ch);
    }

    /* 과전류 보호 */
    if (i_mA >= OCP_THRESHOLD_MA) {
        c->status |= CH_STATUS_OCP;
        c->status &= ~CH_STATUS_ENABLED;
        PID_SetEnabled(&c->pid, 0);
        PWMOut_Disable(ch);
    }
}
