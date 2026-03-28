/**
 * @file    voltage_ctrl.h
 * @brief   4채널 전압 제어 레이어 (DAC + ADC 피드백 + PID)
 *
 * 하드웨어 구성:
 *   채널 1 : DAC1_OUT1  (PA4)  — STM32H5 내부 12-bit DAC
 *   채널 2 : DAC1_OUT2  (PA5)  — STM32H5 내부 12-bit DAC
 *   채널 3 : TIM3_CH1   (PA6)  — PWM → RC 저역 통과 필터 → 아날로그 출력
 *   채널 4 : TIM3_CH2   (PA7)  — PWM → RC 저역 통과 필터 → 아날로그 출력
 *
 * ADC 피드백:
 *   채널 1 : ADC1_IN0   (PA0)
 *   채널 2 : ADC1_IN1   (PA1)
 *   채널 3 : ADC1_IN2   (PA2)
 *   채널 4 : ADC1_IN3   (PA3)
 *
 * 전압 범위 : 0 ~ 3.3 V  (VREF = 3.3 V, 12-bit 해상도)
 * PWM 해상도: 12-bit 등가 (TIM ARR = 4095)
 */

#ifndef VOLTAGE_CTRL_H
#define VOLTAGE_CTRL_H

#include "pid.h"
#include <stdint.h>
#include <stdbool.h>

/* ======================================================================== */
/*  상수 정의                                                                 */
/* ======================================================================== */

#define VCTRL_NUM_CHANNELS      4U      /**< 제어 채널 수 */
#define VCTRL_VREF_MV           3300U   /**< VREF 전압 [mV] */
#define VCTRL_ADC_RESOLUTION    4096U   /**< ADC/DAC 해상도 (12-bit) */
#define VCTRL_DAC_MAX_RAW       4095U   /**< DAC 최대 raw 값 */

/** ADC raw → 전압[V] 변환 */
#define VCTRL_RAW_TO_VOLT(raw)  ((float)(raw) * 3.3f / 4095.0f)

/** 전압[V] → DAC raw 변환 */
#define VCTRL_VOLT_TO_RAW(v)    ((uint32_t)((v) * 4095.0f / 3.3f))

/* ======================================================================== */
/*  채널 타입 정의                                                            */
/* ======================================================================== */

/**
 * @brief  출력 드라이버 종류
 */
typedef enum {
    VCTRL_OUT_DAC = 0,  /**< STM32H5 내부 DAC (채널 1, 2) */
    VCTRL_OUT_PWM       /**< TIM PWM + RC 필터 (채널 3, 4) */
} VCtrl_OutType_t;

/**
 * @brief  단일 채널 제어 구조체
 */
typedef struct {
    PID_t           pid;            /**< PID 제어기 인스턴스 */
    VCtrl_OutType_t out_type;       /**< 출력 드라이버 종류 */
    uint32_t        dac_channel;    /**< HAL DAC 채널 (DAC_CHANNEL_1/2) */
    uint32_t        tim_channel;    /**< HAL TIM 채널 (TIM_CHANNEL_1/2) */
    uint32_t        adc_rank;       /**< ADC 변환 순서 (0-based) */

    float           setpoint_v;     /**< 현재 목표 전압 [V] */
    float           feedback_v;     /**< 최근 ADC 피드백 전압 [V] */
    float           output_raw;     /**< 현재 PID 출력 (DAC/PWM raw) */
    float           error_v;        /**< 현재 오차 [V] */
} VCtrl_Channel_t;

/**
 * @brief  4채널 전압 제어기 최상위 구조체
 */
typedef struct {
    VCtrl_Channel_t ch[VCTRL_NUM_CHANNELS];
    bool            initialized;
} VCtrl_t;

/* ======================================================================== */
/*  API                                                                       */
/* ======================================================================== */

/**
 * @brief  전압 제어기 전체 초기화
 * @note   HAL 주변장치(DAC, TIM, ADC)는 이 함수 호출 전에 초기화 완료해야 함.
 */
void VCtrl_Init(void);

/**
 * @brief  특정 채널의 목표 전압을 설정한다.
 * @param  ch_idx  채널 인덱스 (0 ~ VCTRL_NUM_CHANNELS-1)
 * @param  volt    목표 전압 [V]  (0.0 ~ 3.3)
 */
void VCtrl_SetTarget(uint8_t ch_idx, float volt);

/**
 * @brief  ADC DMA 완료 후 피드백 데이터를 업데이트한다.
 * @param  adc_buf  ADC DMA 버퍼 포인터 (채널 수 = VCTRL_NUM_CHANNELS)
 */
void VCtrl_UpdateFeedback(const uint16_t *adc_buf);

/**
 * @brief  모든 채널의 PID 연산 및 DAC/PWM 출력을 수행한다.
 *         FreeRTOS 제어 태스크에서 주기적으로 호출.
 */
void VCtrl_RunControl(void);

/**
 * @brief  채널 상태 정보를 가져온다 (모니터링용).
 * @param  ch_idx   채널 인덱스
 * @param  out_ch   결과를 저장할 포인터 (복사본 반환)
 */
void VCtrl_GetChannelInfo(uint8_t ch_idx, VCtrl_Channel_t *out_ch);

/**
 * @brief  특정 채널의 PID를 활성화/비활성화한다.
 * @param  ch_idx  채널 인덱스
 * @param  enable  true: 활성화
 */
void VCtrl_SetEnabled(uint8_t ch_idx, bool enable);

#endif /* VOLTAGE_CTRL_H */
