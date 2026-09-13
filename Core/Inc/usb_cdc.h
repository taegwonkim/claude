/**
  ******************************************************************************
  * @file    usb_cdc.h
  * @brief   USB CDC(가상 COM 포트) 송수신 래퍼
  *
  *  CubeMX 가 생성한 USB Device / CDC 미들웨어 위에 얹는 얇은 층이다.
  *  필요한 CubeMX 설정과 usbd_cdc_if.c 수정 두 줄은 README 를 참고할 것.
  ******************************************************************************
  */
#ifndef __USB_CDC_H
#define __USB_CDC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#if (USE_USB_CDC == 1U)

/* 호스트가 장치를 열거(CONFIGURED)했는지 */
bool USB_CDC_IsReady(void);

/* 열거될 때까지 최대 timeout_ms 만큼 대기. 성공하면 true */
bool USB_CDC_WaitReady(uint32_t timeout_ms);

/* 블로킹 송신. 연결되어 있지 않으면 조용히 버린다(멈추지 않는다). */
void USB_CDC_Write(const uint8_t *data, uint16_t len);

/* 수신 링버퍼에서 1바이트 꺼내기 */
bool USB_CDC_GetChar(uint8_t *ch);

/* 마지막 패킷이 호스트로 넘어갈 시간 확보 */
void USB_CDC_Flush(void);

/* 리셋 직전 정상 분리 (호스트가 즉시 장치 제거를 인식하도록) */
void USB_CDC_Detach(void);

/* usbd_cdc_if.c 의 CDC_Receive_FS() 에서 호출해 줄 것 (README 참고) */
void USB_CDC_RxHandler(const uint8_t *data, uint32_t len);

#else

#define USB_CDC_IsReady()           (false)
#define USB_CDC_WaitReady(t)        (false)
#define USB_CDC_Write(d, l)         ((void)0)
#define USB_CDC_GetChar(c)          (false)
#define USB_CDC_Flush()             ((void)0)
#define USB_CDC_Detach()            ((void)0)
#define USB_CDC_RxHandler(d, l)     ((void)0)

#endif /* USE_USB_CDC */

#ifdef __cplusplus
}
#endif

#endif /* __USB_CDC_H */
