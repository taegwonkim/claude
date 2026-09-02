/**
  ******************************************************************************
  * @file    uart_link.h
  * @brief   DMA 순환 수신 + 링버퍼 + DMA 송신을 갖는 UART 추상화
  *
  *  - RX : HAL_UARTEx_ReceiveToIdle_DMA() + Circular DMA
  *         → Half / Complete / Idle 이벤트마다 RxEventCallback 이 호출되고
  *           카운팅 세마포어를 release 한다. 소비자는 read/readline 으로 꺼낸다.
  *  - TX : HAL_UART_Transmit_DMA() + 완료 세마포어 + 뮤텍스
  ******************************************************************************
  */
#ifndef UART_LINK_H
#define UART_LINK_H

#include "app_types.h"
#include "main.h"

typedef struct
{
  UART_HandleTypeDef *huart;
  uint8_t            *rx_buf;      /* DMA 순환 버퍼 (정적/전역이어야 함) */
  uint16_t            rx_size;
  volatile uint16_t   rd;          /* 소비자 읽기 인덱스 */
  osSemaphoreId_t     rx_sig;      /* counting, RX 이벤트 알림 */
  osSemaphoreId_t     tx_done;     /* binary, TX 완료 */
  osMutexId_t         tx_mtx;      /* TX 경로 배타 */
  volatile uint32_t   ovf_cnt;     /* 링버퍼 오버런 추정 카운트 */
} uart_link_t;

/* 전역 링크 (uart_link.c 정의) */
extern uart_link_t g_lnkEsp;    /* USART1 : ESP32-C WROOM */
extern uart_link_t g_lnkFpga;   /* USART2 : Cyclone IV    */
extern uart_link_t g_lnkPc;     /* USART3 : RS485 PC      */

/* 모든 링크 초기화 + DMA 수신 시작. osKernelStart() 이전(App_Init)에 호출 */
sd_res_t uartlink_init_all(void);

/* 수신 -------------------------------------------------------------------- */
uint16_t uartlink_avail(uart_link_t *lnk);
void     uartlink_flush_rx(uart_link_t *lnk);

/* 최대 timeout_ms 동안 1바이트 수신. 성공 시 SD_OK */
sd_res_t uartlink_getc(uart_link_t *lnk, uint8_t *b, uint32_t timeout_ms);

/* len 바이트를 모두 채울 때까지 수신 (timeout 은 전체 예산) */
sd_res_t uartlink_read(uart_link_t *lnk, uint8_t *dst, uint16_t len, uint32_t timeout_ms);

/* '\n' 까지 수신. CR/LF 는 제거하고 NUL 종단. 반환: 문자열 길이(>=0) 또는 음수 에러
 * 빈 줄이면 0 을 반환한다. */
int32_t  uartlink_readline(uart_link_t *lnk, char *dst, uint16_t dst_sz, uint32_t timeout_ms);

/* 송신 -------------------------------------------------------------------- */
sd_res_t uartlink_write(uart_link_t *lnk, const uint8_t *src, uint16_t len, uint32_t timeout_ms);
sd_res_t uartlink_puts(uart_link_t *lnk, const char *s, uint32_t timeout_ms);

/* HAL 콜백에서 호출 (hooks.c) */
void uartlink_on_rx_event(UART_HandleTypeDef *huart, uint16_t size);
void uartlink_on_tx_done(UART_HandleTypeDef *huart);
void uartlink_on_error(UART_HandleTypeDef *huart);

#endif /* UART_LINK_H */
