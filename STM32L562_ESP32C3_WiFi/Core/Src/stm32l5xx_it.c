/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l5xx_it.c
  * @brief   Interrupt Service Routines.
  *
  *  폴링 방식이므로 주변장치 인터럽트는 없다. SysTick(HAL 1ms 틱)만 사용.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l5xx_it.h"

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
/* 없음 (UART 는 폴링) */
