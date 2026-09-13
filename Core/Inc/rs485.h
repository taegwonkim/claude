/**
  ******************************************************************************
  * @file    rs485.h
  * @brief   USART3 를 RS485(Driver Enable) 모드로 쓰는 얇은 래퍼
  ******************************************************************************
  */
#ifndef __RS485_H
#define __RS485_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#if (USE_RS485 == 1U)

extern UART_HandleTypeDef huart_rs485;

/* USART3 를 RS485 모드로 초기화한다 (MX_USART3_UART_Init 에서 호출) */
void RS485_Init(void);

/* 블로킹 송신. DE 제어 + 마지막 바이트가 선로에 완전히 나갈 때까지 대기한다. */
void RS485_Write(const uint8_t *data, uint16_t len);
void RS485_Puts(const char *str);
void RS485_Printf(const char *fmt, ...);

/* 수신 링버퍼에서 1바이트 꺼내기. 없으면 false */
bool RS485_GetChar(uint8_t *ch);

/* 마지막 바이트 송신 완료(TC) 대기 ― 리셋 직전에 호출 */
void RS485_WaitTxDone(void);

#else /* USE_RS485 == 0 : 로그 없이 동작 */

#define RS485_Init()              ((void)0)
#define RS485_Write(d, l)         ((void)0)
#define RS485_Puts(s)             ((void)0)
#define RS485_Printf(...)         ((void)0)
#define RS485_GetChar(c)          (false)
#define RS485_WaitTxDone()        ((void)0)

#endif /* USE_RS485 */

#ifdef __cplusplus
}
#endif

#endif /* __RS485_H */
