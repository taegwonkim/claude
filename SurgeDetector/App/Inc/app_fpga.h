/**
 * app_fpga.h — Cyclone IV FPGA 서지 데이터 취득 태스크
 * USART2로 "START\r\n" 최초 1회 전송 후, FPGA_TRIGGER(EXTI4, falling edge) ->
 * USART2 DMA 20바이트 프레임 수신을 반복하며, 파싱된 데이터를 wifiTask/pcCommTask에 배포한다.
 */
#ifndef APP_INC_APP_FPGA_H_
#define APP_INC_APP_FPGA_H_

#include "cmsis_os2.h"

#ifdef __cplusplus
extern "C" {
#endif

extern osSemaphoreId_t semFpgaTrigger;   /* EXTI4 ISR -> Release */
extern osSemaphoreId_t semUart2RxCplt;   /* USART2 DMA RX 콜백 -> Release */

void App_FPGA_Init(void);

/* EXTI4 콜백(HAL_GPIO_EXTI_Callback)에서 GPIO_PIN_4일 때 호출 */
void App_FPGA_OnTrigger(void);

/* USART2 RxCplt/RxEvent 콜백에서 호출 */
void App_FPGA_OnUartRxCplt(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INC_APP_FPGA_H_ */
