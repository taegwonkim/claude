/**
 * @file    voltage_ctrl.c
 * @brief   4채널 전압 제어 레이어 구현
 *
 * 채널 1, 2 : DAC1 내부 DAC (12-bit)
 * 채널 3, 4 : TIM3 PWM + RC 필터 (등가 12-bit)
 *
 * PID 기본 튜닝 파라미터 (하드웨어 RC 특성에 따라 재조정 필요):
 *   Kp = 200.0   (비례: 큰 오차 → 빠른 응답)
 *   Ki =  50.0   (적분: 정상 상태 오차 제거)
 *   Kd =   5.0   (미분: 과도 진동 억제)
 *   dt =  0.01s  (10 ms 제어 주기)
 */

#include "voltage_ctrl.h"
#include "main.h"
#include <string.h>

/* ======================================================================== */
/*  내부 전역 인스턴스                                                        */
/* ======================================================================== */

static VCtrl_t s_ctrl;

/* ======================================================================== */
/*  내부 헬퍼 함수                                                            */
/* ======================================================================== */

/**
 * @brief  DAC 채널에 raw 값을 쓴다 (0 ~ 4095).
 */
static void apply_dac(uint32_t dac_channel, uint32_t raw)
{
    HAL_DAC_SetValue(&hdac1, dac_channel, DAC_ALIGN_12B_R, raw);
}

/**
 * @brief  TIM3 PWM 채널의 비교값을 설정한다 (0 ~ TIM ARR).
 * @note   TIM3 ARR = 4095 (12-bit 등가 해상도)
 */
static void apply_pwm(uint32_t tim_channel, uint32_t raw)
{
    __HAL_TIM_SET_COMPARE(&htim3, tim_channel, raw);
}

/* ======================================================================== */
/*  공개 API                                                                  */
/* ======================================================================== */

void VCtrl_Init(void)
{
    memset(&s_ctrl, 0, sizeof(VCtrl_t));

    /*
     * 채널별 PID 초기화
     * 기본 목표 전압: 1.65 V (VREF/2, 중간 전압)
     * Kp, Ki, Kd, out_min, out_max, dt
     */
    const float KP  = 200.0f;
    const float KI  =  50.0f;
    const float KD  =   5.0f;
    const float DT  =   0.01f;  /* 10 ms */
    const float V0  =   1.65f;  /* 초기 목표 전압 [V] */

    /* ---- 채널 0 : DAC1_OUT1 (PA4) ---- */
    PID_Init(&s_ctrl.ch[0].pid, KP, KI, KD, 0.0f, 4095.0f, DT);
    PID_SetSetpoint(&s_ctrl.ch[0].pid, VCTRL_VOLT_TO_RAW(V0));
    s_ctrl.ch[0].out_type    = VCTRL_OUT_DAC;
    s_ctrl.ch[0].dac_channel = DAC_CHANNEL_1;
    s_ctrl.ch[0].adc_rank    = 0U;
    s_ctrl.ch[0].setpoint_v  = V0;

    /* ---- 채널 1 : DAC1_OUT2 (PA5) ---- */
    PID_Init(&s_ctrl.ch[1].pid, KP, KI, KD, 0.0f, 4095.0f, DT);
    PID_SetSetpoint(&s_ctrl.ch[1].pid, VCTRL_VOLT_TO_RAW(V0));
    s_ctrl.ch[1].out_type    = VCTRL_OUT_DAC;
    s_ctrl.ch[1].dac_channel = DAC_CHANNEL_2;
    s_ctrl.ch[1].adc_rank    = 1U;
    s_ctrl.ch[1].setpoint_v  = V0;

    /* ---- 채널 2 : TIM3_CH1 PWM (PA6) ---- */
    PID_Init(&s_ctrl.ch[2].pid, KP, KI, KD, 0.0f, 4095.0f, DT);
    PID_SetSetpoint(&s_ctrl.ch[2].pid, VCTRL_VOLT_TO_RAW(V0));
    s_ctrl.ch[2].out_type    = VCTRL_OUT_PWM;
    s_ctrl.ch[2].tim_channel = TIM_CHANNEL_1;
    s_ctrl.ch[2].adc_rank    = 2U;
    s_ctrl.ch[2].setpoint_v  = V0;

    /* ---- 채널 3 : TIM3_CH2 PWM (PA7) ---- */
    PID_Init(&s_ctrl.ch[3].pid, KP, KI, KD, 0.0f, 4095.0f, DT);
    PID_SetSetpoint(&s_ctrl.ch[3].pid, VCTRL_VOLT_TO_RAW(V0));
    s_ctrl.ch[3].out_type    = VCTRL_OUT_PWM;
    s_ctrl.ch[3].tim_channel = TIM_CHANNEL_2;
    s_ctrl.ch[3].adc_rank    = 3U;
    s_ctrl.ch[3].setpoint_v  = V0;

    /* ---- DAC 출력 시작 ---- */
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);

    /* ---- PWM 출력 시작 ---- */
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

    /* ---- 초기 출력: VREF/2 ---- */
    uint32_t init_raw = VCTRL_VOLT_TO_RAW(V0);
    apply_dac(DAC_CHANNEL_1, init_raw);
    apply_dac(DAC_CHANNEL_2, init_raw);
    apply_pwm(TIM_CHANNEL_1, init_raw);
    apply_pwm(TIM_CHANNEL_2, init_raw);

    s_ctrl.initialized = true;
}

void VCtrl_SetTarget(uint8_t ch_idx, float volt)
{
    if (ch_idx >= VCTRL_NUM_CHANNELS) return;

    /* 전압 범위 클램핑 */
    if (volt < 0.0f)  volt = 0.0f;
    if (volt > 3.3f)  volt = 3.3f;

    VCtrl_Channel_t *ch = &s_ctrl.ch[ch_idx];
    ch->setpoint_v = volt;

    /* PID 목표값은 DAC raw 단위로 전달 */
    PID_SetSetpoint(&ch->pid, VCTRL_VOLT_TO_RAW(volt));
}

void VCtrl_UpdateFeedback(const uint16_t *adc_buf)
{
    for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
        uint16_t raw = adc_buf[s_ctrl.ch[i].adc_rank];
        s_ctrl.ch[i].feedback_v = VCTRL_RAW_TO_VOLT(raw);
    }
}

void VCtrl_RunControl(void)
{
    if (!s_ctrl.initialized) return;

    for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
        VCtrl_Channel_t *ch = &s_ctrl.ch[i];

        /* PID 연산: 측정값은 ADC raw 등가 전압 단위로 전달 */
        float measurement_raw = VCTRL_VOLT_TO_RAW(ch->feedback_v);
        float output = PID_Update(&ch->pid, measurement_raw);

        ch->output_raw = output;
        ch->error_v    = ch->setpoint_v - ch->feedback_v;

        uint32_t raw_u32 = (uint32_t)output;
        if (raw_u32 > VCTRL_DAC_MAX_RAW) raw_u32 = VCTRL_DAC_MAX_RAW;

        /* 출력 드라이버 적용 */
        if (ch->out_type == VCTRL_OUT_DAC) {
            apply_dac(ch->dac_channel, raw_u32);
        } else {
            apply_pwm(ch->tim_channel, raw_u32);
        }
    }
}

void VCtrl_GetChannelInfo(uint8_t ch_idx, VCtrl_Channel_t *out_ch)
{
    if (ch_idx >= VCTRL_NUM_CHANNELS || out_ch == NULL) return;
    *out_ch = s_ctrl.ch[ch_idx];  /* 구조체 복사 */
}

void VCtrl_SetEnabled(uint8_t ch_idx, bool enable)
{
    if (ch_idx >= VCTRL_NUM_CHANNELS) return;
    PID_SetEnabled(&s_ctrl.ch[ch_idx].pid, enable);
}

void VCtrl_SetPID(uint8_t ch_idx, float Kp, float Ki, float Kd)
{
    if (ch_idx >= VCTRL_NUM_CHANNELS) return;
    PID_t *pid = &s_ctrl.ch[ch_idx].pid;
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    PID_Reset(pid);  /* 이득 변경 시 과도응답 방지를 위해 적분 초기화 */
}
