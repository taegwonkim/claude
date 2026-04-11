/* channel_ctrl.h - 4채널 전압 제어 상태 및 인터페이스 */
#ifndef CHANNEL_CTRL_H
#define CHANNEL_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "pid.h"
#include "main.h"

/* ==========================================================
 * 채널 상태 플래그 (status 비트 정의)
 * ========================================================== */
#define CH_STATUS_ENABLED       (1U << 0)   /* 채널 활성화 */
#define CH_STATUS_OVP           (1U << 1)   /* 과전압 보호 발동 */
#define CH_STATUS_OCP           (1U << 2)   /* 과전류 보호 발동 */
#define CH_STATUS_FAULT         (1U << 3)   /* 일반 오류 */

/* ==========================================================
 * 기본 PID 게인 (초기값; CAN 명령으로 런타임 변경 가능)
 * ========================================================== */
#define PID_DEFAULT_KP      2.0f
#define PID_DEFAULT_KI      5.0f
#define PID_DEFAULT_KD      0.05f
#define PID_SAMPLE_TIME_S   0.002f   /* 2ms (500 Hz PID 루프) */

/* ==========================================================
 * 채널 데이터 구조체
 * ========================================================== */
typedef struct {
    float    voltage_setpoint_V;   /* 목표 전압 (V) */
    float    voltage_meas_V;       /* 측정 전압 (V) */
    float    current_meas_A;       /* 측정 전류 (A) */
    float    pwm_duty;             /* 현재 PWM 듀티 (0.0 ~ 1.0) */
    PID_t    pid;                  /* PID 컨트롤러 인스턴스 */
    uint8_t  status;               /* 상태 비트 플래그 */
    uint8_t  ch_index;             /* 채널 번호 (0~3) */
} ChannelCtrl_t;

/* ==========================================================
 * 전역 채널 배열 (channel_ctrl.c 에서 정의)
 * ========================================================== */
extern ChannelCtrl_t g_channels[NUM_CHANNELS];

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */

/** @brief 모든 채널 초기화 (PID 포함) */
void ChannelCtrl_Init(void);

/**
 * @brief 한 채널의 PID를 실행하고 PWM 듀티 갱신
 * @param ch  채널 번호 (0~3)
 */
void ChannelCtrl_Update(uint8_t ch);

/**
 * @brief ADC 변환 결과로 측정값 갱신
 * @param ch          채널 번호 (0~3)
 * @param voltage_mV  측정 전압 (mV)
 * @param current_mA  측정 전류 (mA)
 */
void ChannelCtrl_SetMeasured(uint8_t ch,
                              uint32_t voltage_mV,
                              uint32_t current_mA);

/**
 * @brief 채널 목표 전압 설정
 * @param ch          채널 번호 (0~3)
 * @param setpoint_mV 목표 전압 (mV, 0~12000)
 */
void ChannelCtrl_SetVoltage(uint8_t ch, uint32_t setpoint_mV);

/**
 * @brief 채널 활성화/비활성화
 * @param ch      채널 번호 (0~3)
 * @param enable  1=활성화, 0=비활성화
 */
void ChannelCtrl_Enable(uint8_t ch, uint8_t enable);

/**
 * @brief PID 게인 변경
 * @param ch  채널 번호 (0~3)
 */
void ChannelCtrl_SetPIDGains(uint8_t ch, float Kp, float Ki, float Kd);

/**
 * @brief 보호 회로 점검 (OVP/OCP 발동 시 채널 비활성화)
 * @param ch  채널 번호 (0~3)
 */
void ChannelCtrl_CheckProtection(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_CTRL_H */
