/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   STM32L562 - RTC Wakeup Timer 기반 주기적 소프트웨어 리셋
  *                   ("부팅 시점"으로부터 N초마다 리셋)
  *                   RTC 클럭 : 내부 LSI(~32kHz) 전용
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
 *
 * RTC 클럭은 내부 LSI(~32kHz) 고정이다. 외부 크리스탈(LSE)을 쓰지 않으므로
 * 추가 부품 없이 어떤 보드에서도 동작하지만, LSI 오차(±5%)만큼 리셋 주기가
 * 흔들린다는 점을 전제로 한다. (README 1-3절 참고)
 * ========================================================================= */
#define USE_DEBUG_UART        1U
#define USE_STATUS_LED        1U

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
 * 부팅(리셋) 시점부터 이 시간이 지나면 다시 리셋된다.
 *
 * 지원 범위 : 60초(1분) ~ 108000초(30시간)
 *   1분   =        60U
 *   5분   = ( 5U * 60U)
 *   30분  = (30U * 60U)
 *   1시간 = ( 1U * 3600U)
 *   12시간= (12U * 3600U)
 *   24시간= (24U * 3600U)
 *   30시간= (30U * 3600U)   <- 현재 설정
 * ===================================================================== */
#define RESET_PERIOD_SEC      (30U * 3600U)   /* 108000초 = 30시간 */

/* 지원 범위 검사 */
#if   (RESET_PERIOD_SEC < 60U)
  #error "RESET_PERIOD_SEC 는 60초(1분) 이상이어야 합니다."
#elif (RESET_PERIOD_SEC > 108000U)
  #error "RESET_PERIOD_SEC 는 108000초(30시간) 이하여야 합니다."
#endif

/* --- Wakeup Timer 파라미터 자동 계산 (수정 불필요) -----------------------
 * RTC WUT 카운터는 16bit 이므로 ck_spre(1Hz) 기준 최대 65536초(약 18.2h).
 * 그보다 긴 주기는 WUCKSEL=11x (CK_SPRE_17BITS) 로 2^16(65536)을 더해
 * 최대 131072초(약 36.4h)까지 만들 수 있다.
 *   - 16BITS 모드 : 주기 = (WUT + 1) 초
 *   - 17BITS 모드 : 주기 = (WUT + 1 + 65536) 초
 * ---------------------------------------------------------------------- */
#if   (RESET_PERIOD_SEC <= 65536U)
  #define WUT_CLOCK_SEL       RTC_WAKEUPCLOCK_CK_SPRE_16BITS
  #define WUT_COUNTER         (RESET_PERIOD_SEC - 1U)
#elif (RESET_PERIOD_SEC <= 131072U)
  #define WUT_CLOCK_SEL       RTC_WAKEUPCLOCK_CK_SPRE_17BITS
  #define WUT_COUNTER         (RESET_PERIOD_SEC - 65536U - 1U)
#endif
/* 하드웨어 한계는 131072초(약 36.4h)이지만, 위에서 30시간으로 제한해 두었다. */

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
