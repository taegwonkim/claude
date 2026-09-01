/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   STM32L562 - RTC Alarm A 기반 소프트웨어 리셋
  *                   ("벽시계 시각" 기준. 기본값 : 매일 03:00:00)
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

/* ===== 보드 설정 ===========================================================
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

/* ===== Alarm A 동작 모드 ================================================
 *  ALARM_MODE_DAILY_FIXED : 매일 정해진 시각(ALARM_RESET_HH:MM:SS)에 리셋.
 *                           AlarmMask 로 날짜를 제외하므로 알람이 매일
 *                           자동 반복된다 -> 주기는 24시간 고정.
 *  ALARM_MODE_RELATIVE    : (현재 시각 + RESET_PERIOD_SEC) 에 알람을 건다.
 *                           날짜를 마스킹하므로 86400초(24h) 이하만 가능.
 * ===================================================================== */
#define ALARM_MODE_DAILY_FIXED   0U
#define ALARM_MODE_RELATIVE      1U

#define ALARM_MODE            ALARM_MODE_DAILY_FIXED

/* ALARM_MODE_DAILY_FIXED 에서 리셋할 시각 (24시간 표기) */
#define ALARM_RESET_HOUR      3U
#define ALARM_RESET_MINUTE    0U
#define ALARM_RESET_SECOND    0U

/* ALARM_MODE_RELATIVE 에서 사용할 주기 [초] (86400초 이하) */
#define RESET_PERIOD_SEC      (24U * 3600U)   /* 86400초 = 24시간 */

#if (ALARM_MODE == ALARM_MODE_RELATIVE) && (RESET_PERIOD_SEC > 86400U)
  #error "ALARM_MODE_RELATIVE 에서 RESET_PERIOD_SEC 는 86400초(24h)를 넘을 수 없습니다."
#endif

/* ===== 콜드 부트 시 RTC 달력에 넣을 초기 시각 ==========================
 * Alarm 은 "벽시계 시각"을 보므로 달력이 실제 시각과 맞아야 의미가 있다.
 *   1U : 빌드 시각(__DATE__/__TIME__)을 사용 -> 별도 동기화 없이도 대략 맞음
 *        (빌드 ~ 플래싱 사이의 시간만큼 오차가 남는다)
 *   0U : 2000-01-01 00:00:00 으로 시작
 * 실제 제품에서는 GPS/NTP/호스트 등에서 받은 시각으로 설정할 것.
 * ===================================================================== */
#define RTC_INIT_FROM_BUILD_TIME  1U

/* LSI 는 오차가 ±5% 수준이라 하루에 최대 ±72분까지 벌어진다.
   "매일 정해진 시각"이 중요하다면 LSE(외부 크리스탈)를 반드시 사용할 것. */
#if (RTC_CLOCK_LSE == 0U)
  #warning "Alarm uses wall-clock time but internal LSI drifts +/-5%. Set RTC_CLOCK_LSE to 1."
#endif

/* 백업 레지스터(TAMP_BKPxR) 용도 정의 */
#define BKP_REG_MAGIC         RTC_BKP_DR0   /* 콜드부트 판별용 매직 값 */
#define BKP_REG_RESET_COUNT   RTC_BKP_DR1   /* 소프트 리셋 누적 횟수    */
#define BKP_MAGIC_VALUE       0xA5A5C3C3U

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
