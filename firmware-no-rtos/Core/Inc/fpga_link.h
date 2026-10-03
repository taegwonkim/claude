/**
 * fpga_link.h
 *
 * FPGA <-> MCU 인터페이스 (RTOS 미사용 버전): 부팅 후 1회 측정 개시 커맨드 송신(USART2 TX) +
 * GPIO EXTI falling-edge 트리거(PH1, 핀 라벨 "FROM_FPGA") + USART2로 도착하는 ADC 측정값
 * 라인("ADC <seq> <sample0> ...\r\n") 수신. docs/프로토콜_명세.md §3 참고.
 */
#ifndef FPGA_LINK_H
#define FPGA_LINK_H

#include <stdint.h>
#include "usart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FPGA_LINE_TIMEOUT_MS (200U) /* 트리거 후 USART2 라인 도착 대기 타임아웃 */

/** USART2 idle-DMA 수신 시작 + FPGA START 커맨드 1회 송신. main()의 App_Init() 단계에서 호출. */
void FpgaLink_Init(void);

/** HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)에서 호출 (내부에서 트리거 핀인지 확인, 플래그만 세팅). */
void FpgaLink_OnExti(uint16_t gpio_pin);

/** HAL_UARTEx_RxEventCallback에서 호출 (내부에서 USART2 인스턴스인지 확인). */
void FpgaLink_OnUartRxEvent(UART_HandleTypeDef *huart, uint16_t pos);

/**
 * @brief super-loop 매 반복 호출(논블로킹): 트리거 플래그가 없으면 즉시 반환. 트리거가 있으면
 *        최대 FPGA_LINE_TIMEOUT_MS(200ms) 동안 ADC 라인 도착을 기다린 뒤 파싱/전송한다 —
 *        이 대기 구간 동안은 App_Run()의 다른 폴링(ESP32/PC통신)이 지연된다는 점에 유의
 *        (firmware-no-rtos/README.md 참고).
 */
void FpgaLink_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* FPGA_LINK_H */
