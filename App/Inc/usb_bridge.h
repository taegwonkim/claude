/**
  ******************************************************************************
  * @file    usb_bridge.h
  * @brief   USB CDC ↔ 애플리케이션 브리지
  *
  *  usbd_cdc_if.c 에서
  *    - CDC_Receive_FS()  → usbbridge_rx_from_isr(Buf, *Len)
  *  를 호출하도록 연결한다. (Integration/usbd_cdc_if_usercode.c 참조)
  ******************************************************************************
  */
#ifndef USB_BRIDGE_H
#define USB_BRIDGE_H

#include "app_types.h"

/* USB 인터럽트 컨텍스트에서 호출 : 수신 바이트를 qUsbRx 로 밀어넣는다 */
void usbbridge_rx_from_isr(const uint8_t *buf, uint32_t len);

/* qUsbTx 를 소비해 CDC_Transmit_FS 로 내보내는 태스크 */
void usb_tx_task(void *arg);

/* 애플리케이션에서 직접 송신 (큐 경유) */
void usbbridge_write(const char *s, uint16_t len);

#endif /* USB_BRIDGE_H */
