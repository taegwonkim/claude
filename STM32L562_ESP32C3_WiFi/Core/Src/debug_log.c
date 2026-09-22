/**
  ******************************************************************************
  * @file    debug_log.c
  * @brief   USART2 디버그 로그 (115200-8-N-1, 블로킹 송신)
  ******************************************************************************
  */
#include "debug_log.h"

#if (USE_DEBUG_LOG == 1U)

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

UART_HandleTypeDef huart_dbg;

/**
  * @brief  USART2 초기화. GPIO/클럭은 HAL_UART_MspInit() 에서 처리.
  */
void DBG_Init(void)
{
  huart_dbg.Instance                    = DBG_UART_INSTANCE;
  huart_dbg.Init.BaudRate               = DBG_UART_BAUDRATE;
  huart_dbg.Init.WordLength             = UART_WORDLENGTH_8B;
  huart_dbg.Init.StopBits               = UART_STOPBITS_1;
  huart_dbg.Init.Parity                 = UART_PARITY_NONE;
  huart_dbg.Init.Mode                   = UART_MODE_TX_RX;
  huart_dbg.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
  huart_dbg.Init.OverSampling           = UART_OVERSAMPLING_16;
  huart_dbg.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
  huart_dbg.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
  huart_dbg.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  if (HAL_UART_Init(&huart_dbg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart_dbg, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart_dbg, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart_dbg) != HAL_OK)
  {
    Error_Handler();
  }
}

void DBG_Write(const uint8_t *data, uint16_t len)
{
  if ((data == NULL) || (len == 0U))
  {
    return;
  }
  (void)HAL_UART_Transmit(&huart_dbg, (uint8_t *)data, len, 1000U);
}

void DBG_Printf(const char *fmt, ...)
{
  static char buf[256];
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (n <= 0)
  {
    return;
  }
  if (n > (int)(sizeof(buf) - 1U))
  {
    n = (int)(sizeof(buf) - 1U);
  }
  DBG_Write((const uint8_t *)buf, (uint16_t)n);
}

#endif /* USE_DEBUG_LOG */
