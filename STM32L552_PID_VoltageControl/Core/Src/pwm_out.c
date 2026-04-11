/* pwm_out.c - TIM1 4채널 PWM 출력 구현
 *
 * TIM1: Advanced timer (PCLK2 = 110 MHz)
 *   PSC=0, ARR=1099 → f_PWM = 110,000,000 / 1100 ≈ 100 kHz
 *
 * 채널 ↔ TIM1 매핑:
 *   CH0 → TIM_CHANNEL_1 (PA8)
 *   CH1 → TIM_CHANNEL_2 (PA9)
 *   CH2 → TIM_CHANNEL_3 (PA10)
 *   CH3 → TIM_CHANNEL_4 (PA11)
 */
#include "pwm_out.h"

/* 내부 채널 TIM 매핑 테이블 */
static const uint32_t s_tim_ch[NUM_CHANNELS] = {
    TIM_CHANNEL_1,
    TIM_CHANNEL_2,
    TIM_CHANNEL_3,
    TIM_CHANNEL_4
};

/* 현재 듀티 값 캐시 (0.0 ~ 1.0) */
static float s_duty_cache[NUM_CHANNELS] = {0};

/* ==========================================================
 * PWMOut_Start
 * ========================================================== */
void PWMOut_Start(void)
{
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        __HAL_TIM_SET_COMPARE(&htim1, s_tim_ch[i], 0U);
        HAL_TIM_PWM_Start(&htim1, s_tim_ch[i]);
        s_duty_cache[i] = 0.0f;
    }
}

/* ==========================================================
 * PWMOut_Stop
 * ========================================================== */
void PWMOut_Stop(void)
{
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        HAL_TIM_PWM_Stop(&htim1, s_tim_ch[i]);
        s_duty_cache[i] = 0.0f;
    }
}

/* ==========================================================
 * PWMOut_SetDuty
 * ========================================================== */
void PWMOut_SetDuty(uint8_t ch, float duty)
{
    if (ch >= NUM_CHANNELS) return;

    /* 범위 클램프 */
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    uint32_t ccr = (uint32_t)(duty * (float)PWM_ARR);
    __HAL_TIM_SET_COMPARE(&htim1, s_tim_ch[ch], ccr);
    s_duty_cache[ch] = duty;
}

/* ==========================================================
 * PWMOut_SetCCR
 * ========================================================== */
void PWMOut_SetCCR(uint8_t ch, uint32_t ccr)
{
    if (ch >= NUM_CHANNELS) return;

    if (ccr > PWM_MAX_DUTY) ccr = PWM_MAX_DUTY;

    __HAL_TIM_SET_COMPARE(&htim1, s_tim_ch[ch], ccr);
    s_duty_cache[ch] = (float)ccr / (float)PWM_ARR;
}

/* ==========================================================
 * PWMOut_Disable
 * ========================================================== */
void PWMOut_Disable(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return;

    __HAL_TIM_SET_COMPARE(&htim1, s_tim_ch[ch], 0U);
    s_duty_cache[ch] = 0.0f;
}

/* ==========================================================
 * PWMOut_GetDuty
 * ========================================================== */
float PWMOut_GetDuty(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return 0.0f;
    return s_duty_cache[ch];
}
