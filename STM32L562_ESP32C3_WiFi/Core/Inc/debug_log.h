/**
  ******************************************************************************
  * @file    debug_log.h
  * @brief   USART2 로 내보내는 간단한 printf 로그 (블로킹 송신)
  ******************************************************************************
  */
#ifndef __DEBUG_LOG_H
#define __DEBUG_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#if (USE_DEBUG_LOG == 1U)

extern UART_HandleTypeDef huart_dbg;

void DBG_Init(void);
void DBG_Printf(const char *fmt, ...);
void DBG_Write(const uint8_t *data, uint16_t len);

#define LOG(fmt, ...)   DBG_Printf("[%8lu] " fmt "\r\n", (unsigned long)HAL_GetTick(), ##__VA_ARGS__)

#else

#define DBG_Init()              ((void)0)
#define DBG_Printf(...)         ((void)0)
#define DBG_Write(d, l)         ((void)0)
#define LOG(...)                ((void)0)

#endif /* USE_DEBUG_LOG */

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_LOG_H */
