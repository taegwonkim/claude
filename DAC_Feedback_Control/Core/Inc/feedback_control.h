/**
 ******************************************************************************
 * @file    feedback_control.h
 * @brief   PI 피드백 제어기 (DAC 출력 ↔ ADC 측정 전압 일치)
 *
 * 제어 주기: 5ms (DACControlTask에서 vTaskDelayUntil 사용)
 *
 * PI 알고리즘 (고정소수점):
 *   error_mv = target_mv - measured_mv
 *   integral += error_mv * dt_ms       (오버플로우 방지: anti-windup 포함)
 *   output_mv = Kp * error_mv + Ki * integral
 *   dac_code  = clamp(mV_to_code(output_mv + feedforward), 0, 16383)
 *
 * 스케일: 정수 연산을 위해 Kp/Ki × 1000 저장
 ******************************************************************************
 */

#ifndef __FEEDBACK_CONTROL_H
#define __FEEDBACK_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* -------------------------------------------------------------------------
 * PI 제어 파라미터 (기본값 – 런타임 변경 가능)
 * ------------------------------------------------------------------------- */
#define PI_KP_DEFAULT_x1000     500     /* Kp = 0.5 */
#define PI_KI_DEFAULT_x1000     50      /* Ki = 0.05 */
#define PI_DT_MS                5       /* 제어 주기 (ms) */

/* 적분항 포화 방지 (anti-windup) 한계 (mV 단위 × 1000 스케일) */
#define PI_INTEGRAL_MAX         (int32_t)(VREF_MV * 1000)
#define PI_INTEGRAL_MIN         (int32_t)(-VREF_MV * 1000)

/* DAC 코드 변화 속도 제한 (slew rate limiter): 1회 최대 변화량 */
#define PI_MAX_CODE_SLEW        200     /* 1 tick당 최대 200 LSB */

/* -------------------------------------------------------------------------
 * PI 제어기 상태 구조체
 * ------------------------------------------------------------------------- */
typedef struct {
    int32_t Kp_x1000;       /* 비례 이득 × 1000 */
    int32_t Ki_x1000;       /* 적분 이득 × 1000 */
    int32_t integral_x1000; /* 적분 누적값 × 1000 (mV·ms) */
    int32_t prev_error_mv;  /* 이전 오차 (D항 사용 시) */
    uint16_t output_code;   /* 최종 DAC 코드 출력 */
} PiController_t;

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * @brief  전체 채널 PI 제어기 초기화
 */
void FeedbackControl_Init(void);

/**
 * @brief  단일 채널 PI 제어 1스텝 실행
 * @param  ch         채널 인덱스 (0~3)
 * @param  target_mv  목표 전압 (mV)
 * @param  meas_mv    측정 전압 (mV)
 * @return 새로 설정된 DAC 코드
 */
uint16_t FeedbackControl_Step(uint8_t ch, uint16_t target_mv, uint16_t meas_mv);

/**
 * @brief  채널 PI 제어기 리셋 (적분항 클리어, 피드포워드로 초기화)
 * @param  ch          채널 인덱스 (0~3)
 * @param  init_mv     초기 출력 전압 (mV) – 피드포워드 시작값
 */
void FeedbackControl_Reset(uint8_t ch, uint16_t init_mv);

/**
 * @brief  채널 PI 이득 변경
 */
void FeedbackControl_SetGains(uint8_t ch, int32_t kp_x1000, int32_t ki_x1000);

/**
 * @brief  모든 채널 제어 루프 1회 실행 (g_channels 읽고 DAC 업데이트)
 *         DACControlTask에서 주기적으로 호출
 */
void FeedbackControl_RunAll(void);

#ifdef __cplusplus
}
#endif

#endif /* __FEEDBACK_CONTROL_H */
