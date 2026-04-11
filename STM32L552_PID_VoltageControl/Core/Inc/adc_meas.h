/* adc_meas.h - MCP3465R 외부 ADC 인터페이스 (SPI2)
 *
 * MCP3465R: 16비트 유효 Δ-Σ ADC, 8채널 스캔
 * 채널 배치 (단일 종단, VINCOM=AGND):
 *   CH0: 전압측정 채널0   CH1: 전류측정 채널0
 *   CH2: 전압측정 채널1   CH3: 전류측정 채널1
 *   CH4: 전압측정 채널2   CH5: 전류측정 채널2
 *   CH6: 전압측정 채널3   CH7: 전류측정 채널3
 *
 * 동작 방식:
 *   - Continuous Scan 모드: CH0→CH7 순환 변환
 *   - 변환 완료 시 /IRQ 핀 LOW → EXTI5 인터럽트
 *   - ISR에서 SPI로 ADCDATA 읽기 (32비트: CHID+DATA)
 */
#ifndef ADC_MEAS_H
#define ADC_MEAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"

/* 측정 결과 버퍼 (ISR에서 기록, 태스크에서 읽기) */
extern volatile int32_t  g_adc_raw[8];      /* 채널별 24비트 ADC 원시값 */
extern volatile uint8_t  g_adc_updated[8];  /* 채널 갱신 플래그 */

/* ISR ↔ 태스크 동기화 세마포어 */
extern SemaphoreHandle_t g_adc_irq_sem;

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */

/**
 * @brief MCP3465R 초기화 및 연속 스캔 시작
 *        모든 레지스터 설정 후 CONV_START 커맨드 전송
 */
HAL_StatusTypeDef ADCMeas_Init(void);

/**
 * @brief /IRQ EXTI ISR에서 호출: ADCDATA 읽기 + 버퍼 갱신
 *        HAL_GPIO_EXTI_Callback()에서 내부 호출됨
 */
void ADCMeas_IRQHandler(void);

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

/* 내부 헬퍼 (main.c 또는 테스트에서 직접 사용 가능) */
HAL_StatusTypeDef MCP3465R_WriteReg1(uint8_t reg, uint8_t data);
HAL_StatusTypeDef MCP3465R_WriteReg3(uint8_t reg, uint32_t data);
uint32_t          MCP3465R_ReadADCData(uint8_t *ch_id_out);

#ifdef __cplusplus
}
#endif
#endif /* ADC_MEAS_H */
