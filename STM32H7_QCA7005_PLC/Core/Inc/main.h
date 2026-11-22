/**
  ******************************************************************************
  * @file    main.h
  * @brief   STM32H743 + QCA7005 HomePlug Green PHY (ISO 15118-3 SLAC)
  ******************************************************************************
  */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include "app_conf.h"

extern SPI_HandleTypeDef  hspi1;
extern UART_HandleTypeDef huart3;
extern RNG_HandleTypeDef  hrng;
extern ADC_HandleTypeDef  hadc1;
extern TIM_HandleTypeDef  htim1;

void Error_Handler(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
