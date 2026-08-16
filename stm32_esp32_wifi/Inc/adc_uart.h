/*
 * adc_uart.h
 *
 * USART2로 들어오는 외부 ADC 장비의 데이터를 인터럽트로 수신해,
 * 가장 최근에 완성된 한 줄을 보관한다.
 *
 * 외부 ADC 장비가 "<value>\r\n" 형식의 ASCII 라인을 주기적으로 보낸다고
 * 가정한다. 실제 장비의 프로토콜(바이너리 프레임 등)이 다르면
 * adc_uart.c의 라인 조립 로직만 그에 맞게 바꾸면 되고, 이 헤더의
 * 인터페이스(Init/RxCpltCallback/GetLatest)는 그대로 재사용할 수 있다.
 *
 * ESP32/WiFi 상태와는 완전히 독립적으로 동작한다: USART2 인터럽트는
 * WiFi 재접속 시도로 메인 루프가 지연되는 것과 무관하게 계속 들어오고,
 * 여기 저장되는 값도 계속 최신 상태로 갱신된다.
 */

#ifndef ADC_UART_H
#define ADC_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l5xx_hal.h"

#define ADC_UART_LINE_MAX 64

void ADC_UART_Init(UART_HandleTypeDef *huart);

/* 해당 UART의 HAL_UART_RxCpltCallback()에서 호출 */
void ADC_UART_RxCpltCallback(UART_HandleTypeDef *huart);

/* 가장 최근에 수신 완료된 한 줄을 out에 복사한다.
 * 한 번도 수신하지 못했으면 false를 반환하고 out은 건드리지 않는다.
 * out_tick에는 그 줄이 수신 완료된 시각(HAL_GetTick())이 담긴다(NULL 가능). */
bool ADC_UART_GetLatest(char *out, uint16_t out_size, uint32_t *out_tick);

#endif /* ADC_UART_H */
