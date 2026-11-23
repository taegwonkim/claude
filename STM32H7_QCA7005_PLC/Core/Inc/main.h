/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   STM32H743 + QCA7005 HomePlug Green PHY (ISO 15118-3 SLAC)
  ******************************************************************************
  * Layout follows STM32CubeMX code generation (STM32H7_QCA7005_PLC.ioc):
  * only the USER CODE sections are kept when the code is regenerated.
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
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_conf.h"
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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
extern SPI_HandleTypeDef  hspi1;
extern UART_HandleTypeDef huart3;
extern RNG_HandleTypeDef  hrng;
extern ADC_HandleTypeDef  hadc1;
extern TIM_HandleTypeDef  htim1;
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_GREEN_Pin GPIO_PIN_0
#define LED_GREEN_GPIO_Port GPIOB
#define CP_ADC_Pin GPIO_PIN_3
#define CP_ADC_GPIO_Port GPIOA
#define QCA_SCK_Pin GPIO_PIN_5
#define QCA_SCK_GPIO_Port GPIOA
#define QCA_MISO_Pin GPIO_PIN_6
#define QCA_MISO_GPIO_Port GPIOA
#define CP_PWM_Pin GPIO_PIN_9
#define CP_PWM_GPIO_Port GPIOE
#define CP_S2_Pin GPIO_PIN_11
#define CP_S2_GPIO_Port GPIOE
#define QCA_INT_Pin GPIO_PIN_13
#define QCA_INT_GPIO_Port GPIOE
#define QCA_INT_EXTI_IRQn EXTI15_10_IRQn
#define QCA_RST_Pin GPIO_PIN_14
#define QCA_RST_GPIO_Port GPIOE
#define LED_RED_Pin GPIO_PIN_14
#define LED_RED_GPIO_Port GPIOB
#define STLINK_RX_Pin GPIO_PIN_8
#define STLINK_RX_GPIO_Port GPIOD
#define STLINK_TX_Pin GPIO_PIN_9
#define STLINK_TX_GPIO_Port GPIOD
#define QCA_CS_Pin GPIO_PIN_14
#define QCA_CS_GPIO_Port GPIOD
#define QCA_MOSI_Pin GPIO_PIN_5
#define QCA_MOSI_GPIO_Port GPIOB
#define LED_YELLOW_Pin GPIO_PIN_1
#define LED_YELLOW_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
