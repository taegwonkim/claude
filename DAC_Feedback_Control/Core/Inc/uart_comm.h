/**
 ******************************************************************************
 * @file    uart_comm.h
 * @brief   UART PC 통신 모듈
 *
 * 명령 프로토콜 (ASCII, \r\n 종결):
 *   SET <ch> <mV>       채널 ch(1~4) 전압 설정
 *   EN  <ch>            채널 활성화
 *   DIS <ch>            채널 비활성화
 *   RST <ch>            채널 결함 리셋 (0=전체)
 *   GET                 모든 채널 상태 조회
 *
 * 응답 포맷 (주기 전송 & 요청 응답):
 *   >CH1 TARGET=1500mV MEAS=1498mV CURR=25mA FAULT=NONE\r\n
 *   ...
 *
 * DMA 기반 수신: HAL_UART_Receive_DMA로 환형 버퍼 운용
 * 송신: DMA 또는 polling (txBusy 플래그로 직렬화)
 ******************************************************************************
 */

#ifndef __UART_COMM_H
#define __UART_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* -------------------------------------------------------------------------
 * 버퍼 크기 정의
 * ------------------------------------------------------------------------- */
#define UART_RX_BUF_SIZE        256U
#define UART_TX_BUF_SIZE        512U
#define UART_MAX_CMD_LEN        64U

/* -------------------------------------------------------------------------
 * 텍스트 패킷 구조체 (xQueueTxData로 전달)
 * ------------------------------------------------------------------------- */
#define TX_PKT_MAX_LEN          128U
typedef struct {
    uint8_t  data[TX_PKT_MAX_LEN];
    uint16_t len;
    uint8_t  via_uart;      /* 1: UART로 전송 */
    uint8_t  via_fdcan;     /* 1: FDCAN으로 전송 */
} TxPacket_t;

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * @brief  UART 수신 DMA 시작 및 수신 버퍼 초기화
 */
void UART_Init(void);

/**
 * @brief  UART 수신 태스크 메인 루프 (vTaskUARTReceive에서 호출)
 *         수신 버퍼를 파싱하고 UartCmd_t를 xQueueUARTCmd에 넣음
 */
void UART_RxProcess(void);

/**
 * @brief  문자열 즉시 송신 (blocking, 최대 100ms 대기)
 * @param  str  NULL 종결 문자열
 */
void UART_SendString(const char *str);

/**
 * @brief  포맷 출력 (printf 스타일, 내부 TX 버퍼 사용)
 */
void UART_Printf(const char *fmt, ...);

/**
 * @brief  모든 채널 상태를 UART로 출력
 */
void UART_SendChannelStatus(void);

/**
 * @brief  HAL_UART_RxCpltCallback / HAL_UART_ErrorCallback 내부 처리
 *         uart_comm.c 내에 구현, 인터럽트 컨텍스트에서 호출됨
 */
void UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* __UART_COMM_H */
