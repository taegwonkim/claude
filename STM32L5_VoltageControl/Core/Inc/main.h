/**
 ******************************************************************************
 * @file    main.h
 * @brief   Main header file
 ******************************************************************************
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* Private defines -----------------------------------------------------------*/
/* GPIO Pin Definitions */
#define SPI1_CS_Pin         GPIO_PIN_0
#define SPI1_CS_GPIO_Port   GPIOB
#define LED_FAULT_Pin       GPIO_PIN_1
#define LED_FAULT_GPIO_Port GPIOB
#define LED_STATUS_Pin      GPIO_PIN_13
#define LED_STATUS_GPIO_Port GPIOC

/* Current Sense ADC Pins */
#define ISENSE_CH0_Pin      GPIO_PIN_0
#define ISENSE_CH0_Port     GPIOC
#define ISENSE_CH1_Pin      GPIO_PIN_1
#define ISENSE_CH1_Port     GPIOC
#define ISENSE_CH2_Pin      GPIO_PIN_2
#define ISENSE_CH2_Port     GPIOC
#define ISENSE_CH3_Pin      GPIO_PIN_3
#define ISENSE_CH3_Port     GPIOC

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
