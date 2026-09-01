/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32l5xx_hal_msp.c
  * @brief        This file provides code for the MSP Initialization
  *               and de-Initialization codes.
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
  * @brief RTC MSP Initialization
  * @param hrtc: RTC handle pointer
  */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc)
{
  if (hrtc->Instance == RTC)
  {
    /* USER CODE BEGIN RTC_MspInit 0 */
    /* USER CODE END RTC_MspInit 0 */

    /* RTC 커널 클럭 및 RTC/TAMP 레지스터 인터페이스(APB) 클럭 활성화 */
    __HAL_RCC_RTC_ENABLE();
    __HAL_RCC_RTCAPB_CLK_ENABLE();

    /* RTC interrupt Init
     * STM32L5 는 RTC 관련 인터럽트가 RTC_IRQn 하나로 통합되어 있다.
     * (TrustZone 활성화 프로젝트의 Secure 측에서는 RTC_S_IRQn 사용) */
    HAL_NVIC_SetPriority(RTC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);

    /* USER CODE BEGIN RTC_MspInit 1 */
    /* USER CODE END RTC_MspInit 1 */
  }
}

/**
  * @brief RTC MSP De-Initialization
  * @param hrtc: RTC handle pointer
  */
void HAL_RTC_MspDeInit(RTC_HandleTypeDef *hrtc)
{
  if (hrtc->Instance == RTC)
  {
    __HAL_RCC_RTC_DISABLE();
    __HAL_RCC_RTCAPB_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(RTC_IRQn);
  }
}

#if (USE_DEBUG_UART == 1U)
/**
  * @brief UART MSP Initialization
  * @param huart: UART handle pointer
  */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (huart->Instance == DBG_UART_INSTANCE)
  {
    /* Peripheral clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /**USART1 GPIO Configuration
      PA9  ------> USART1_TX
      PA10 ------> USART1_RX
      */
    GPIO_InitStruct.Pin = DBG_UART_TX_PIN | DBG_UART_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = DBG_UART_AF;
    HAL_GPIO_Init(DBG_UART_GPIO_PORT, &GPIO_InitStruct);
  }
}

/**
  * @brief UART MSP De-Initialization
  * @param huart: UART handle pointer
  */
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance == DBG_UART_INSTANCE)
  {
    __HAL_RCC_USART1_CLK_DISABLE();
    HAL_GPIO_DeInit(DBG_UART_GPIO_PORT, DBG_UART_TX_PIN | DBG_UART_RX_PIN);
  }
}
#endif /* USE_DEBUG_UART */
