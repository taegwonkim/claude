/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : STM32L562CET6 + ESP32-C3-WROOM (AT 커맨드, 폴링 방식)
  *                   Wi-Fi AP / TCP 서버 접속 및 자동 재접속
  *
  *  핀 배치 / 보레이트 / 기능 스위치는 이 파일에서 바꿉니다.
  *  AP·서버·DHCP 설정은 wifi_config.h 에 있습니다.
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
#include "wifi_config.h"
/* USER CODE END Includes */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* =========================================================================
 * 1. ESP32-C3 AT 포트  (USART1)
 *
 *    STM32 PA9  (USART1_TX, AF7) ---> ESP32-C3 GPIO6  (AT UART RX)
 *    STM32 PA10 (USART1_RX, AF7) <--- ESP32-C3 GPIO7  (AT UART TX)
 *    STM32 PB0  (GPIO Output)    ---> ESP32-C3 EN     (하드웨어 리셋)
 *    ESP32-C3 GPIO5 (CTS)        ---> GND  (흐름제어 미사용 시 반드시 L 로)
 *
 *    ESP-AT 기본 115200-8-N-1. ESP32-C3 AT 펌웨어의 명령 포트는 UART1
 *    (GPIO6/7) 이고, GPIO20/21 은 로그/다운로드용 UART0 이므로 헷갈리지 말 것.
 *    두 칩 모두 3.3V 로직이라 레벨 변환은 필요 없다.
 * ======================================================================= */
#define ESP_UART_INSTANCE       USART1
#define ESP_UART_BAUDRATE       115200U
#define ESP_UART_GPIO_PORT      GPIOA
#define ESP_UART_TX_PIN         GPIO_PIN_9
#define ESP_UART_RX_PIN         GPIO_PIN_10
#define ESP_UART_AF             GPIO_AF7_USART1
#define ESP_UART_CLK_ENABLE()   __HAL_RCC_USART1_CLK_ENABLE()
#define ESP_UART_CLK_DISABLE()  __HAL_RCC_USART1_CLK_DISABLE()

#define ESP_EN_GPIO_PORT        GPIOB
#define ESP_EN_PIN              GPIO_PIN_0
/* EN 을 L 로 유지하는 시간. ESP32-C3 는 최소 수십 us 면 되지만 넉넉히. */
#define ESP_EN_RESET_PULSE_MS   50U
/* 리셋 후 "ready" 를 기다리는 최대 시간 */
#define ESP_READY_TIMEOUT_MS    5000U

/* =========================================================================
 * 2. 디버그 로그 포트 (USART2 -> USB-Serial -> PC 터미널)
 *
 *    STM32 PA2 (USART2_TX, AF7) ---> USB-Serial RX
 *    STM32 PA3 (USART2_RX, AF7) <--- USB-Serial TX (현재는 사용 안 함)
 *
 *    USE_DEBUG_LOG = 0 이면 로그 관련 코드가 통째로 빠진다.
 *    ESP_AT_DEBUG  = 1 이면 ESP32 와 주고받는 AT 문자열을 그대로 찍는다.
 * ======================================================================= */
#define USE_DEBUG_LOG           1U
#define ESP_AT_DEBUG            1U

#define DBG_UART_INSTANCE       USART2
#define DBG_UART_BAUDRATE       115200U
#define DBG_UART_GPIO_PORT      GPIOA
#define DBG_UART_TX_PIN         GPIO_PIN_2
#define DBG_UART_RX_PIN         GPIO_PIN_3
#define DBG_UART_AF             GPIO_AF7_USART2
#define DBG_UART_CLK_ENABLE()   __HAL_RCC_USART2_CLK_ENABLE()
#define DBG_UART_CLK_DISABLE()  __HAL_RCC_USART2_CLK_DISABLE()

/* =========================================================================
 * 3. 상태 LED (PA5)  ― 상태별 점멸 패턴은 main.c 참고
 * ======================================================================= */
#define STATUS_LED_GPIO_PORT    GPIOA
#define STATUS_LED_PIN          GPIO_PIN_5
#define STATUS_LED_ACTIVE_HIGH  1U

/* =========================================================================
 * 4. AT 드라이버 버퍼 크기
 * ======================================================================= */
#define ESP_LINE_BUF_SIZE       256U   /* 한 줄(응답/URC) 최대 길이             */
#define ESP_RESP_BUF_SIZE       512U   /* 명령 하나의 응답 전체 (여러 줄)        */
#define ESP_DATA_BUF_SIZE       1024U  /* 서버에서 받은 데이터(+IPD) 링버퍼      */
#define ESP_TX_TIMEOUT_MS       1000U  /* UART 송신 블로킹 타임아웃              */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
