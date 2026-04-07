# STM32L552 USART1 DMA(Normal) + IDLE Line 수신 파서

## 파일
- `uart_dma_parser.h` / `uart_dma_parser.c`

## CubeMX 설정
- **USART1**: Asynchronous, 115200-8-N-1
- **DMA**: USART1_RX, **Normal** mode, Peripheral → Memory, Byte/Byte
- **NVIC**: USART1 global interrupt, DMA1/2 Stream(USART1_RX) 인터럽트 Enable
- **FreeRTOS**: CMSIS_V2

## 연동 (stm32l5xx_it.c)
```c
#include "uart_dma_parser.h"

void USART1_IRQHandler(void)
{
    UART_DmaIdle_IRQHandler(&huart1);   // IDLE 처리 (DMA 중단/재시작 포함)
    HAL_UART_IRQHandler(&huart1);       // HAL 기본 처리
}
```

## 초기화 (main.c / app_freertos.c)
```c
#include "uart_dma_parser.h"

/* 모든 MX_xxx_Init() 이후, osKernelStart() 이전 또는
   디폴트 태스크 시작 시점에 호출 */
UART_DmaIdle_Start();

const osThreadAttr_t parserAttr = {
    .name       = "uartParser",
    .stack_size = 1024,
    .priority   = (osPriority_t)osPriorityNormal,
};
osThreadNew(UART_ParserTask, NULL, &parserAttr);
```

## 사용자 콜백
```c
void UART_OnFrameReceived(const UartFrame_t *frame)
{
    /* frame->data, frame->length 에 STX/ETX 를 제외한 DATA 가 들어있음 */
}
```

## 프레임
```
+------+----------+------+
| STX  |   DATA   | ETX  |
| 0x02 |  N bytes | 0x03 |
+------+----------+------+
```

## 동작 요약
1. `HAL_UART_Receive_DMA()` 로 Normal 모드 수신 시작, IDLE IT Enable
2. PC 가 프레임 송신 후 라인이 IDLE → USART1 IDLE 인터럽트 발생
3. `UART_DmaIdle_IRQHandler()` 에서 `NDTR` 로 수신 바이트 계산,
   StreamBuffer 에 push, DMA 재시작
4. DMA 버퍼가 꽉 찼을 경우 `HAL_UART_RxCpltCallback()` 으로도 push
5. `UART_ParserTask` 가 StreamBuffer 에서 꺼내 STX/ETX 상태머신으로
   프레임 조립 → Queue + `UART_OnFrameReceived()` 호출
