/**
 ******************************************************************************
 * @file    main.h
 * @brief   STM32L552R 4채널 PID 전압 제어 시스템 - 메인 헤더
 *
 * Hardware Configuration:
 *   MCU: STM32L552RET6 (Cortex-M33, 110MHz)
 *   DAC: AD5641 x4 (14-bit, SPI) - 전압 출력 설정
 *   ADC: MCP3465R (4ch, 16-bit Delta-Sigma, SPI) - 전압 피드백 읽기
 *   RTOS: FreeRTOS
 *   전압 범위: 0.5V ~ 4.5V
 *
 * Pin Assignment:
 *   SPI1 (AD5641 DACs):
 *     PA5  - SPI1_SCK
 *     PA7  - SPI1_MOSI
 *     PA4  - DAC_CS0 (GPIO Output)
 *     PB0  - DAC_CS1 (GPIO Output)
 *     PB1  - DAC_CS2 (GPIO Output)
 *     PB2  - DAC_CS3 (GPIO Output)
 *
 *   SPI2 (MCP3465R ADC):
 *     PB13 - SPI2_SCK
 *     PB14 - SPI2_MISO
 *     PB15 - SPI2_MOSI
 *     PB12 - ADC_CS (GPIO Output)
 *     PC6  - ADC_IRQ (GPIO Input, EXTI)
 *
 *   USART2 (Debug/Monitor):
 *     PA2  - USART2_TX
 *     PA3  - USART2_RX
 *
 *   Status LEDs:
 *     PC8  - LED_STATUS (heartbeat)
 *     PC9  - LED_ERROR
 ******************************************************************************
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"

/* Exported defines ----------------------------------------------------------*/

/* Number of voltage control channels */
#define NUM_CHANNELS                4

/* Voltage range (volts) */
#define VOLTAGE_MIN                 0.5f
#define VOLTAGE_MAX                 4.5f
#define VOLTAGE_DEFAULT             2.5f

/* DAC reference voltage (AD5641 external Vref) */
#define DAC_VREF                    5.0f

/* ADC reference voltage (MCP3465R, internal 2.4V) */
#define ADC_VREF                    2.4f

/*
 * ADC voltage divider scale factor
 * 외부 전압 분배기(1:1)를 사용하여 0~5V 범위를 0~2.5V로 변환
 * 실제 전압 = ADC 읽기 값 × ADC_VOLTAGE_DIVIDER_SCALE
 */
#define ADC_VOLTAGE_DIVIDER_SCALE   2.0f

/* PID control loop period (ms) */
#define PID_CONTROL_PERIOD_MS       10

/* UART monitor update period (ms) */
#define MONITOR_PERIOD_MS           500

/* ---- SPI1 Pins (AD5641 DACs) ---- */
#define SPI1_SCK_PIN                GPIO_PIN_5
#define SPI1_SCK_PORT               GPIOA
#define SPI1_MOSI_PIN               GPIO_PIN_7
#define SPI1_MOSI_PORT              GPIOA

/* DAC Chip Select pins */
#define DAC_CS0_PIN                 GPIO_PIN_4
#define DAC_CS0_PORT                GPIOA
#define DAC_CS1_PIN                 GPIO_PIN_0
#define DAC_CS1_PORT                GPIOB
#define DAC_CS2_PIN                 GPIO_PIN_1
#define DAC_CS2_PORT                GPIOB
#define DAC_CS3_PIN                 GPIO_PIN_2
#define DAC_CS3_PORT                GPIOB

/* ---- SPI2 Pins (MCP3465R ADC) ---- */
#define SPI2_SCK_PIN                GPIO_PIN_13
#define SPI2_SCK_PORT               GPIOB
#define SPI2_MISO_PIN               GPIO_PIN_14
#define SPI2_MISO_PORT              GPIOB
#define SPI2_MOSI_PIN               GPIO_PIN_15
#define SPI2_MOSI_PORT              GPIOB

#define ADC_CS_PIN                  GPIO_PIN_12
#define ADC_CS_PORT                 GPIOB
#define ADC_IRQ_PIN                 GPIO_PIN_6
#define ADC_IRQ_PORT                GPIOC

/* ---- USART2 Pins ---- */
#define USART2_TX_PIN               GPIO_PIN_2
#define USART2_TX_PORT              GPIOA
#define USART2_RX_PIN               GPIO_PIN_3
#define USART2_RX_PORT              GPIOA

/* ---- Status LEDs ---- */
#define LED_STATUS_PIN              GPIO_PIN_8
#define LED_STATUS_PORT             GPIOC
#define LED_ERROR_PIN               GPIO_PIN_9
#define LED_ERROR_PORT              GPIOC

/* Exported types ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart2;

/* Exported functions --------------------------------------------------------*/
void Error_Handler(void);
void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
