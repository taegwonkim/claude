/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   STM32L562 - RTC Wakeup Timer 기반 10분 주기 소프트웨어 리셋
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* ===== 사용자 설정 =========================================================
 * 보드에 맞게 아래 3개만 바꾸면 됩니다.
 *  - USE_DEBUG_UART : 0 이면 UART 로그 없이 동작 (핀/보드 상관없이 동작)
 *  - USE_STATUS_LED : 0 이면 LED 하트비트 사용 안 함
 *  - RTC_CLOCK_LSE  : 1 이면 32.768kHz 외부 크리스탈(LSE) 사용
 * ========================================================================= */
#define USE_DEBUG_UART        1U
#define USE_STATUS_LED        1U
#define RTC_CLOCK_LSE         0U   /* 0 = LSI(내부 32kHz), 1 = LSE(외부 32.768kHz) */

/* 디버그 UART : USART1 (PA9 = TX, PA10 = RX) — 보드에 맞게 수정 */
#define DBG_UART_INSTANCE     USART1
#define DBG_UART_GPIO_PORT    GPIOA
#define DBG_UART_TX_PIN       GPIO_PIN_9
#define DBG_UART_RX_PIN       GPIO_PIN_10
#define DBG_UART_AF           GPIO_AF7_USART1

/* 상태 LED : PA5 — 보드에 맞게 수정 (예: NUCLEO-L552ZE-Q 의 LD1 = PC7) */
#define LED_GPIO_PORT         GPIOA
#define LED_PIN               GPIO_PIN_5

/* 소프트웨어 리셋 주기 [초] : 10분 = 600초 */
#define RESET_PERIOD_SEC      600U

/* 백업 레지스터(TAMP_BKPxR) 용도 정의 */
#define BKP_REG_MAGIC         RTC_BKP_DR0   /* 콜드부트 판별용 매직 값 */
#define BKP_REG_RESET_COUNT   RTC_BKP_DR1   /* 소프트 리셋 누적 횟수    */
#define BKP_MAGIC_VALUE       0xA5A5C3C3U

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
