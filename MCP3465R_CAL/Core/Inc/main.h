#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

/* ── Exported peripheral handles ─────────────────────────────────────────── */
extern SPI_HandleTypeDef  hspi1;
extern UART_HandleTypeDef huart1;

/* ── GPIO pin labels (generated from .ioc) ───────────────────────────────── */
#define MCP_CS_Pin          GPIO_PIN_4
#define MCP_CS_GPIO_Port    GPIOA

#define MCP_IRQ_Pin         GPIO_PIN_13
#define MCP_IRQ_GPIO_Port   GPIOC

/* ── Error handler ───────────────────────────────────────────────────────── */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
