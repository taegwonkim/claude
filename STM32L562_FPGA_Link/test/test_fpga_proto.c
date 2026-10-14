/* test_fpga_proto.c - PC 에서 실행하는 프로토콜 단위 테스트
 *   gcc -Wall -ICore/Inc test/test_fpga_proto.c Core/Src/fpga_proto.c -o t && ./t
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fpga_proto.h"

static FPGA_RxParser  p;
static FPGA_PktQueue  q;
static FPGA_LinkStats st;

/* DMA 이벤트처럼 임의 크기 조각으로 나눠 넣기 */
static void feed_chunks(const uint8_t *d, uint32_t n, uint32_t chunk)
{
  uint32_t i;
  for (i = 0; i < n; i += chunk)
    FPGA_Proto_Feed(&p, &q, d + i, (n - i < chunk) ? n - i : chunk, &st);
}

int main(void)
{
  FPGA_Packet pkt;
  long v[16];
  uint8_t stream[600];
  uint32_t n = 0;
  const char *adc = "\x02\"80909,12,0,0,1234,4000000000,5,6,7,8,9,10\"\r\n";
  int k;

  /* 1) 원본 FPGA 펌웨어 : ADC 프레임 뒤에 0x00 패딩 (100바이트 고정 전송) */
  memset(stream, 0, 100);
  memcpy(stream, adc, strlen(adc));
  n = 100;
  /* 2) SR_ALL 응답 (\r\n), SR_OFFSV 응답 (ETX), 프레임 앞 쓰레기 */
  memcpy(stream + n, "zz\x02" "5000,100,1000\r\n", 18); n += 18;
  memcpy(stream + n, "\x02" "100\x03", 5); n += 5;
  /* 3) 중간에 끊긴 프레임 뒤 새 STX → 새 프레임으로 재시작 */
  memcpy(stream + n, "\x02" "12,3\x02" "OK\r\n", 10); n += 10;

  for (k = 1; k <= 7; k += 3)
  {
    memset(&p, 0, sizeof(p)); memset(&q, 0, sizeof(q)); memset(&st, 0, sizeof(st));
    feed_chunks(stream, n, (uint32_t)k);

    assert(FPGA_Proto_QueuePop(&q, &pkt));
    assert(FPGA_Proto_ParseNumbers(&pkt, v, 16) == 12);
    assert(v[0] == 80909 && v[1] == 12 && v[4] == 1234 && v[5] == 4000000000L && v[11] == 10);
    assert(pkt.data[0] == '"');

    assert(FPGA_Proto_QueuePop(&q, &pkt));
    assert(FPGA_Proto_ParseNumbers(&pkt, v, 16) == 3 && v[0] == 5000 && v[2] == 1000);

    assert(FPGA_Proto_QueuePop(&q, &pkt));
    assert(!strcmp(pkt.data, "100"));

    assert(FPGA_Proto_QueuePop(&q, &pkt));
    assert(!strcmp(pkt.data, "OK"));
    assert(!FPGA_Proto_QueuePop(&q, &pkt));
    assert(st.rx_packets == 4 && st.rx_garbage == (100 - strlen(adc)) + 2);
  }

  /* 4) 너무 긴 프레임은 버림 */
  memset(&p, 0, sizeof(p)); memset(&q, 0, sizeof(q)); memset(&st, 0, sizeof(st));
  stream[0] = 0x02; memset(stream + 1, 'a', 300); stream[301] = '\n';
  feed_chunks(stream, 302, 64);
  assert(!FPGA_Proto_QueuePop(&q, &pkt) && st.rx_too_long == 1);

  /* 5) 큐 가득 참 : QUEUE_LEN-1 개 저장, 나머지 카운트 */
  for (k = 0; k < 10; k++) FPGA_Proto_Feed(&p, &q, (const uint8_t *)"\x02" "1\r\n", 4, &st);
  assert(st.rx_packets == FPGA_PKT_QUEUE_LEN - 1 && st.rx_queue_full == 10 - (FPGA_PKT_QUEUE_LEN - 1));

  /* 6) 명령 프레임 */
  {
    uint8_t out[16];
    assert(FPGA_Proto_BuildCommand(out, sizeof(out), "START") == 8);
    assert(!memcmp(out, "\x02START\r\n", 8));
    assert(FPGA_Proto_BuildCommand(out, 7, "START") == -1);
  }

  puts("FPGA PROTO TEST PASS");
  return 0;
}
