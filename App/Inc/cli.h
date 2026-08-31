/**
  ******************************************************************************
  * @file    cli.h
  * @brief   PC 설정 프로토콜 파서 (USART3 RS485 / USB CDC 공용)
  *          규격: docs/03_Protocol.md 3장
  ******************************************************************************
  */
#ifndef CLI_H
#define CLI_H

#include "app_types.h"

/* 응답 출력 콜백 — 채널별로 다르게 주입한다 */
typedef void (*cli_out_fn)(const char *s, uint16_t len);

typedef struct
{
  cli_out_fn out;
  char       line[SD_CLI_LINE_MAX];
  uint16_t   idx;
} cli_ctx_t;

void cli_init(cli_ctx_t *ctx, cli_out_fn out);

/* 완성된 한 줄을 처리하고 응답을 out 으로 내보낸다 */
void cli_exec_line(cli_ctx_t *ctx, char *line);

/* 바이트 스트림을 먹여 라인이 완성되면 자동으로 cli_exec_line 호출 */
void cli_feed(cli_ctx_t *ctx, uint8_t c);

/* 태스크 */
void cli_uart_task(void *arg);
void cli_usb_task(void *arg);

#endif /* CLI_H */
