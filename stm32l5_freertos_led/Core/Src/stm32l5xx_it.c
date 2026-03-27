/**
 * @file stm32l5xx_it.c
 * @brief 인터럽트 서비스 루틴
 *
 * TIM6_DAC_IRQHandler 는 tim6_tick.c 에 구현되어 있음.
 * 이 파일에서는 NMI, HardFault, SysTick(HAL 전용) 등을 처리한다.
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* ---------------------------------------------------------------
 * Cortex-M33 예외 핸들러
 * --------------------------------------------------------------- */

/**
 * @brief NMI 핸들러
 */
void NMI_Handler(void)
{
    while (1) {}
}

/**
 * @brief HardFault 핸들러
 */
void HardFault_Handler(void)
{
    while (1) {}
}

/**
 * @brief MemManage 핸들러
 */
void MemManage_Handler(void)
{
    while (1) {}
}

/**
 * @brief BusFault 핸들러
 */
void BusFault_Handler(void)
{
    while (1) {}
}

/**
 * @brief UsageFault 핸들러
 */
void UsageFault_Handler(void)
{
    while (1) {}
}

/**
 * @brief SecureFault 핸들러 (Cortex-M33 TrustZone)
 */
void SecureFault_Handler(void)
{
    while (1) {}
}

/**
 * @brief DebugMon 핸들러
 */
void DebugMon_Handler(void)
{
}

/**
 * @brief SysTick 핸들러
 *
 * HAL 타임베이스 전용.
 * FreeRTOS 틱은 TIM6_DAC_IRQHandler(tim6_tick.c)에서 처리.
 *
 * 스케줄러 동작 중 SysTick 이 xPortSysTickHandler 를 호출하지 않도록
 * 여기서는 HAL_IncTick 만 호출한다.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ---------------------------------------------------------------
 * PendSV / SVC 핸들러는 FreeRTOS 포트 파일(port.c)에서 정의됨
 * (xPortPendSVHandler, vPortSVCHandler)
 * --------------------------------------------------------------- */
