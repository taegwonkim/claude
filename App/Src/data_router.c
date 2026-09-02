/**
  ******************************************************************************
  * @file    data_router.c
  ******************************************************************************
  */
#include "data_router.h"
#include "uart_link.h"
#include "cfg_store.h"
#include <stdio.h>

uint16_t router_format(const sd_sample_t *s, char *dst, uint16_t dst_sz)
{
  int n = snprintf(dst, dst_sz,
                   "SD,%u,%lu,%u,%u,%u,%u,%u,%u,%u\r\n",
                   (unsigned)s->seq,
                   (unsigned long)s->tick_ms,
                   (unsigned)s->ch[0], (unsigned)s->ch[1], (unsigned)s->ch[2],
                   (unsigned)s->ch[3], (unsigned)s->ch[4], (unsigned)s->ch[5],
                   (unsigned)s->status);

  if (n < 0)                     { return 0u; }
  if ((uint16_t)n >= dst_sz)     { return (uint16_t)(dst_sz - 1u); }
  return (uint16_t)n;
}

void router_send_uart(const char *s, uint16_t len)
{
  if (uartlink_write(&g_lnkPc, (const uint8_t *)s, len, 200u) == SD_OK)
  {
    g_stat.tx_uart++;
  }
}

void router_send_usb(const char *s, uint16_t len)
{
  sd_line_t line;

  if (len >= SD_LINE_MAX) { len = SD_LINE_MAX - 1u; }

  line.len = len;
  memcpy(line.data, s, len);
  line.data[len] = '\0';

  (void)sd_queue_put_overwrite(g_qUsbTx, &line, &g_stat.drop_usb);
}

void router_task(void *arg)
{
  sd_sample_t s;
  sd_line_t   line;
  bool        o_uart, o_usb, o_wifi;

  (void)arg;

  for (;;)
  {
    if (osMessageQueueGet(g_qSample, &s, NULL, osWaitForever) != osOK)
    {
      continue;
    }

    line.len = router_format(&s, line.data, sizeof(line.data));
    if (line.len == 0u) { continue; }

    /* 출력 경로 on/off 는 설정에서 매번 읽는다 (변경 즉시 반영) */
    if (cfgstore_lock(50u))
    {
      o_uart = (g_cfg.out_uart != 0u);
      o_usb  = (g_cfg.out_usb  != 0u);
      o_wifi = (g_cfg.out_wifi != 0u);
      cfgstore_unlock();
    }
    else
    {
      o_uart = true; o_usb = true; o_wifi = true;
    }

    if (o_uart)
    {
      router_send_uart(line.data, line.len);
    }
    if (o_usb)
    {
      (void)sd_queue_put_overwrite(g_qUsbTx, &line, &g_stat.drop_usb);
    }
    if (o_wifi)
    {
      (void)sd_queue_put_overwrite(g_qWifiTx, &line, &g_stat.drop_wifi);
    }
  }
}
