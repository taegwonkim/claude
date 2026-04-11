/* adc_meas.c - ADC DMA 전압/전류 측정 구현
 *
 * ADC1 연속 스캔 모드 + DMA (8채널)
 * 소프트웨어 이동 평균 (8샘플)으로 노이즈 감소
 *
 * 하드웨어 오버샘플링: STM32L5 ADC는 최대 256× 오버샘플링 지원
 *   → CubeMX에서 Oversampling Ratio=16, Right Shift=4 설정 권장
 */
#include "adc_meas.h"
#include <string.h>

/* ==========================================================
 * 내부 DMA 버퍼 및 이동 평균 필터
 * ========================================================== */
static uint16_t s_dma_buf[ADC_NUM_CHANNELS];          /* DMA 단일 프레임 */
static uint32_t s_avg_acc[ADC_NUM_CHANNELS];          /* 누산기 */
static uint16_t s_avg_buf[ADC_AVG_WINDOW][ADC_NUM_CHANNELS]; /* 링 버퍼 */
static uint8_t  s_avg_idx;                             /* 링 버퍼 인덱스 */
static uint32_t s_avg_out[ADC_NUM_CHANNELS];          /* 이동 평균 결과 */

volatile uint8_t g_adc_conv_cplt = 0U;

/* ==========================================================
 * ADCMeas_Calibrate
 * ========================================================== */
HAL_StatusTypeDef ADCMeas_Calibrate(void)
{
    return HAL_ADCEx_Calibration_Start(&hadc1,
                                       ADC_CALIB_OFFSET,
                                       ADC_SINGLE_ENDED);
}

/* ==========================================================
 * ADCMeas_Start
 * ========================================================== */
void ADCMeas_Start(void)
{
    memset(s_dma_buf,  0, sizeof(s_dma_buf));
    memset(s_avg_acc,  0, sizeof(s_avg_acc));
    memset(s_avg_buf,  0, sizeof(s_avg_buf));
    memset(s_avg_out,  0, sizeof(s_avg_out));
    s_avg_idx = 0;

    HAL_ADC_Start_DMA(&hadc1,
                      (uint32_t *)s_dma_buf,
                      ADC_NUM_CHANNELS);
}

/* ==========================================================
 * ADCMeas_Stop
 * ========================================================== */
void ADCMeas_Stop(void)
{
    HAL_ADC_Stop_DMA(&hadc1);
}

/* ==========================================================
 * ADCMeas_ProcessDMA
 *   DMA 완료 콜백(HAL_ADC_ConvCpltCallback)에서 호출
 *   이동 평균 링 버퍼 갱신
 * ========================================================== */
void ADCMeas_ProcessDMA(void)
{
    for (uint8_t i = 0; i < ADC_NUM_CHANNELS; i++) {
        /* 링 버퍼에서 가장 오래된 값 빼기 */
        s_avg_acc[i] -= s_avg_buf[s_avg_idx][i];
        /* 새 값 추가 */
        s_avg_buf[s_avg_idx][i] = s_dma_buf[i];
        s_avg_acc[i] += s_dma_buf[i];
        /* 이동 평균 결과 */
        s_avg_out[i] = s_avg_acc[i] >> ADC_AVG_SHIFT;
    }

    s_avg_idx = (s_avg_idx + 1U) % ADC_AVG_WINDOW;
    g_adc_conv_cplt = 1U;
}

/* ==========================================================
 * ADCMeas_GetVoltage_mV
 *
 * V_in = V_adc / divider_ratio
 * V_adc = (adc_val / 4096) * 3300 mV
 * V_in  = V_adc * (R1+R2) / R2
 *       = adc_val * 3300 * VOLT_DIV_DEN / (4096 * VOLT_DIV_NUM)  [mV]
 * ========================================================== */
uint32_t ADCMeas_GetVoltage_mV(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return 0U;

    uint8_t  idx = (uint8_t)(ch * 2U);          /* 전압은 짝수 인덱스 */
    uint32_t raw = s_avg_out[idx];

    /* mV 단위 환산: 정수 연산 (오버플로우 방지를 위해 64비트 사용) */
    uint32_t v_mV = (uint32_t)(
        ((uint64_t)raw * ADC_VREF_MV * VOLT_DIV_DEN)
        / ((uint64_t)ADC_RESOLUTION * VOLT_DIV_NUM)
    );

    return v_mV;
}

/* ==========================================================
 * ADCMeas_GetCurrent_mA
 *
 * I (A) = V_adc / (R_shunt * G_amp)
 * V_adc = (adc_val / 4096) * 3300 mV
 * I (A) = (adc_val * 3300) / (4096 * 3300 / I_max)
 *       = adc_val * I_max / 4096
 *   여기서 I_max = 5000 mA 가 풀스케일 3300mV에 대응
 * ========================================================== */
uint32_t ADCMeas_GetCurrent_mA(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return 0U;

    uint8_t  idx = (uint8_t)(ch * 2U + 1U);     /* 전류는 홀수 인덱스 */
    uint32_t raw = s_avg_out[idx];

    uint32_t i_mA = (uint32_t)(
        ((uint64_t)raw * CURRENT_MAX_MA) / ADC_RESOLUTION
    );

    return i_mA;
}

/* ==========================================================
 * HAL_ADC_ConvCpltCallback (weak 함수 오버라이드)
 *   DMA 완료 시 자동 호출 → 이동 평균 갱신
 * ========================================================== */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        ADCMeas_ProcessDMA();
    }
}
