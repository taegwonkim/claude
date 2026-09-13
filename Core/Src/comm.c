/**
  ******************************************************************************
  * @file    comm.c
  * @brief   RS485 / USB CDC 두 채널로 같은 내용을 내보내는 분배 계층
  ******************************************************************************
  */
#include "comm.h"
#include "rs485.h"
#include "usb_cdc.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 포맷 버퍼는 여기 한 곳에만 둔다 (채널마다 두면 RAM 낭비) */
static char s_fmt[256];

void COMM_Init(void)
{
#if (USE_USB_CDC == 1U)
  /* USB 는 케이블이 꽂혀 있어도 호스트가 장치를 열거(enumerate)하는 데
     수백 ms 가 걸린다. 그 전에 배너를 보내면 통째로 사라지므로 기다린다.
     케이블이 없으면 타임아웃만큼만 지연되고 그냥 진행한다. */
  (void)USB_CDC_WaitReady(USB_CDC_READY_TIMEOUT_MS);
#endif
}

void COMM_Write(const uint8_t *data, uint16_t len)
{
  if ((data == NULL) || (len == 0U))
  {
    return;
  }

#if (USE_RS485 == 1U)
  RS485_Write(data, len);
#endif
#if (USE_USB_CDC == 1U)
  USB_CDC_Write(data, len);
#endif
}

void COMM_Puts(const char *str)
{
  if (str != NULL)
  {
    COMM_Write((const uint8_t *)str, (uint16_t)strlen(str));
  }
}

void COMM_Printf(const char *fmt, ...)
{
  va_list args;
  int     len;

  va_start(args, fmt);
  len = vsnprintf(s_fmt, sizeof(s_fmt), fmt, args);
  va_end(args);

  if (len <= 0)
  {
    return;
  }
  if ((size_t)len >= sizeof(s_fmt))
  {
    len = (int)sizeof(s_fmt) - 1;      /* 잘렸을 때 */
  }
  COMM_Write((const uint8_t *)s_fmt, (uint16_t)len);
}

bool COMM_GetChar(uint8_t *ch)
{
#if (USE_RS485 == 1U) && (USE_COMM_CMD == 1U)
  if (RS485_GetChar(ch))
  {
    return true;
  }
#endif
#if (USE_USB_CDC == 1U) && (USE_COMM_CMD == 1U)
  if (USB_CDC_GetChar(ch))
  {
    return true;
  }
#endif
  (void)ch;
  return false;
}

void COMM_PrepareReset(void)
{
#if (USE_RS485 == 1U)
  /* 마지막 바이트가 선로에 완전히 실릴 때까지 */
  RS485_WaitTxDone();
#endif
#if (USE_USB_CDC == 1U)
  /* 마지막 패킷이 호스트로 넘어갈 시간을 준 뒤 정상적으로 분리한다.
     그냥 리셋하면 호스트가 "장치가 사라짐"을 늦게 알아채 COM 포트가
     한동안 좀비로 남는다. */
  USB_CDC_Flush();
  USB_CDC_Detach();
#endif
}
