/**
 * @file    stm32l5xx_it.c
 * @brief   인터럽트 서비스 루틴 (ISR) 모음
 */
#include "main.h"
#include "stm32l5xx_it.h"
#include "FreeRTOS.h"
#include "task.h"

/* ================================================================== */
/*  Cortex-M33 핵심 예외 핸들러                                           */
/* ================================================================== */
void NMI_Handler(void)
{
    while (1) {}
}

void HardFault_Handler(void)
{
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

void SecureFault_Handler(void)
{
    while (1) {}
}

/* FreeRTOS SVC / PendSV / SysTick 핸들러 */
void SVC_Handler(void)        { vPortSVCHandler(); }
void PendSV_Handler(void)     { xPortPendSVHandler(); }
void SysTick_Handler(void)    { xPortSysTickHandler(); }

/* ================================================================== */
/*  EXTI Line0 — MCP3465R ADC INT 핀 (PC0)                            */
/* ================================================================== */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(ADC_INT_PIN);
}

/**
 * @brief EXTI 콜백 — MCP3465R 변환 완료 인터럽트
 *
 * 현재 구현: MCP3465R_WaitReady()가 INT 핀 폴링으로 동작하므로
 * 이 콜백은 빈 상태로 둡니다.
 *
 * 세마포어 기반 전환 시:
 *   extern osSemaphoreId_t g_adcReadySem;
 *   if (GPIO_Pin == ADC_INT_PIN) osSemaphoreRelease(g_adcReadySem);
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    (void)GPIO_Pin;
    /* 현재: 폴링 방식 사용 — 인터럽트 플래그는 HAL이 자동 클리어 */
}
