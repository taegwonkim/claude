/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.h
  * @brief   STM32H5 FileX + GPDMA1/2 프로젝트 메인 헤더
  *
  *  - GPDMA1: SDMMC1 (SD 카드 TX/RX)
  *  - GPDMA2: USART1 (디버그 UART TX/RX)
  *  - FileX:  RTOS 없이 SD 카드 파일시스템
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h5xx_hal.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* ---- SDMMC1 / SD Card 핀 ---- */
#define SD_DETECT_PIN       GPIO_PIN_3
#define SD_DETECT_GPIO_PORT GPIOD
#define SD_DETECT_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()

/* ---- USART1 핀 ---- */
#define USART1_TX_PIN       GPIO_PIN_9
#define USART1_TX_GPIO_PORT GPIOA
#define USART1_RX_PIN       GPIO_PIN_10
#define USART1_RX_GPIO_PORT GPIOA

/* ---- DMA Transfer Timeout (ms) ---- */
#define SD_DMA_TIMEOUT_MS       5000U
#define UART_DMA_TIMEOUT_MS     1000U

/* ---- UART 수신 버퍼 크기 ---- */
#define UART_RX_BUF_SIZE        256U
#define UART_TX_BUF_SIZE        512U

/* Exported variables --------------------------------------------------------*/
extern SD_HandleTypeDef  hsd1;
extern DMA_HandleTypeDef hdma_sdmmc1_tx;
extern DMA_HandleTypeDef hdma_sdmmc1_rx;

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_tx;
extern DMA_HandleTypeDef  hdma_usart1_rx;

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* Private defines -----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
