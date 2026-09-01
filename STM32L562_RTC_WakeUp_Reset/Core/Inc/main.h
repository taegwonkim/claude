/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   STM32L562 - RTC Wakeup Timer 기반 주기적 소프트웨어 리셋
  *                   ("부팅 시점"으로부터 N초마다 리셋. 기본 24시간)
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

/* ===== 소프트웨어 리셋 주기 =============================================
 * 초 단위로 지정. 24시간 = 24 * 3600 = 86400초
 * (예: 60U=1분, 600U=10분, 3600U=1시간, 86400U=24시간)
 * 부팅(리셋) 시점부터 이 시간이 지나면 다시 리셋된다.
 * ===================================================================== */
#define RESET_PERIOD_SEC      (24U * 3600U)   /* 86400초 = 24시간 */

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
  #error "RESET_PERIOD_SEC 가 너무 큽니다(최대 131072초). 더 긴 주기는 Alarm A 프로젝트를 사용하세요."
#endif

/* LSI 는 오차가 ±5% 수준이라 장주기에서는 편차가 커진다(24h -> 최대 ±72분).
   장주기 사용 시 LSE(외부 32.768kHz 크리스탈) 를 강력히 권장한다. */
#if (RTC_CLOCK_LSE == 0U) && (RESET_PERIOD_SEC > 3600U)
  #warning "Long reset period with internal LSI (+/-5%). Set RTC_CLOCK_LSE to 1 for accuracy."
#endif

/* 살아있음(heartbeat) 로그 : HEARTBEAT_PERIOD_SEC 마다 uptime 과 리셋까지 남은
   시간을 UART 로 출력한다. USE_DEBUG_UART 가 1 일 때만 동작한다.
   장주기 리셋이 제대로 대기 중인지 눈으로 확인하는 용도. */
#define USE_HEARTBEAT_LOG     1U
#define HEARTBEAT_PERIOD_SEC  60U

/* 백업 레지스터(TAMP_BKPxR) 용도 정의 */
#define BKP_REG_MAGIC         RTC_BKP_DR0   /* 콜드부트 판별용 매직 값 */
#define BKP_REG_RESET_COUNT   RTC_BKP_DR1   /* 소프트 리셋 누적 횟수    */
#define BKP_MAGIC_VALUE       0xA5A5C3C3U

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
