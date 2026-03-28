#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 * main.h - STM32L5 전압 제어 시스템 헤더
 * =========================================================*/

/* HAL 드라이버 */
#include "stm32l5xx_hal.h"

/* HAL 핸들 선언 (main.c에서 정의) */
extern SPI_HandleTypeDef  hspi1;
extern TIM_HandleTypeDef  htim2;
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

/* 에러 핸들러 */
void Error_Handler(void);

/* ----- 핀 정의 (CubeMX 스타일) ----- */
/* PWM 출력 핀 */
#define VOLT_CH1_Pin        GPIO_PIN_0
#define VOLT_CH1_GPIO_Port  GPIOA
#define VOLT_CH2_Pin        GPIO_PIN_1
#define VOLT_CH2_GPIO_Port  GPIOA
#define VOLT_CH3_Pin        GPIO_PIN_2
#define VOLT_CH3_GPIO_Port  GPIOA
#define VOLT_CH4_Pin        GPIO_PIN_3
#define VOLT_CH4_GPIO_Port  GPIOA

/* UART 핀 */
#define UART_TX_Pin         GPIO_PIN_9
#define UART_TX_GPIO_Port   GPIOA
#define UART_RX_Pin         GPIO_PIN_10
#define UART_RX_GPIO_Port   GPIOA

/* SPI 핀 */
#define SPI_SCK_Pin         GPIO_PIN_5
#define SPI_SCK_GPIO_Port   GPIOA
#define SPI_MISO_Pin        GPIO_PIN_6
#define SPI_MISO_GPIO_Port  GPIOA
#define SPI_MOSI_Pin        GPIO_PIN_7
#define SPI_MOSI_GPIO_Port  GPIOA

/* MCP3465R 제어 핀 */
#define ADC_CS_Pin          GPIO_PIN_6
#define ADC_CS_GPIO_Port    GPIOB
#define ADC_IRQ_Pin         GPIO_PIN_7
#define ADC_IRQ_GPIO_Port   GPIOB

/* LED 핀 */
#define LED_STATUS_Pin      GPIO_PIN_7
#define LED_STATUS_GPIO_Port GPIOC

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
