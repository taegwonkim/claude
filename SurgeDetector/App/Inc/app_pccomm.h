/**
 * app_pccomm.h — PC 설정 read/write 및 서지 데이터 통지 (USART3 RS485 + USB CDC)
 */
#ifndef APP_INC_APP_PCCOMM_H_
#define APP_INC_APP_PCCOMM_H_

#include "cmsis_os2.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* fpgaTask가 PC 통지용 데이터를 넣는 큐 (pcUartTask/pcUsbTask가 소비, 브로드캐스트) */
extern osMessageQueueId_t qFpgaToPc;

extern osSemaphoreId_t semUart3Event;

void App_PcComm_Init(void);

/* USART3 RxEvent(IDLE) 콜백에서 호출 */
void App_PcComm_OnUart3RxEvent(uint16_t size);

/* usbd_cdc_if.c의 CDC_Receive_FS()에서 호출: 수신 바이트를 pcUsbTask로 전달 */
void App_PcComm_OnUsbRx(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* APP_INC_APP_PCCOMM_H_ */
