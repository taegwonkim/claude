/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l5xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l5xx_it.h"

/* External variables --------------------------------------------------------*/
extern RTC_HandleTypeDef hrtc;

/******************************************************************************/
/*           Cortex-M33 Processor Interruption and Exception Handlers         */
/******************************************************************************/

void NMI_Handler(void)
{
  while (1)
  {
  }
}

void HardFault_Handler(void)
{
  while (1)
  {
  }
}

void MemManage_Handler(void)
{
  while (1)
  {
  }
}

void BusFault_Handler(void)
{
  while (1)
  {
  }
}

void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

void SecureFault_Handler(void)
{
  while (1)
  {
  }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

/******************************************************************************/
/* STM32L5xx Peripheral Interrupt Handlers                                    */
/******************************************************************************/

/**
  * @brief  This function handles RTC global interrupt.
  * @note   STM32L5 는 Alarm / WakeUp / Timestamp 등 RTC 인터럽트가
  *         RTC_IRQn 하나로 통합되어 있다. 여기서는 Alarm 만 사용한다.
  */
void RTC_IRQHandler(void)
{
  /* USER CODE BEGIN RTC_IRQn 0 */
  /* USER CODE END RTC_IRQn 0 */

  HAL_RTC_AlarmIRQHandler(&hrtc);

  /* USER CODE BEGIN RTC_IRQn 1 */
  /* USER CODE END RTC_IRQn 1 */
}
