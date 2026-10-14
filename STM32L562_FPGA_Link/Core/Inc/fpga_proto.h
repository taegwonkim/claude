/**
  ******************************************************************************
  * @file    fpga_proto.h
  * @brief   FPGA(Nios II) <-> STM32 UART 프레임 프로토콜 (HAL 비의존, PC 에서 테스트 가능)
  *
  *  프레임 형식 (FPGA 펌웨어 pc_cmd.c / adc_ltc2292.c 기준)
  *    MCU  -> FPGA : STX(0x02) + "CMD[,p1,p2..]" + "\r\n"
  *    FPGA -> MCU  : STX(0x02) + payload + ("\r\n" 또는 ETX(0x03))
  *                   예) STX "\"80909,12,0,0,...\"" "\r\n"   (ADC 피크/카운트 12개)
  *                       STX "5000,100,1000" "\r\n"          (SR_ALL 응답)
  *                       STX "100" ETX                       (SR_OFFSV / SR_RDTIME 응답)
  ******************************************************************************
  */
#ifndef FPGA_PROTO_H
#define FPGA_PROTO_H

#include <stdint.h>

#define FPGA_STX                0x02U
#define FPGA_ETX                0x03U

#define FPGA_PKT_MAX_LEN        128U    /* payload 최대 길이 (FPGA RXBUFSIZE=100 보다 크게) */
#define FPGA_PKT_QUEUE_LEN      8U      /* 수신 완료 패킷 큐 (2의 거듭제곱)                 */
#define FPGA_CMD_MAX_LEN        64U     /* 송신 명령 1개 최대 길이 (STX, CRLF 포함)          */
#define FPGA_CMD_QUEUE_LEN      8U      /* 송신 명령 큐 (2의 거듭제곱)                      */

typedef struct
{
  uint16_t len;                         /* payload 길이 (STX / CR / LF / ETX 제외) */
  char     data[FPGA_PKT_MAX_LEN + 1];  /* '\0' 종료                               */
} FPGA_Packet;

typedef struct
{
  volatile uint32_t rx_bytes;
  volatile uint32_t rx_packets;
  volatile uint32_t rx_queue_full;      /* 메인루프가 꺼내지 않아 버린 패킷   */
  volatile uint32_t rx_too_long;        /* 종료문자 없이 너무 긴 프레임       */
  volatile uint32_t rx_garbage;         /* STX 밖에서 들어온 바이트           */
  volatile uint32_t uart_errors;        /* ORE / FE / NE                       */
  volatile uint32_t rx_restarts;        /* DMA 수신 재시작 횟수                */
  volatile uint32_t tx_frames;
  volatile uint32_t tx_queue_full;
} FPGA_LinkStats;

/* ---- 수신 프레임 파서 (ISR 에서 호출) ---- */
typedef struct
{
  uint8_t  active;
  uint16_t len;
  char     buf[FPGA_PKT_MAX_LEN + 1];
} FPGA_RxParser;

/* ---- 단일 생산자 / 단일 소비자 큐 (ISR -> main) ---- */
typedef struct
{
  FPGA_Packet       pkt[FPGA_PKT_QUEUE_LEN];
  volatile uint32_t head;               /* ISR 이 증가 */
  volatile uint32_t tail;               /* main 이 증가 */
} FPGA_PktQueue;

void FPGA_Proto_ParserReset(FPGA_RxParser *p);
void FPGA_Proto_Feed(FPGA_RxParser *p, FPGA_PktQueue *q, const uint8_t *data, uint32_t len,
                     FPGA_LinkStats *st);

int  FPGA_Proto_QueuePop(FPGA_PktQueue *q, FPGA_Packet *out);    /* 1 = 꺼냄, 0 = 비어있음 */

/* STX + cmd + "\r\n" 프레임 생성. 반환 = 프레임 길이, 실패 시 -1 */
int  FPGA_Proto_BuildCommand(uint8_t *out, uint32_t out_size, const char *cmd);

/* payload 안의 정수들을 순서대로 추출 ("\"80909,12,0\"" -> {80909,12,0}). 반환 = 개수 */
int  FPGA_Proto_ParseNumbers(const FPGA_Packet *pkt, long *out, int max);

#endif /* FPGA_PROTO_H */
