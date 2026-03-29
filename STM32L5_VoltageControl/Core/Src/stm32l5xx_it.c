/* =========================================================
 * stm32l5xx_it.c - 인터럽트 서비스 루틴
 * =========================================================*/
#include "main.h"
#include "stm32l5xx_it.h"
#include "FreeRTOS.h"
#include "task.h"

/* 외부 핸들 참조 */
extern DMA_HandleTypeDef   hdma_usart1_rx;
extern DMA_HandleTypeDef   hdma_usart1_tx;
extern UART_HandleTypeDef  huart1;
extern SPI_HandleTypeDef   hspi1;
extern FDCAN_HandleTypeDef hfdcan1;

/* =========================================================
 * Cortex-M 예외 핸들러
 * =========================================================*/

void NMI_Handler(void)
{
    while (1) {}
}

void HardFault_Handler(void)
{
    /* HardFault 디버깅: SP, PC, LR를 읽어 폴트 원인 파악 */
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

/* SVC, PendSV, SysTick은 FreeRTOS가 처리 */
void SVC_Handler(void)          { /* FreeRTOS 포트 구현 */ }
void DebugMon_Handler(void)     { }
void PendSV_Handler(void)       { /* FreeRTOS 포트 구현 */ }
void SysTick_Handler(void)
{
    HAL_IncTick();
    /* FreeRTOS tick: xPortSysTickHandler()는 FreeRTOS 포트에서 처리 */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

/* =========================================================
 * DMA 인터럽트 핸들러
 * =========================================================*/
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

void DMA1_Channel2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

/* =========================================================
 * UART 인터럽트 핸들러
 * =========================================================*/
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* =========================================================
 * SPI 인터럽트 핸들러
 * =========================================================*/
void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi1);
}

/* =========================================================
 * EXTI 인터럽트 핸들러 (MCP3465R IRQ = PB7)
 * EXTI9_5: PB5~PB9 범위 커버
 * =========================================================*/
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7); /* PB7: MCP3465R IRQ */
}

/* =========================================================
 * FDCAN1 인터럽트 핸들러
 *
 * FDCAN1_IT0: RX FIFO0 수신 알림 → HAL_FDCAN_RxFifo0Callback
 * FDCAN1_IT1: TX 완료, 에러 등 (선택적)
 *
 * 우선순위: 6 (FreeRTOS API 사용 가능, ≥ configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5)
 * =========================================================*/
void FDCAN1_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}

void FDCAN1_IT1_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}
