/* =========================================================
 * 전압 제어 구현 (PI 피드백 제어)
 * =========================================================*/
#include "voltage_control.h"
#include <string.h>

/* PWM 채널 → TIM2 CCR 레지스터 매핑 */
static uint32_t const pwm_channel_map[VOLTAGE_NUM_CH] = {
    TIM_CHANNEL_1,
    TIM_CHANNEL_2,
    TIM_CHANNEL_3,
    TIM_CHANNEL_4,
};

/* =========================================================
 * 초기화
 * =========================================================*/
void VoltageCtrl_Init(VoltageCtrl_t *hctrl)
{
    memset(hctrl, 0, sizeof(VoltageCtrl_t));
    hctrl->system_enabled = false;

    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        hctrl->ch[i].target_mv   = 0;
        hctrl->ch[i].enabled     = false;
        hctrl->ch[i].pwm_duty    = 0;
        hctrl->ch[i].integral    = 0.0f;
        hctrl->ch[i].fault_flags = CH_STATUS_DISABLED;
        hctrl->ch[i].min_mv      = 9999999;
        hctrl->ch[i].max_mv      = -9999999;
    }

    /* TIM2 PWM 시작 */
    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        HAL_TIM_PWM_Start(&htim2, pwm_channel_map[i]);
        VoltageCtrl_SetPwm(i, 0);
    }
}

/* =========================================================
 * 목표 전압 설정
 * =========================================================*/
void VoltageCtrl_SetTarget(VoltageCtrl_t *hctrl, uint8_t ch, int32_t target_mv)
{
    if (ch >= VOLTAGE_NUM_CH) return;

    /* 범위 클리핑 */
    if (target_mv < VOLTAGE_MIN_MV) target_mv = VOLTAGE_MIN_MV;
    if (target_mv > VOLTAGE_MAX_MV) target_mv = VOLTAGE_MAX_MV;

    hctrl->ch[ch].target_mv = target_mv;

    /* 목표 전압이 바뀌면 적분항 초기화 (와인드업 방지) */
    hctrl->ch[ch].integral = 0.0f;
}

/* =========================================================
 * 채널 활성화/비활성화
 * =========================================================*/
void VoltageCtrl_Enable(VoltageCtrl_t *hctrl, uint8_t ch, bool enable)
{
    if (ch >= VOLTAGE_NUM_CH) return;

    hctrl->ch[ch].enabled = enable;

    if (!enable) {
        hctrl->ch[ch].fault_flags |= CH_STATUS_DISABLED;
        hctrl->ch[ch].integral = 0.0f;
        VoltageCtrl_SetPwm(ch, 0);
    } else {
        hctrl->ch[ch].fault_flags &= ~CH_STATUS_DISABLED;
    }
}

void VoltageCtrl_EnableAll(VoltageCtrl_t *hctrl, bool enable)
{
    hctrl->system_enabled = enable;
    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        VoltageCtrl_Enable(hctrl, i, enable);
    }
}

/* =========================================================
 * PI 제어기 계산
 * 반환: 보정 PWM 조정량 (mV 단위의 제어 출력)
 * =========================================================*/
float PI_Compute(VoltageCh_t *ch, int32_t measured_mv)
{
    float error = (float)(ch->target_mv - measured_mv);
    ch->error_mv = error;

    /* 불감대: 오차가 아주 작으면 적분 누적 중단 */
    if (error > -(float)CTRL_DEAD_BAND_MV && error < (float)CTRL_DEAD_BAND_MV) {
        error = 0.0f;
    }

    /* 적분 누적 */
    ch->integral += error;

    /* 적분 와인드업 방지 */
    if (ch->integral > CTRL_INTEGRAL_MAX) ch->integral = CTRL_INTEGRAL_MAX;
    if (ch->integral < CTRL_INTEGRAL_MIN) ch->integral = CTRL_INTEGRAL_MIN;

    /* PI 출력 (mV 보정량) */
    float output = CTRL_KP * error + CTRL_KI * ch->integral;

    return output;
}

/* =========================================================
 * PI 출력 → PWM 듀티 변환
 * =========================================================*/
uint16_t PI_OutputToPwm(float pi_output, int32_t vref_mv)
{
    if (vref_mv <= 0) return 0;

    /* PWM 듀티 = PI출력 / VREF × PWM_MAX */
    float duty_f = (pi_output / (float)vref_mv) * (float)VOLTAGE_PWM_MAX;

    /* 클리핑 */
    if (duty_f < 0.0f)                    duty_f = 0.0f;
    if (duty_f > (float)VOLTAGE_PWM_MAX)  duty_f = (float)VOLTAGE_PWM_MAX;

    return (uint16_t)duty_f;
}

/* =========================================================
 * PWM 듀티 설정 (하드웨어 레지스터 직접 설정)
 * =========================================================*/
void VoltageCtrl_SetPwm(uint8_t ch, uint16_t duty)
{
    if (ch >= VOLTAGE_NUM_CH) return;
    if (duty > VOLTAGE_PWM_MAX) duty = VOLTAGE_PWM_MAX;

    /* TIM2 CCR 레지스터에 직접 쓰기 (가장 빠름) */
    switch (ch) {
        case 0: htim2.Instance->CCR1 = duty; break;
        case 1: htim2.Instance->CCR2 = duty; break;
        case 2: htim2.Instance->CCR3 = duty; break;
        case 3: htim2.Instance->CCR4 = duty; break;
        default: break;
    }
}

/* =========================================================
 * 제어 루프 업데이트 (CTRL_PERIOD_MS마다 호출)
 * =========================================================*/
void VoltageCtrl_Update(VoltageCtrl_t *hctrl, MCP3465R_Handle_t *hadc)
{
    if (!hctrl->system_enabled) return;

    hctrl->ctrl_tick++;

    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        VoltageCh_t *ch = &hctrl->ch[i];

        /* 비활성 채널 또는 폴트 채널 스킵 */
        if (!ch->enabled || (ch->fault_flags & (CH_STATUS_SHORT | CH_STATUS_OVERVOLTAGE))) {
            continue;
        }

        /* ADC 측정값 업데이트 */
        if (hadc->results[i].is_valid) {
            ch->measured_mv = hadc->results[i].voltage_mv;

            /* 통계 업데이트 */
            if (ch->measured_mv < ch->min_mv) ch->min_mv = ch->measured_mv;
            if (ch->measured_mv > ch->max_mv) ch->max_mv = ch->measured_mv;
            ch->update_count++;
        }

        /* PI 제어 계산 */
        float pi_out = PI_Compute(ch, ch->measured_mv);

        /* 현재 PWM에서 보정량 추가 (증분식 PI) */
        float new_duty_f = (float)ch->pwm_duty + pi_out * ((float)VOLTAGE_PWM_MAX / (float)VOLTAGE_MAX_MV);

        /* 클리핑 */
        if (new_duty_f < 0.0f)                    new_duty_f = 0.0f;
        if (new_duty_f > (float)VOLTAGE_PWM_MAX)  new_duty_f = (float)VOLTAGE_PWM_MAX;

        ch->pwm_duty = (uint16_t)new_duty_f;

        /* 과전압 감지 */
        if (ch->measured_mv > (VOLTAGE_MAX_MV + 200)) {
            ch->fault_flags |= CH_STATUS_OVERVOLTAGE;
            VoltageCtrl_SetPwm(i, 0);
            ch->pwm_duty = 0;
            continue;
        }

        /* PWM 출력 */
        VoltageCtrl_SetPwm(i, ch->pwm_duty);
    }
}

/* =========================================================
 * 채널 리셋 (폴트 해제 및 재시작)
 * =========================================================*/
void VoltageCtrl_Reset(VoltageCtrl_t *hctrl, uint8_t ch)
{
    if (ch >= VOLTAGE_NUM_CH) return;

    VoltageCh_t *c = &hctrl->ch[ch];
    c->fault_flags = CH_STATUS_OK;
    c->fault_count = 0;
    c->integral    = 0.0f;
    c->pwm_duty    = 0;
    c->error_mv    = 0.0f;
    c->min_mv      = 9999999;
    c->max_mv      = -9999999;

    VoltageCtrl_SetPwm(ch, 0);
}

/* =========================================================
 * 폴트 여부 확인
 * =========================================================*/
bool VoltageCtrl_IsFault(VoltageCtrl_t *hctrl, uint8_t ch)
{
    if (ch >= VOLTAGE_NUM_CH) return true;
    return (hctrl->ch[ch].fault_flags & (CH_STATUS_SHORT |
                                         CH_STATUS_OPEN  |
                                         CH_STATUS_OVERVOLTAGE)) != 0;
}

/* =========================================================
 * 채널 상태 복사 반환
 * =========================================================*/
void VoltageCtrl_GetStatus(VoltageCtrl_t *hctrl, uint8_t ch, VoltageCh_t *status)
{
    if (ch >= VOLTAGE_NUM_CH || status == NULL) return;
    *status = hctrl->ch[ch];
}
