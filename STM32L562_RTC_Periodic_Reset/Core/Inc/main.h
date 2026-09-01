/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   STM32L562 - RTC Wakeup Timer 기반 주기적 소프트웨어 리셋
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

/* ===== 소프트웨어 리셋 주기 =============================================
 * 초 단위로 지정. 24시간 = 24 * 3600 = 86400초
 * (예: 60U=1분, 600U=10분, 3600U=1시간, 86400U=24시간)
 * ===================================================================== */
#define RESET_PERIOD_SEC      (24U * 3600U)   /* 86400초 = 24시간 */

/* ===== 리셋 트리거 방식 선택 ============================================
 *  RESET_SRC_WAKEUP_TIMER : RTC Wakeup Timer.
 *                           "부팅 시점 기준" RESET_PERIOD_SEC 마다 리셋.
 *                           설정 한 줄이면 끝나고 자동 재장전된다.
 *  RESET_SRC_ALARM_A      : RTC Alarm A.
 *                           "벽시계 시각" 기준으로 리셋. 매일 03:00 처럼
 *                           고정된 시각에 리셋해야 할 때 사용한다.
 * ===================================================================== */
#define RESET_SRC_WAKEUP_TIMER   0U
#define RESET_SRC_ALARM_A        1U

#define RESET_SOURCE          RESET_SRC_WAKEUP_TIMER   /* <-- 여기만 바꾸면 방식 전환 */

/* ----- Alarm A 방식 세부 설정 (RESET_SOURCE == RESET_SRC_ALARM_A 일 때만 사용) --
 *  ALARM_MODE_DAILY_FIXED : 매일 정해진 시각(ALARM_RESET_HH:MM:SS)에 리셋.
 *                           알람이 자동 반복되므로 재장전이 필요 없다.
 *                           -> 주기는 항상 24시간 고정.
 *  ALARM_MODE_RELATIVE    : (현재 시각 + RESET_PERIOD_SEC) 에 알람을 걸고
 *                           콜백에서 다음 알람을 다시 건다.
 *                           -> RESET_PERIOD_SEC 는 86400초(24h) 이하만 가능.
 * ---------------------------------------------------------------------- */
#define ALARM_MODE_DAILY_FIXED   0U
#define ALARM_MODE_RELATIVE      1U

#define ALARM_MODE            ALARM_MODE_DAILY_FIXED

/* ALARM_MODE_DAILY_FIXED 에서 리셋할 시각 (24시간 표기) */
#define ALARM_RESET_HOUR      3U
#define ALARM_RESET_MINUTE    0U
#define ALARM_RESET_SECOND    0U

/* 콜드 부트 시 RTC 달력에 넣을 초기 시각.
 *   1U : 빌드 시각(__DATE__/__TIME__)을 사용 -> 별도 시각 동기화 없이도
 *        Alarm 의 "벽시계 시각" 이 대충 맞는다(플래싱 시점과의 오차는 존재).
 *   0U : 2000-01-01 00:00:00 으로 시작.
 * 실제 제품에서는 GPS/NTP/호스트 등에서 받은 시각으로 설정할 것. */
#define RTC_INIT_FROM_BUILD_TIME  1U

/* --- Wakeup Timer 파라미터 자동 계산 (수정 불필요) -----------------------
 * RTC WUT 카운터는 16bit 이므로 ck_spre(1Hz) 기준 최대 65536초(약 18.2h).
 * 그보다 긴 주기는 WUCKSEL=11x (CK_SPRE_17BITS) 로 2^16(65536)을 더해
 * 최대 131072초(약 36.4h)까지 만들 수 있다.
 *   - 16BITS 모드 : 주기 = (WUT + 1) 초
 *   - 17BITS 모드 : 주기 = (WUT + 1 + 65536) 초
 * ---------------------------------------------------------------------- */
#if   (RESET_PERIOD_SEC == 0U)
  #error "RESET_PERIOD_SEC 는 1 이상이어야 합니다."
#elif (RESET_PERIOD_SEC <= 65536U)
  #define WUT_CLOCK_SEL       RTC_WAKEUPCLOCK_CK_SPRE_16BITS
  #define WUT_COUNTER         (RESET_PERIOD_SEC - 1U)
#elif (RESET_PERIOD_SEC <= 131072U)
  #define WUT_CLOCK_SEL       RTC_WAKEUPCLOCK_CK_SPRE_17BITS
  #define WUT_COUNTER         (RESET_PERIOD_SEC - 65536U - 1U)
#else
  #error "RESET_PERIOD_SEC 가 너무 큽니다(최대 131072초). 더 긴 주기는 RTC Alarm 방식을 사용하세요."
#endif

/* Alarm A + RELATIVE 모드는 날짜를 마스킹하므로 24시간을 넘길 수 없다. */
#if (RESET_SOURCE == RESET_SRC_ALARM_A) && (ALARM_MODE == ALARM_MODE_RELATIVE) \
    && (RESET_PERIOD_SEC > 86400U)
  #error "Alarm RELATIVE 모드에서 RESET_PERIOD_SEC 는 86400초(24h)를 넘을 수 없습니다."
#endif

/* LSI 는 오차가 ±5% 수준이라 장주기에서는 편차가 커진다(24h -> 최대 ±72분).
   장주기 사용 시 LSE(외부 32.768kHz 크리스탈) 를 강력히 권장한다. */
#if (RTC_CLOCK_LSE == 0U) && (RESET_PERIOD_SEC > 3600U)
  #warning "Long reset period with internal LSI (+/-5%). Set RTC_CLOCK_LSE to 1 for accuracy."
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
