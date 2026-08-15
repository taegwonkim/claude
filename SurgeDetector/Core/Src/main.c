/**
 * main.c — CubeMX 표준 구조 요약본.
 * 실제 프로젝트에서는 CubeMX가 생성하는 MX_GPIO_Init/MX_DMA_Init/MX_USARTx_UART_Init/
 * MX_SPI2_Init/MX_USB_Device_Init/SystemClock_Config 전체 구현이 이 파일에 포함됩니다.
 * 여기서는 CubeMX가 만들지 않는 "USER CODE" 삽입 지점만 표시합니다.
 */
#include "main.h"
#include "cmsis_os2.h"

/* CubeMX가 생성 (usart.c/spi.c/usb_device.c/dma.c/gpio.c) */
extern void MX_GPIO_Init(void);
extern void MX_DMA_Init(void);
extern void MX_USART1_UART_Init(void);
extern void MX_USART2_UART_Init(void);
extern void MX_USART3_UART_Init(void);
extern void MX_SPI2_Init(void);
extern void MX_USB_Device_Init(void);
extern void SystemClock_Config(void);

/* Core/Src/app_freertos.c */
extern void MX_FREERTOS_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();
    MX_SPI2_Init();
    MX_USB_Device_Init();

    /* USER CODE BEGIN 2 */
    /* App 모듈 자체 RTOS 오브젝트 생성은 MX_FREERTOS_Init() -> StartDefaultTask()
     * 안에서 수행되므로 여기서는 별도 초기화가 필요 없다. */
    /* USER CODE END 2 */

    MX_FREERTOS_Init();

    osKernelStart();

    /* osKernelStart()는 정상 동작 시 반환하지 않는다 */
    for (;;)
    {
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
