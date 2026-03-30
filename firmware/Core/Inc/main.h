/**
 * @file    main.h
 * @brief   Main header - Pin definitions and function prototypes
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

/* Exported functions prototypes */
void Error_Handler(void);

/* ========================================================================
 * Pin Definitions (matching CubeMX label assignments)
 * ======================================================================== */

/* SPI1 CS - MCP3465R ADC */
#define ADC_CS_Pin          GPIO_PIN_4
#define ADC_CS_GPIO_Port    GPIOA

/* SPI2 CS - AD5641 DAC x4 */
#define DAC_CS0_Pin         GPIO_PIN_12
#define DAC_CS0_GPIO_Port   GPIOB

#define DAC_CS1_Pin         GPIO_PIN_14
#define DAC_CS1_GPIO_Port   GPIOB

#define DAC_CS2_Pin         GPIO_PIN_1
#define DAC_CS2_GPIO_Port   GPIOB

#define DAC_CS3_Pin         GPIO_PIN_2
#define DAC_CS3_GPIO_Port   GPIOB

/* Status LEDs */
#define LED_STATUS_Pin      GPIO_PIN_13
#define LED_STATUS_GPIO_Port GPIOC

#define LED_FAULT_Pin       GPIO_PIN_14
#define LED_FAULT_GPIO_Port GPIOC

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
