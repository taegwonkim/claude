/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : STM32L562RCT6 - RTC WakeUp Timer 주기적 소프트웨어 리셋
  *                   + USART3(RS485) 리셋 메시지/횟수 전송
  *
  *  ※ 이 파일 하나만 고치면 주기/핀/기능을 모두 바꿀 수 있습니다.
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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* =========================================================================
 * 1. 리셋 주기 설정  ―  "분 단위" / "시간 단위" 두 가지
 *
 *    RESET_PERIOD_UNIT  : RESET_UNIT_MINUTE (분) 또는 RESET_UNIT_HOUR (시간)
 *    RESET_PERIOD_VALUE : 1 ~ 1000
 *
 *    예)  5분  -> UNIT = RESET_UNIT_MINUTE, VALUE = 5
 *         24시간 -> UNIT = RESET_UNIT_HOUR,   VALUE = 24
 *
 *    부팅(리셋) 시점으로부터 이 시간이 지나면 다시 소프트웨어 리셋된다.
 *    RS485 로 'm'/'h'/'+'/'-' 명령을 보내면 실행 중에도 바꿀 수 있다
 *    (단, 전원을 껐다 켜면 아래 컴파일 타임 기본값으로 돌아온다).
 * ======================================================================= */
#define RESET_UNIT_MINUTE       0U
#define RESET_UNIT_HOUR         1U

#define RESET_PERIOD_UNIT       RESET_UNIT_MINUTE
#define RESET_PERIOD_VALUE      5U

#if (RESET_PERIOD_VALUE == 0U) || (RESET_PERIOD_VALUE > 1000U)
  #error "RESET_PERIOD_VALUE 는 1 ~ 1000 이어야 합니다."
#endif

#if (RESET_PERIOD_UNIT == RESET_UNIT_HOUR)
  #define RESET_PERIOD_SEC      (RESET_PERIOD_VALUE * 3600U)
#else
  #define RESET_PERIOD_SEC      (RESET_PERIOD_VALUE * 60U)
#endif

/* WakeUp Timer 는 16bit 이므로 ck_spre(1Hz) 기준 한 번에 최대 65535초.
   그보다 긴 주기는 소프트웨어로 여러 번 나눠서(chunk) 재장전한다.
   따라서 주기 상한은 사실상 없다(예: 48시간도 가능). */
#define WUT_MAX_CHUNK_SEC       65535U

/* =========================================================================
 * 2. RS485 (USART3) 설정
 *
 *    PB10 = USART3_TX, PB11 = USART3_RX, PB14 = USART3_DE (Driver Enable)
 *
 *    RS485_USE_HW_DE = 1 : USART3 하드웨어 DE 기능 사용 (권장)
 *                          → 송신 시작/끝에 맞춰 DE 핀이 자동으로 토글됨
 *    RS485_USE_HW_DE = 0 : 임의의 GPIO 를 소프트웨어로 토글
 *                          → RS485_DE_GPIO_PORT / RS485_DE_PIN 을 쓰는 모드
 * ======================================================================= */
#define USE_RS485               1U
#define RS485_USE_HW_DE         1U
#define RS485_BAUDRATE          115200U
#define RS485_TX_TIMEOUT_MS     1000U

#define RS485_UART_INSTANCE     USART3
#define RS485_UART_AF           GPIO_AF7_USART3
#define RS485_GPIO_PORT         GPIOB
#define RS485_TX_PIN            GPIO_PIN_10
#define RS485_RX_PIN            GPIO_PIN_11
#define RS485_DE_PIN            GPIO_PIN_14
#define RS485_DE_GPIO_PORT      GPIOB

/* 하드웨어 DE 타이밍(샘플 시간 단위, 0~31).
   115200bps, OVERSAMPLING 16 기준 1 = 1/16 비트시간. 8 ≒ 0.5 비트시간. */
#define RS485_DE_ASSERT_TIME    8U
#define RS485_DE_DEASSERT_TIME  8U

/* RS485 수신 명령 사용 여부 (테스트에 매우 유용) */
#define USE_RS485_CMD           1U

/* =========================================================================
 * 3. 상태 LED / 하트비트 로그
 * ======================================================================= */
#define USE_STATUS_LED          1U
#define LED_GPIO_PORT           GPIOA
#define LED_PIN                 GPIO_PIN_5

#define USE_HEARTBEAT_LOG       1U
#define HEARTBEAT_PERIOD_SEC    30U     /* 몇 초마다 남은 시간 출력할지 */

/* =========================================================================
 * 4. RTC 클럭 소스
 *
 *    RTC_CLOCK_LSE = 1 : 외부 32.768kHz 크리스탈 (정확도 ppm 급, 권장)
 *    RTC_CLOCK_LSE = 0 : 내부 LSI 32kHz (크리스탈 불필요, 오차 ±5%)
 *
 *    !! CubeMX 의 RCC 설정과 반드시 일치시켜야 합니다 (README 3장 참고) !!
 *       LSE 로 설정했는데 크리스탈이 없으면 부팅이 Error_Handler() 에서 멈춥니다.
 * ======================================================================= */
#define RTC_CLOCK_LSE           0U

#if (RTC_CLOCK_LSE == 1U)
  #define RTC_ASYNC_PREDIV      127U   /* (127+1)*(255+1) = 32768 -> 1Hz */
  #define RTC_SYNC_PREDIV       255U
  #define RTC_CLOCK_NAME        "LSE 32768Hz"
#else
  #define RTC_ASYNC_PREDIV      127U   /* (127+1)*(249+1) = 32000 -> 1Hz */
  #define RTC_SYNC_PREDIV       249U
  #define RTC_CLOCK_NAME        "LSI 32000Hz(+-5%)"
#endif

#if (RTC_CLOCK_LSE == 0U) && (RESET_PERIOD_SEC > 3600U)
  #warning "LSI(+-5%) 로 장주기 사용 중 - 24h 기준 최대 +-72분 오차. LSE 권장."
#endif

/* =========================================================================
 * 5. VBAT 이 VDD 와 함께 on/off 되는 보드용 설정
 *
 *    이 보드는 VBAT 를 MCU 전원과 공용으로 쓰므로 전원을 끄면
 *    백업 도메인(RTC 달력 + TAMP 백업 레지스터)이 전부 지워진다.
 *      - 소프트웨어 리셋  -> 백업 도메인 유지  (WARM  boot)
 *      - 전원 off -> on   -> 백업 도메인 소실  (COLD  boot)
 *
 *    그래서 "전원을 꺼도 남는 누적 횟수"가 필요하면 내부 Flash 마지막
 *    페이지에 저장한다(USE_FLASH_COUNTER = 1).
 * ======================================================================= */
#define USE_FLASH_COUNTER       1U

/* 백업 레지스터(TAMP_BKPxR) 용도 ― 전원이 끊기면 전부 0 으로 돌아간다 */
#define BKP_REG_MAGIC           RTC_BKP_DR0   /* 콜드/웜 부트 판별 매직값      */
#define BKP_REG_RESET_COUNT     RTC_BKP_DR1   /* 전원 인가 후 RTC 리셋 횟수    */
#define BKP_REG_BOOT_COUNT      RTC_BKP_DR2   /* 전원 인가 후 부팅 횟수        */
#define BKP_REG_RUN_SEC         RTC_BKP_DR3   /* 전원 인가 후 누적 동작 시간[s]*/
#define BKP_REG_PENDING         RTC_BKP_DR4   /* "내가 건 리셋" 표식           */
#define BKP_REG_UNIT            RTC_BKP_DR5   /* 실행 중 변경된 단위           */
#define BKP_REG_VALUE           RTC_BKP_DR6   /* 실행 중 변경된 값             */

#define BKP_MAGIC_VALUE         0xA5A5C3C3U
#define BKP_PENDING_VALUE       0x52535421U   /* 'RST!' */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
