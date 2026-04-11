/* adc_meas.h - ADC DMA 전압/전류 측정 인터페이스 */
#ifndef ADC_MEAS_H
#define ADC_MEAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"

/* ==========================================================
 * ADC DMA 버퍼 레이아웃 (채널 순서)
 *
 *  인덱스  ADC채널  핀   용도
 *  ------  -------  ---  --------
 *    0      IN5     PA0  CH0 전압
 *    1      IN6     PA1  CH0 전류
 *    2      IN7     PA2  CH1 전압
 *    3      IN8     PA3  CH1 전류
 *    4      IN1     PC0  CH2 전압
 *    5      IN2     PC1  CH2 전류
 *    6      IN3     PC2  CH3 전압
 *    7      IN4     PC3  CH3 전류
 * ========================================================== */
#define ADC_IDX_CH0_VOLT    0
#define ADC_IDX_CH0_CURR    1
#define ADC_IDX_CH1_VOLT    2
#define ADC_IDX_CH1_CURR    3
#define ADC_IDX_CH2_VOLT    4
#define ADC_IDX_CH2_CURR    5
#define ADC_IDX_CH3_VOLT    6
#define ADC_IDX_CH3_CURR    7

/* 소프트웨어 이동 평균 윈도우 크기 (2의 거듭제곱) */
#define ADC_AVG_WINDOW      8U
#define ADC_AVG_SHIFT       3U   /* log2(8) = 3 */

/* 변환 완료 플래그 */
extern volatile uint8_t g_adc_conv_cplt;

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */

/** @brief ADC DMA 변환 시작 */
void ADCMeas_Start(void);

/** @brief ADC DMA 변환 정지 */
void ADCMeas_Stop(void);

/**
 * @brief 채널 전압 반환 (mV)
 * @param ch  채널 번호 (0~3)
 */
uint32_t ADCMeas_GetVoltage_mV(uint8_t ch);

/**
 * @brief 채널 전류 반환 (mA)
 * @param ch  채널 번호 (0~3)
 */
uint32_t ADCMeas_GetCurrent_mA(uint8_t ch);

/**
 * @brief 이동 평균 필터 처리 (DMA 완료 콜백에서 호출)
 *        HAL_ADC_ConvCpltCallback() 에서 내부 호출됨
 */
void ADCMeas_ProcessDMA(void);

/** @brief ADC 교정(calibration) 실행 */
HAL_StatusTypeDef ADCMeas_Calibrate(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_MEAS_H */
