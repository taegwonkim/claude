/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32l5xx_hal_msp.c
  * @brief        MSP 초기화 (클럭 / GPIO)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{
  /* USER CODE BEGIN MspInit 0 */
  /* USER CODE END MspInit 0 */

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* USER CODE BEGIN MspInit 1 */
  /* USER CODE END MspInit 1 */
}

/**
  * @brief UART MSP Initialization
  *
  *   USART1 (ESP32-C3 AT)   PA9  ---> USART1_TX (AF7)
  *                          PA10 <--- USART1_RX (AF7)
  *   USART2 (디버그 로그)   PA2  ---> USART2_TX (AF7)
  *                          PA3  <--- USART2_RX (AF7)
  *
  *  폴링 방식이므로 NVIC 는 설정하지 않는다.
  */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (huart->Instance == ESP_UART_INSTANCE)
  {
    /* USER CODE BEGIN USART1_MspInit 0 */
    /* USER CODE END USART1_MspInit 0 */

    ESP_UART_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin       = ESP_UART_TX_PIN | ESP_UART_RX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;    /* ESP32 리셋 중 RX 가 떠 있지 않게 */
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = ESP_UART_AF;
    HAL_GPIO_Init(ESP_UART_GPIO_PORT, &GPIO_InitStruct);

    /* USER CODE BEGIN USART1_MspInit 1 */
    /* USER CODE END USART1_MspInit 1 */
  }
#if (USE_DEBUG_LOG == 1U)
  else if (huart->Instance == DBG_UART_INSTANCE)
  {
    /* USER CODE BEGIN USART2_MspInit 0 */
    /* USER CODE END USART2_MspInit 0 */

    DBG_UART_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin       = DBG_UART_TX_PIN | DBG_UART_RX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = DBG_UART_AF;
    HAL_GPIO_Init(DBG_UART_GPIO_PORT, &GPIO_InitStruct);

    /* USER CODE BEGIN USART2_MspInit 1 */
    /* USER CODE END USART2_MspInit 1 */
  }
#endif
  else
  {
    /* 다른 UART 없음 */
  }
}

/**
  * @brief UART MSP De-Initialization
  */
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance == ESP_UART_INSTANCE)
  {
    ESP_UART_CLK_DISABLE();
    HAL_GPIO_DeInit(ESP_UART_GPIO_PORT, ESP_UART_TX_PIN | ESP_UART_RX_PIN);
  }
#if (USE_DEBUG_LOG == 1U)
  else if (huart->Instance == DBG_UART_INSTANCE)
  {
    DBG_UART_CLK_DISABLE();
    HAL_GPIO_DeInit(DBG_UART_GPIO_PORT, DBG_UART_TX_PIN | DBG_UART_RX_PIN);
  }
#endif
  else
  {
    /* 다른 UART 없음 */
  }
}
