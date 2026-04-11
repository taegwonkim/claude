/* main.h - STM32L552 4채널 PID 전압 제어 프로젝트 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

/* ==========================================================
 * 프로젝트 버전
 * ========================================================== */
#define APP_VERSION_MAJOR    1
#define APP_VERSION_MINOR    0
#define APP_VERSION_PATCH    0

/* ==========================================================
 * 채널 수
 * ========================================================== */
#define NUM_CHANNELS         4

/* ==========================================================
 * ADC 설정
 *   - VREF = 3.3V, 12bit 해상도 (0~4095)
 *   - DMA 버퍼: 채널당 전압+전류 = 2개 × 4채널 = 8개
 *   - 하드웨어 오버샘플링 16× → 우측 4비트 시프트
 * ========================================================== */
#define ADC_VREF_MV          3300U
#define ADC_RESOLUTION       4096U
#define ADC_NUM_CHANNELS     8U          /* 전압 4 + 전류 4 */
#define ADC_OVERSAMPLE_RATIO 16U

/* ==========================================================
 * 전압 측정 분압기
 *   출력 최대 12V → ADC 최대 3.3V
 *   R1=27kΩ, R2=10kΩ → ratio_num/ratio_den = 10k/37k
 * ========================================================== */
#define VOLT_DIV_NUM         10000UL     /* R2 (Ω) */
#define VOLT_DIV_DEN         37000UL     /* R1+R2 (Ω) */

/* ==========================================================
 * 전류 측정
 *   INA219(또는 동등 회로): 0.1Ω 션트, 게인=6.6
 *   0~5A → 0~3.3V (ADC 풀스케일)
 * ========================================================== */
#define CURRENT_MAX_MA       5000U       /* 최대 측정 전류 (mA) */

/* ==========================================================
 * PWM 출력 (TIM1, 4채널)
 *   PCLK2 = 110 MHz, PSC=0, ARR=1099 → f_PWM = 100 kHz
 *   12비트 분해능(0~1099 = ~10비트 실효)
 * ========================================================== */
#define PWM_ARR              1099U       /* Auto-reload 값 */
#define PWM_MAX_DUTY         PWM_ARR     /* 최대 PWM CCR 값 */

/* ==========================================================
 * 출력 전압 범위 및 보호 임계값
 * ========================================================== */
#define OUTPUT_VOLT_MIN_MV   0U
#define OUTPUT_VOLT_MAX_MV   12000U
#define OVP_THRESHOLD_MV     13000U      /* 과전압 보호 */
#define OCP_THRESHOLD_MA     5500U       /* 과전류 보호 */

/* ==========================================================
 * LED 핀 (Nucleo 기준)
 * ========================================================== */
#define LED_GREEN_Pin        GPIO_PIN_7
#define LED_GREEN_GPIO_Port  GPIOC
#define LED_RED_Pin          GPIO_PIN_6
#define LED_RED_GPIO_Port    GPIOC

/* ==========================================================
 * HAL 핸들 외부 선언 (main.c 에서 정의)
 * ========================================================== */
extern ADC_HandleTypeDef  hadc1;
extern DMA_HandleTypeDef  hdma_adc1;
extern TIM_HandleTypeDef  htim1;
extern FDCAN_HandleTypeDef hfdcan1;
extern IWDG_HandleTypeDef hiwdg;

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */
void SystemClock_Config(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
