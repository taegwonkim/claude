/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt handlers. SPI/ADC/RNG run in polling mode; the only
  *          peripheral interrupt is the QCA7005 INTR line (EXTI13).
  ******************************************************************************
  */
#include "main.h"
#include "stm32h7xx_it.h"

void NMI_Handler(void)
{
  while (1) {}
}

void HardFault_Handler(void)
{
  HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
  while (1) {}
}

void MemManage_Handler(void)
{
  while (1) {}
}

void BusFault_Handler(void)
{
  while (1) {}
}

void UsageFault_Handler(void)
{
  while (1) {}
}

void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

/* QCA7005 INTR (PE13) -> HAL_GPIO_EXTI_Callback in qca7k_port_stm32.c */
void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(QCA_INT_PIN);
}
