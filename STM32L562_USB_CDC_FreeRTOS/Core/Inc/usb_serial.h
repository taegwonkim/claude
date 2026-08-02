#ifndef __USB_SERIAL_H
#define __USB_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* timeoutMs에 UINT32_MAX(=osWaitForever)를 넘기면 무한 대기 */
void USB_Serial_RTOS_Init(void);
uint8_t USB_Serial_IsConnected(void);

int32_t USB_Serial_Transmit(const uint8_t *data, uint16_t len, uint32_t timeoutMs);
uint16_t USB_Serial_Receive(uint8_t *buf, uint16_t maxLen, uint32_t timeoutMs);

/* usbd_cdc_if.c에서 USB 인터럽트 컨텍스트로 호출되는 함수 */
void USB_Serial_PushRxDataFromISR(uint8_t *data, uint32_t len);
void USB_Serial_NotifyTxCpltFromISR(void);

#ifdef __cplusplus
}
#endif

#endif /* __USB_SERIAL_H */
