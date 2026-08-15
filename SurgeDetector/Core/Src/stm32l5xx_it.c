/**
 * stm32l5xx_it.c — 인터럽트 핸들러 및 HAL 콜백.
 * CubeMX가 각 IRQHandler 본체(HAL_UART_IRQHandler/HAL_DMA_IRQHandler/
 * HAL_GPIO_EXTI_IRQHandler 호출)를 자동 생성하므로, 여기서는 콜백에서 세마포어를
 * Release하는 애플리케이션 연동 코드만 표시한다. ISR에서의 FreeRTOS API 호출은
 * 반드시 ...FromISR 계열을 사용해야 하지만, CMSIS-RTOS2 osSemaphoreRelease()는
 * 내부적으로 ISR 컨텍스트를 자동 판별하여 FromISR 루틴으로 라우팅한다.
 */
#include "stm32l5xx_it.h"
#include "main.h"
#include "app_fpga.h"
#include "app_wifi.h"
#include "app_pccomm.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart2_rx;
extern DMA_HandleTypeDef  hdma_usart3_rx;

/* ---------------------- IRQ Handlers (CubeMX 자동생성 본체와 동일) ---------------- */
void EXTI4_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(FPGA_TRIGGER_Pin);
}

void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }
void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }
void USART3_IRQHandler(void) { HAL_UART_IRQHandler(&huart3); }

void DMA1_Channel1_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart1_rx); }
void DMA1_Channel2_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart2_rx); }
void DMA1_Channel3_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart3_rx); }

/* ---------------------- HAL 콜백 (weak 함수 재정의) ------------------------------ */

/* FPGA_TRIGGER(PB4) falling edge */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == FPGA_TRIGGER_Pin)
    {
        App_FPGA_OnTrigger();
    }
}

/* USART2: fpgaTask가 HAL_UART_Receive_DMA()로 시작한 고정 길이(20B) 수신 완료 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        App_FPGA_OnUartRxCplt();
    }
}

/* USART1(ESP32 AT 응답)/USART3(PC RS485) : IDLE 라인 검출 기반 가변 길이 수신 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        App_WiFi_OnUartRxEvent(Size);
    }
    else if (huart->Instance == USART3)
    {
        App_PcComm_OnUart3RxEvent(Size);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    /* 프레이밍/오버런 에러 시 해당 채널 재수신 시작 (각 태스크 루프가 timeout으로
     * 자체 복구하므로 여기서는 HAL 에러 플래그만 클리어) */
    __HAL_UNLOCK(huart);
}
