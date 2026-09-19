/**
 * fpga_link.h
 *
 * FPGA -> MCU 인터페이스: GPIO EXTI falling-edge 트리거(PH1, 핀 라벨 "FROM_FPGA") + USART2로
 * 도착하는 ADC 측정값 라인("ADC <seq> <sample0> ...\r\n") 수신. docs/프로토콜_명세.md §3 참고.
 */
#ifndef FPGA_LINK_H
#define FPGA_LINK_H

#include <stdint.h>
#include "usart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FPGA_LINE_TIMEOUT_MS (200U) /* 트리거 후 USART2 라인 도착 대기 타임아웃 */

/** USART2 idle-DMA 수신 시작, 트리거 세마포어 생성. RTOS 스케줄러 시작 후 호출. */
void FpgaLink_Init(void);

/** HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)에서 호출 (내부에서 트리거 핀인지 확인). */
void FpgaLink_OnExti(uint16_t gpio_pin);

/** HAL_UARTEx_RxEventCallback에서 호출 (내부에서 USART2 인스턴스인지 확인). */
void FpgaLink_OnUartRxEvent(UART_HandleTypeDef *huart, uint16_t pos);

/** FPGA_Task 본체 (app_freertos.c에서 osThreadNew로 생성). */
void FpgaLink_Task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FPGA_LINK_H */
