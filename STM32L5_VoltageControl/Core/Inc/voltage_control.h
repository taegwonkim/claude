#ifndef VOLTAGE_CONTROL_H
#define VOLTAGE_CONTROL_H

#include "main.h"
#include "mcp3465r.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * 전압 제어 모듈
 * - PWM으로 아날로그 전압 출력 (RC 필터 후)
 * - PI 제어로 목표 전압 추종
 * =========================================================*/

/* ----- 채널 설정 ----- */
#define VOLTAGE_NUM_CH          4        /* 전압 출력 채널 수 */
#define VOLTAGE_MAX_MV          3300     /* 최대 출력 전압 (mV) */
#define VOLTAGE_MIN_MV          0        /* 최소 출력 전압 (mV) */
#define VOLTAGE_PWM_MAX         4095     /* PWM 최대 카운트 (12비트) */

/* ----- PI 제어기 파라미터 ----- */
#define CTRL_KP                 5.0f     /* 비례 게인 */
#define CTRL_KI                 0.5f     /* 적분 게인 */
#define CTRL_INTEGRAL_MAX       2000.0f  /* 적분 와인드업 방지 한계 (mV) */
#define CTRL_INTEGRAL_MIN      -2000.0f
#define CTRL_DEAD_BAND_MV       5        /* 제어 불감대 (mV) - 헌팅 방지 */
#define CTRL_PERIOD_MS          10       /* 제어 주기 (ms) */

/* ----- 전류 한계 (단락/단선 감지) ----- */
#define CURRENT_MAX_MA          1000     /* 단락 판정 전류 (mA) */
#define CURRENT_MIN_MA          1        /* 단선 판정 전류 (mA) - 부하 연결 시 */
#define CURRENT_SHUNT_OHM_X100  10       /* 전류감지 저항 (0.01Ω × 100 = 1) */
#define CURRENT_AMP_GAIN        20       /* 전류감지 앰프 게인 (INA199A1) */
#define CURRENT_FAULT_COUNT     5        /* 폴트 연속 감지 횟수 임계값 */

/* ----- 채널 상태 플래그 ----- */
#define CH_STATUS_OK            0x00
#define CH_STATUS_SHORT         0x01     /* 단락 */
#define CH_STATUS_OPEN          0x02     /* 단선 */
#define CH_STATUS_OVERVOLTAGE   0x04     /* 과전압 */
#define CH_STATUS_DISABLED      0x08     /* 비활성 */

/* ----- 채널 데이터 구조체 ----- */
typedef struct {
    /* 설정값 */
    int32_t  target_mv;         /* 목표 전압 (mV) */
    bool     enabled;           /* 채널 활성화 여부 */

    /* 측정값 */
    int32_t  measured_mv;       /* ADC 측정 전압 (mV) */
    int32_t  current_ma;        /* 측정 전류 (mA) */

    /* 제어 상태 */
    float    error_mv;          /* 현재 오차 */
    float    integral;          /* 적분값 */
    uint16_t pwm_duty;          /* 현재 PWM 듀티 (0~4095) */

    /* 폴트 상태 */
    uint8_t  fault_flags;       /* 채널 상태 플래그 */
    uint8_t  fault_count;       /* 연속 폴트 카운터 */

    /* 통계 */
    int32_t  min_mv;            /* 최솟값 기록 */
    int32_t  max_mv;            /* 최댓값 기록 */
    uint32_t update_count;      /* 업데이트 횟수 */
} VoltageCh_t;

/* ----- 전압 제어 핸들 ----- */
typedef struct {
    VoltageCh_t ch[VOLTAGE_NUM_CH];
    bool        system_enabled;   /* 전체 시스템 활성화 */
    uint32_t    ctrl_tick;        /* 제어 루프 틱 카운터 */
} VoltageCtrl_t;

/* ----- 외부 TIM 핸들 (CubeMX 생성) ----- */
extern TIM_HandleTypeDef htim2;

/* ----- 함수 선언 ----- */
void     VoltageCtrl_Init(VoltageCtrl_t *hctrl);
void     VoltageCtrl_SetTarget(VoltageCtrl_t *hctrl, uint8_t ch, int32_t target_mv);
void     VoltageCtrl_Enable(VoltageCtrl_t *hctrl, uint8_t ch, bool enable);
void     VoltageCtrl_EnableAll(VoltageCtrl_t *hctrl, bool enable);
void     VoltageCtrl_Update(VoltageCtrl_t *hctrl, MCP3465R_Handle_t *hadc);
void     VoltageCtrl_SetPwm(uint8_t ch, uint16_t duty);
void     VoltageCtrl_Reset(VoltageCtrl_t *hctrl, uint8_t ch);
bool     VoltageCtrl_IsFault(VoltageCtrl_t *hctrl, uint8_t ch);
void     VoltageCtrl_GetStatus(VoltageCtrl_t *hctrl, uint8_t ch, VoltageCh_t *status);

/* 내부 PI 제어기 */
float    PI_Compute(VoltageCh_t *ch, int32_t measured_mv);
uint16_t PI_OutputToPwm(float pi_output, int32_t vref_mv);

#endif /* VOLTAGE_CONTROL_H */
