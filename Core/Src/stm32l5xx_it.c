/**
 ******************************************************************************
 * @file    stm32l5xx_it.c
 * @brief   인터럽트 서비스 루틴 (ISR)
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l5xx_it.h"

/* External variables --------------------------------------------------------*/
extern UART_HandleTypeDef huart2;

/* Cortex-M33 Core Interrupt Handlers ----------------------------------------*/

void NMI_Handler(void)
{
    while (1) {
    }
}

void HardFault_Handler(void)
{
    while (1) {
    }
}

void MemManage_Handler(void)
{
    while (1) {
    }
}

void BusFault_Handler(void)
{
    while (1) {
    }
}

void UsageFault_Handler(void)
{
    while (1) {
    }
}

void SecureFault_Handler(void)
{
    while (1) {
    }
}

void DebugMon_Handler(void)
{
}

/*
 * SVC_Handler와 PendSV_Handler는 FreeRTOS가 제공합니다.
 * (port.c에서 xPortSysTickHandler, vPortSVCHandler, xPortPendSVHandler)
 *
 * SysTick_Handler는 FreeRTOS에 의해 오버라이드되지만,
 * HAL_IncTick()도 호출해야 HAL_Delay()가 동작합니다.
 */

/**
 * @brief  SysTick 인터럽트 핸들러
 *
 * FreeRTOS가 SysTick을 사용하므로, 여기서는 HAL 틱만 증가시킵니다.
 * FreeRTOS의 xPortSysTickHandler()는 FreeRTOSConfig.h에서
 * configOVERRIDE_DEFAULT_TICK_CONFIGURATION에 따라 자동 처리됩니다.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();

#if (INCLUDE_xTaskGetSchedulerState == 1)
    extern void xPortSysTickHandler(void);
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
#else
    extern void xPortSysTickHandler(void);
    xPortSysTickHandler();
#endif
}

/* Peripheral Interrupt Handlers ---------------------------------------------*/

/**
 * @brief  USART2 인터럽트 핸들러
 */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}
