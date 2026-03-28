/**
 * @file    adc_drv.h
 * @brief   ADC 드라이버 (전압/전류 8채널 DMA 연속 변환)
 *
 * ADC1 스캔 모드 + DMA 사용:
 *   인덱스 0 : PA0  (ADC1_INP0)  - V_SENSE_CH1
 *   인덱스 1 : PA1  (ADC1_INP1)  - V_SENSE_CH2
 *   인덱스 2 : PA2  (ADC1_INP6)  - V_SENSE_CH3
 *   인덱스 3 : PA3  (ADC1_INP7)  - V_SENSE_CH4
 *   인덱스 4 : PC0  (ADC1_INP10) - I_SENSE_CH1
 *   인덱스 5 : PC1  (ADC1_INP11) - I_SENSE_CH2
 *   인덱스 6 : PC2  (ADC1_INP12) - I_SENSE_CH3
 *   인덱스 7 : PC3  (ADC1_INP13) - I_SENSE_CH4
 *
 * 소프트웨어 오버샘플링(ADC_OVERSAMPLE_COUNT 회 평균)으로
 * 측정 노이즈 저감.
 */

#ifndef ADC_DRV_H
#define ADC_DRV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

/* =========================================================
 * ADC 설정 상수
 * ========================================================= */
#define ADC_NUM_CH          8U          /* 스캔 채널 수 */
#define ADC_OVERSAMPLE_CNT  16U         /* 소프트웨어 오버샘플 횟수 */

/* DMA 버퍼 크기 */
#define ADC_DMA_BUF_LEN     (ADC_NUM_CH * ADC_OVERSAMPLE_CNT)

/* 채널 인덱스 */
#define ADC_IDX_V1          0U
#define ADC_IDX_V2          1U
#define ADC_IDX_V3          2U
#define ADC_IDX_V4          3U
#define ADC_IDX_I1          4U
#define ADC_IDX_I2          5U
#define ADC_IDX_I3          6U
#define ADC_IDX_I4          7U

/* =========================================================
 * 전역 DMA 버퍼 (HAL_ADC_ConvCpltCallback에서 접근)
 * ========================================================= */
extern volatile uint16_t g_adc_dma_buf[ADC_DMA_BUF_LEN];

/* 오버샘플 완료 후 평균값 저장 배열 */
extern volatile uint16_t g_adc_avg[ADC_NUM_CH];

/* =========================================================
 * 함수 프로토타입
 * ========================================================= */

/**
 * @brief ADC 드라이버 초기화
 */
void ADC_Drv_Init(void);

/**
 * @brief DMA 연속 변환 시작
 */
void ADC_Drv_StartDMA(void);

/**
 * @brief 전압 채널 값을 mV 단위로 반환
 * @param v_idx  전압 채널 인덱스 (0~3)
 * @return 측정 전압 (mV), 분압기 보정 포함
 */
float ADC_Drv_GetVoltageMv(uint8_t v_idx);

/**
 * @brief 전류 채널 값을 mA 단위로 반환
 * @param i_idx  전류 채널 인덱스 (0~3)
 * @return 측정 전류 (mA)
 *
 * 계산식:
 *   Vadc  = (raw / 4095) * 3300  (mV)
 *   I(mA) = Vadc / (SHUNT_mΩ * 0.001 * AMP_GAIN)
 */
float ADC_Drv_GetCurrentMa(uint8_t i_idx);

/**
 * @brief DMA 변환 완료 여부 확인
 */
bool ADC_Drv_IsReady(void);

/**
 * @brief 변환 완료 플래그 클리어
 */
void ADC_Drv_ClearReady(void);

/**
 * @brief DMA 콜백에서 평균 계산 (HAL_ADC_ConvCpltCallback 내에서 호출)
 */
void ADC_Drv_ProcessDMA(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_DRV_H */
