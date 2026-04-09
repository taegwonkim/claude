/* =========================================================
 * stm32l5xx_it.c - 인터럽트 서비스 루틴 (Non-RTOS)
 *
 * Non-RTOS 핵심: ISR에서 플래그만 세팅하고 즉시 반환.
 * 실제 처리는 메인 루프(슈퍼루프)에서 수행.
 * → 인터럽트 응답 시간 최소화, CPU 점유율 예측 가능.
 * =========================================================*/
#include "main.h"
#include "stm32l5xx_hal.h"
#include "uart_protocol.h"

extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef  htim6;
extern TIM_HandleTypeDef  htim7;

/* =========================================================
 * Cortex-M33 예외 핸들러
 * =========================================================*/
void NMI_Handler(void)          { while(1){} }
void HardFault_Handler(void)    { while(1){} }
void MemManage_Handler(void)    { while(1){} }
void BusFault_Handler(void)     { while(1){} }
void UsageFault_Handler(void)   { while(1){} }
void SecureFault_Handler(void)  { while(1){} }
void SVC_Handler(void)          { }
void DebugMon_Handler(void)     { }
void PendSV_Handler(void)       { }

/* SysTick: HAL 틱 카운터 증가 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* =========================================================
 * TIM6 IRQ - 10ms 제어 주기
 * =========================================================*/
void TIM6_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

/* HAL TIM 주기완료 콜백 (TIM6, TIM7 공통) */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        /* 10ms 제어 주기 플래그 세팅 */
        g_flags.ctrl_tick = 1;
    }
    else if (htim->Instance == TIM7) {
        /* 20ms ADC 읽기 트리거 플래그 */
        g_flags.adc_tick = 1;
    }
}

/* =========================================================
 * TIM7 IRQ - 20ms ADC 트리거
 * =========================================================*/
void TIM7_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim7);
}

/* =========================================================
 * DMA1 채널1 IRQ (USART1_RX)
 * =========================================================*/
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

/* =========================================================
 * DMA1 채널2 IRQ (USART1_TX)
 * =========================================================*/
void DMA1_Channel2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

/* =========================================================
 * USART1 IRQ
 * IDLE 라인 감지 → uart_rx_ready 플래그
 * =========================================================*/
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* HAL UART RxEvent 콜백 (DMA + Idle line 감지 시 호출)
 * HAL_UARTEx_ReceiveToIdle_DMA 사용 시 이 콜백 사용 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *h, uint16_t Size)
{
    if (h->Instance == USART1) {
        /* Size = 현재까지 수신된 바이트 수 (DMA 카운터 기반) */
        g_flags.uart_rx_ready = 1;

        /* DMA 재시작 (Circular 모드에서는 자동이지만 명시적으로) */
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1,
                                      g_uart_rx_dma_buf,
                                      PROTO_RX_BUF_SIZE);
    }
}

/* HAL UART TX 완료 콜백 → TX 링버퍼 펌프 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h)
{
    if (h->Instance == USART1) {
        Proto_TxDoneCallback(); /* 다음 TX 항목 전송 */
    }
}

/* =========================================================
 * EXTI9_5 IRQ - MCP3465R IRQ 핀 (PB7)
 * =========================================================*/
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
}

/* GPIO EXTI 콜백 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_7) { /* MCP3465R IRQ */
        g_flags.adc_ready = 1;
    }
}
