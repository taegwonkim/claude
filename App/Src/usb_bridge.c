/**
  ******************************************************************************
  * @file    usb_bridge.c
  ******************************************************************************
  */
#include "usb_bridge.h"
#include "usbd_cdc_if.h"

void usbbridge_rx_from_isr(const uint8_t *buf, uint32_t len)
{
  if ((buf == NULL) || (g_qUsbRx == NULL)) { return; }

  for (uint32_t i = 0u; i < len; i++)
  {
    /* ISR 컨텍스트 : timeout 0 필수. 큐가 차면 버린다. */
    (void)osMessageQueuePut(g_qUsbRx, &buf[i], 0u, 0u);
  }
}

void usbbridge_write(const char *s, uint16_t len)
{
  sd_line_t line;

  if (len >= SD_LINE_MAX) { len = SD_LINE_MAX - 1u; }

  line.len = len;
  memcpy(line.data, s, len);
  line.data[len] = '\0';

  (void)sd_queue_put_overwrite(g_qUsbTx, &line, &g_stat.drop_usb);
}

void usb_tx_task(void *arg)
{
  sd_line_t line;

  (void)arg;

  for (;;)
  {
    if (osMessageQueueGet(g_qUsbTx, &line, NULL, osWaitForever) != osOK)
    {
      continue;
    }

    /* CDC 가 busy 면 잠시 후 재시도 (최대 100ms) */
    for (uint8_t retry = 0u; retry < 20u; retry++)
    {
      uint8_t st = CDC_Transmit_FS((uint8_t *)line.data, line.len);

      if (st == USBD_OK)
      {
        g_stat.tx_usb++;
        break;
      }
      if (st == USBD_FAIL)          /* 미연결 상태 : 버린다 */
      {
        break;
      }
      osDelay(5u);                  /* USBD_BUSY */
    }
  }
}
