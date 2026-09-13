/**
  ******************************************************************************
  * @file    comm.h
  * @brief   보고 채널 분배 계층
  *
  *  애플리케이션은 COMM_* 만 호출하고, 이 계층이 아래 두 채널로 같은 내용을
  *  동시에 내보낸다.
  *
  *    - USART3 RS485  (USE_RS485   = 1)
  *    - USB CDC 가상 COM 포트 (USE_USB_CDC = 1)
  *
  *  둘 다 켜면 같은 문자열이 양쪽으로 나가고, 명령 수신도 양쪽에서 받는다.
  *  한쪽만 켜도 되고 둘 다 꺼도 (로그 없이) 동작한다.
  ******************************************************************************
  */
#ifndef __COMM_H
#define __COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* 모든 MX_xxx_Init() 이후에 호출.
   USB 를 쓰면 호스트가 장치를 열거할 때까지 기다린다(타임아웃 있음). */
void COMM_Init(void);

void COMM_Write(const uint8_t *data, uint16_t len);
void COMM_Puts(const char *str);
void COMM_Printf(const char *fmt, ...);

/* 두 채널 중 먼저 들어온 1바이트를 꺼낸다. 없으면 false */
bool COMM_GetChar(uint8_t *ch);

/* 소프트웨어 리셋 직전 호출 : 송신 완료 대기 + USB 정상 분리 */
void COMM_PrepareReset(void);

#ifdef __cplusplus
}
#endif

#endif /* __COMM_H */
