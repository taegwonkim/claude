/**
  ******************************************************************************
  * @file    fpga_proto.c
  * @brief   FPGA <-> STM32 UART 프레임 파서 / 명령 생성 (HAL 비의존)
  ******************************************************************************
  */
#include "fpga_proto.h"
#include <string.h>

void FPGA_Proto_ParserReset(FPGA_RxParser *p)
{
  p->active = 0U;
  p->len    = 0U;
}

static void push_packet(FPGA_RxParser *p, FPGA_PktQueue *q, FPGA_LinkStats *st)
{
  uint32_t head = q->head;
  uint32_t next = (head + 1U) & (FPGA_PKT_QUEUE_LEN - 1U);

  if (next == q->tail)
  {
    st->rx_queue_full++;                /* 큐가 가득 참 : 새 패킷 버림 */
    return;
  }
  memcpy(q->pkt[head].data, p->buf, p->len);
  q->pkt[head].data[p->len] = '\0';
  q->pkt[head].len = p->len;
  q->head = next;                       /* 데이터 복사 후 인덱스 갱신 */
  st->rx_packets++;
}

void FPGA_Proto_Feed(FPGA_RxParser *p, FPGA_PktQueue *q, const uint8_t *data, uint32_t len,
                     FPGA_LinkStats *st)
{
  uint32_t i;

  st->rx_bytes += len;

  for (i = 0U; i < len; i++)
  {
    uint8_t c = data[i];

    if (c == FPGA_STX)                  /* STX 는 언제나 새 프레임 시작 (FPGA 쪽과 동일) */
    {
      p->active = 1U;
      p->len    = 0U;
      continue;
    }

    if (!p->active)
    {
      st->rx_garbage++;                 /* 프레임 밖의 바이트 (원본 FPGA 의 0x00 패딩 등) */
      continue;
    }

    if ((c == '\n') || (c == FPGA_ETX))
    {
      push_packet(p, q, st);
      FPGA_Proto_ParserReset(p);
      continue;
    }

    if (c == '\r')
    {
      continue;
    }

    if (p->len >= FPGA_PKT_MAX_LEN)
    {
      st->rx_too_long++;
      FPGA_Proto_ParserReset(p);
      continue;
    }
    p->buf[p->len++] = (char)c;
  }
}

int FPGA_Proto_QueuePop(FPGA_PktQueue *q, FPGA_Packet *out)
{
  uint32_t tail = q->tail;

  if (tail == q->head)
  {
    return 0;
  }
  *out = q->pkt[tail];
  q->tail = (tail + 1U) & (FPGA_PKT_QUEUE_LEN - 1U);
  return 1;
}

int FPGA_Proto_BuildCommand(uint8_t *out, uint32_t out_size, const char *cmd)
{
  uint32_t n = (uint32_t)strlen(cmd);

  if ((n == 0U) || (n + 3U > out_size))
  {
    return -1;
  }
  out[0] = FPGA_STX;
  memcpy(&out[1], cmd, n);
  out[n + 1U] = '\r';
  out[n + 2U] = '\n';
  return (int)(n + 3U);
}

int FPGA_Proto_ParseNumbers(const FPGA_Packet *pkt, long *out, int max)
{
  const char *s = pkt->data;
  int count = 0;

  while ((*s != '\0') && (count < max))
  {
    int  neg = 0;
    long v   = 0;

    if ((*s == '-') && (s[1] >= '0') && (s[1] <= '9'))
    {
      neg = 1;
      s++;
    }
    if ((*s >= '0') && (*s <= '9'))
    {
      while ((*s >= '0') && (*s <= '9'))
      {
        v = (v * 10) + (long)(*s - '0');
        s++;
      }
      out[count++] = neg ? -v : v;
    }
    else
    {
      s++;
    }
  }
  return count;
}
