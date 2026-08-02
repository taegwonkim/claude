#ifndef __USB_SERIAL_H
#define __USB_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void USB_Serial_Init(void);
uint8_t USB_Serial_IsConnected(void);

int32_t USB_Serial_Transmit(const uint8_t *data, uint16_t len);

uint16_t USB_Serial_Available(void);
uint16_t USB_Serial_Read(uint8_t *buf, uint16_t maxLen);

/* usbd_cdc_if.c의 CDC_Receive_FS에서 호출되는 내부 연동 함수 */
void USB_Serial_PushRxData(uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __USB_SERIAL_H */
