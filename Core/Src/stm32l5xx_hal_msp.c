/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32l5xx_hal_msp.c
  * @brief        MSP 초기화 (클럭 / GPIO / NVIC)
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

    /* RTC 커널 클럭 + RTC/TAMP 레지스터 인터페이스(APB) 클럭.
       RTCAPB 클럭이 없으면 TAMP 백업 레지스터를 읽고 쓸 수 없다. */
    __HAL_RCC_RTC_ENABLE();
    __HAL_RCC_RTCAPB_CLK_ENABLE();

    /* RTC interrupt Init
     * STM32L5 는 Alarm / WakeUp / Timestamp 인터럽트가 RTC_IRQn 하나로
     * 통합되어 있다. (TrustZone 프로젝트의 Secure 측은 RTC_S_IRQn) */
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

#if (USE_RS485 == 1U)
/**
  * @brief UART MSP Initialization  (USART3 = RS485)
  * @param huart: UART handle pointer
  *
  *   PB10 ------> USART3_TX   (AF7)
  *   PB11 ------> USART3_RX   (AF7)
  *   PB14 ------> USART3_DE   (AF7)  ... RS485_USE_HW_DE = 1 일 때만
  */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (huart->Instance == RS485_UART_INSTANCE)
  {
    /* Peripheral clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin       = RS485_TX_PIN | RS485_RX_PIN;
#if (RS485_USE_HW_DE == 1U)
    GPIO_InitStruct.Pin      |= RS485_DE_PIN;
#endif
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = RS485_UART_AF;
    HAL_GPIO_Init(RS485_GPIO_PORT, &GPIO_InitStruct);

#if (USE_RS485_CMD == 1U)
    /* 수신 인터럽트 (PC 에서 보내는 명령 처리용) */
    HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
#endif
  }
}

/**
  * @brief UART MSP De-Initialization
  * @param huart: UART handle pointer
  */
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance == RS485_UART_INSTANCE)
  {
    __HAL_RCC_USART3_CLK_DISABLE();
    HAL_GPIO_DeInit(RS485_GPIO_PORT, RS485_TX_PIN | RS485_RX_PIN);
#if (RS485_USE_HW_DE == 1U)
    HAL_GPIO_DeInit(RS485_GPIO_PORT, RS485_DE_PIN);
#endif
#if (USE_RS485_CMD == 1U)
    HAL_NVIC_DisableIRQ(USART3_IRQn);
#endif
  }
}
#endif /* USE_RS485 */
