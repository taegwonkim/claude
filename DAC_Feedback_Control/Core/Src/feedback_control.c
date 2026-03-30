/**
 ******************************************************************************
 * @file    feedback_control.c
 * @brief   PI 피드백 제어기 구현
 *
 * 알고리즘 (고정소수점, 스케일 × 1000):
 *   e[k]   = target_mv - measured_mv
 *   I[k]   = I[k-1] + e[k] × dt_ms     (ms 단위, anti-windup 포함)
 *   u[k]   = Kp × e[k] + Ki × I[k]     (단위: mV)
 *   code   = clamp(DAC_VoltageToCode(u[k]), 0, DAC_MAX_CODE)
 *
 * 슬루 레이트 제한:
 *   코드 변화가 PI_MAX_CODE_SLEW를 초과하면 단계적으로 변경
 ******************************************************************************
 */

#include "feedback_control.h"
#include "dac_ad5641.h"

/* -------------------------------------------------------------------------
 * 채널별 PI 제어기 인스턴스
 * ------------------------------------------------------------------------- */
static PiController_t s_pi[NUM_CHANNELS];

/* -------------------------------------------------------------------------
 * 내부 헬퍼
 * ------------------------------------------------------------------------- */
static int32_t _clamp32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* -------------------------------------------------------------------------
 * 공개 API 구현
 * ------------------------------------------------------------------------- */

void FeedbackControl_Init(void)
{
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        s_pi[i].Kp_x1000       = PI_KP_DEFAULT_x1000;
        s_pi[i].Ki_x1000       = PI_KI_DEFAULT_x1000;
        s_pi[i].integral_x1000 = 0;
        s_pi[i].prev_error_mv  = 0;
        s_pi[i].output_code    = 0;
    }
}

void FeedbackControl_Reset(uint8_t ch, uint16_t init_mv)
{
    if (ch >= NUM_CHANNELS) return;

    s_pi[ch].integral_x1000 = 0;
    s_pi[ch].prev_error_mv  = 0;

    /* 피드포워드 초기값으로 DAC 코드 설정 */
    s_pi[ch].output_code = DAC_VoltageToCode(init_mv);
    DAC_WriteCode(ch, s_pi[ch].output_code, DAC_PD_NORMAL);
}

void FeedbackControl_SetGains(uint8_t ch, int32_t kp_x1000, int32_t ki_x1000)
{
    if (ch >= NUM_CHANNELS) return;
    s_pi[ch].Kp_x1000 = kp_x1000;
    s_pi[ch].Ki_x1000 = ki_x1000;
    s_pi[ch].integral_x1000 = 0;   /* 이득 변경 시 적분항 리셋 */
}

uint16_t FeedbackControl_Step(uint8_t ch, uint16_t target_mv, uint16_t meas_mv)
{
    if (ch >= NUM_CHANNELS) return 0;

    PiController_t *pi = &s_pi[ch];

    /* 오차 계산 (mV) */
    int32_t error_mv = (int32_t)target_mv - (int32_t)meas_mv;

    /* 적분항 업데이트 (× 1000 스케일)
     * I += error_mv × dt_ms / 1000 × 1000 = error_mv × dt_ms
     */
    pi->integral_x1000 += error_mv * PI_DT_MS;

    /* Anti-windup: 적분항 포화 클램프 */
    pi->integral_x1000 = _clamp32(pi->integral_x1000,
                                   PI_INTEGRAL_MIN,
                                   PI_INTEGRAL_MAX);

    /*
     * PI 출력 계산 (× 1000 스케일 해제):
     *   u = Kp × e + Ki × I
     *   u_x1000000 = Kp_x1000 × e + Ki_x1000 × I_x1000
     *              = Kp_x1000 × e + (Ki_x1000 × I_x1000) / 1000
     *
     * 정밀도 유지를 위해 중간값을 int64로 처리
     */
    int64_t u_x1000 = ((int64_t)pi->Kp_x1000 * error_mv)
                    + ((int64_t)pi->Ki_x1000 * pi->integral_x1000 / 1000);

    /* 단위: mV × 1000 → mV 로 변환 */
    int32_t output_mv = (int32_t)(u_x1000 / 1000);

    /*
     * 피드포워드: target 전압을 기준으로 PI 보정량 더하기
     * output_mv는 "보정량"이 아니라 "출력 전압" 직접 계산인 경우:
     * 여기서는 PI 출력 = 목표 전압 + 보정량 방식 사용
     */
    int32_t total_mv = (int32_t)target_mv + output_mv;

    /* 전압 범위 클램프 */
    total_mv = _clamp32(total_mv, 0, (int32_t)VREF_MV);

    /* mV → DAC 코드 */
    uint16_t new_code = DAC_VoltageToCode((uint16_t)total_mv);

    /* 슬루 레이트 제한: 급격한 변화 억제 */
    int32_t code_diff = (int32_t)new_code - (int32_t)pi->output_code;
    if (code_diff > PI_MAX_CODE_SLEW) {
        new_code = pi->output_code + PI_MAX_CODE_SLEW;
    } else if (code_diff < -PI_MAX_CODE_SLEW) {
        new_code = (pi->output_code > PI_MAX_CODE_SLEW)
                   ? pi->output_code - PI_MAX_CODE_SLEW
                   : 0;
    }

    pi->output_code   = new_code;
    pi->prev_error_mv = error_mv;

    return new_code;
}

void FeedbackControl_RunAll(void)
{
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
        /* 뮤텍스로 g_channels 읽기 */
        if (osMutexAcquire(xMutexChannelData, 5) != osOK) {
            continue;
        }

        if (!g_channels[ch].enabled ||
            g_channels[ch].fault != FAULT_NONE) {
            osMutexRelease(xMutexChannelData);
            continue;
        }

        uint16_t target = g_channels[ch].target_mv;
        uint16_t meas   = g_channels[ch].measured_mv;
        osMutexRelease(xMutexChannelData);

        /* PI 제어 1스텝 */
        uint16_t new_code = FeedbackControl_Step(ch, target, meas);

        /* DAC 출력 */
        DAC_WriteCode(ch, new_code, DAC_PD_NORMAL);

        /* DAC 코드 기록 */
        if (osMutexAcquire(xMutexChannelData, 5) == osOK) {
            g_channels[ch].dac_code = new_code;
            osMutexRelease(xMutexChannelData);
        }
    }
}
