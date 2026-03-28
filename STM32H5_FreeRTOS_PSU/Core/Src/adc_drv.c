/**
 * @file    adc_drv.c
 * @brief   ADC 드라이버 구현 (DMA 연속 변환 + 소프트웨어 오버샘플링)
 */

#include "adc_drv.h"
#include <string.h>

/* =========================================================
 * 전역 버퍼
 * ========================================================= */
volatile uint16_t g_adc_dma_buf[ADC_DMA_BUF_LEN];
volatile uint16_t g_adc_avg[ADC_NUM_CH];

/* DMA 완료 플래그 (이진 세마포어 대신 간단한 플래그 사용) */
static volatile bool s_conv_ready = false;

/* =========================================================
 * 공개 함수 구현
 * ========================================================= */

void ADC_Drv_Init(void)
{
    /* DMA 버퍼 초기화 */
    memset((void *)g_adc_dma_buf, 0, sizeof(g_adc_dma_buf));
    memset((void *)g_adc_avg,     0, sizeof(g_adc_avg));
    s_conv_ready = false;

    /* ADC 교정 수행 (STM32H5는 시작 전 교정 권장) */
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET,
                                ADC_SINGLE_ENDED);
}

void ADC_Drv_StartDMA(void)
{
    /* DMA 순환 모드로 연속 변환 시작
     * ADC_DMA_BUF_LEN 만큼 채운 뒤 ConvCpltCallback 호출 */
    HAL_ADC_Start_DMA(&hadc1,
                      (uint32_t *)g_adc_dma_buf,
                      ADC_DMA_BUF_LEN);
}

float ADC_Drv_GetVoltageMv(uint8_t v_idx)
{
    if (v_idx >= 4U) {
        return 0.0f;
    }
    /* 오버샘플 평균값 → mV 변환 → 분압기 보정 */
    float raw_mv = ((float)g_adc_avg[v_idx] / (ADC_RESOLUTION - 1.0f))
                   * VREF_MV;
    return raw_mv * V_DIV_RATIO;
}

float ADC_Drv_GetCurrentMa(uint8_t i_idx)
{
    if (i_idx >= 4U) {
        return 0.0f;
    }
    /* 오버샘플 평균값 → mV 변환 */
    float raw_mv = ((float)g_adc_avg[ADC_IDX_I1 + i_idx]
                    / (ADC_RESOLUTION - 1.0f)) * VREF_MV;

    /* I(mA) = V_shunt(mV) / (R_shunt(Ω) * AMP_GAIN)
     *       = raw_mv / (SHUNT_mΩ * 0.001 * GAIN)   */
    float shunt_ohm = SHUNT_RESISTANCE_MOHM * 0.001f;
    float i_ma = raw_mv / (shunt_ohm * CURRENT_AMP_GAIN);
    return i_ma;
}

bool ADC_Drv_IsReady(void)
{
    return s_conv_ready;
}

void ADC_Drv_ClearReady(void)
{
    s_conv_ready = false;
}

/* =========================================================
 * DMA 완료 콜백에서 호출
 * - ADC_DMA_BUF_LEN 개 샘플을 ADC_NUM_CH 채널로 평균
 * ========================================================= */
void ADC_Drv_ProcessDMA(void)
{
    uint32_t sum[ADC_NUM_CH] = {0U};

    for (uint32_t s = 0U; s < ADC_OVERSAMPLE_CNT; s++) {
        for (uint32_t ch = 0U; ch < ADC_NUM_CH; ch++) {
            sum[ch] += g_adc_dma_buf[s * ADC_NUM_CH + ch];
        }
    }

    for (uint32_t ch = 0U; ch < ADC_NUM_CH; ch++) {
        g_adc_avg[ch] = (uint16_t)(sum[ch] / ADC_OVERSAMPLE_CNT);
    }

    s_conv_ready = true;
}

/* =========================================================
 * HAL 콜백 오버라이드 (weak 재정의)
 * ========================================================= */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        ADC_Drv_ProcessDMA();
        /* FreeRTOS 세마포어 신호 (ISR 전용 API 사용) */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xAdcSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
