#include <string.h>
#include "app_usb_cdc.h"
#include "app_cfg_protocol.h"
#include "ring_buffer.h"
#include "cmsis_os2.h"
#include "usbd_cdc_if.h"

static uint8_t      s_rxStorage[USB_CDC_RX_RINGBUF_SIZE];
static RingBuffer_t  s_rxRing;

static char          s_line[USB_CDC_LINE_MAX_LEN];
static uint16_t      s_lineLen;

static void UsbCdc_Reply(const char *msg)
{
    uint16_t len = (uint16_t)strlen(msg);
    uint32_t start = HAL_GetTick();

    /* CDC_Transmit_FS() returns USBD_BUSY while a previous packet is
     * still in flight (host hasn't ACKed it yet) - retry briefly rather
     * than silently dropping the reply. If nothing is reading the port
     * (cable unplugged, no terminal open) give up instead of blocking
     * the task forever. */
    while (CDC_Transmit_FS((uint8_t *)msg, len) == USBD_BUSY) {
        if ((HAL_GetTick() - start) >= USB_CDC_TX_RETRY_TIMEOUT_MS) {
            return;
        }
        osDelay(2);
    }
}

void App_UsbCdc_Init(void)
{
    RingBuffer_Init(&s_rxRing, s_rxStorage, USB_CDC_RX_RINGBUF_SIZE);
    s_lineLen = 0;
}

void App_UsbCdc_RxFromISR(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        RingBuffer_PutByte(&s_rxRing, buf[i]);
    }
}

void App_UsbCdc_Task(void *argument)
{
    (void)argument;
    uint8_t byte;

    for (;;) {
        if (RingBuffer_GetByte(&s_rxRing, &byte)) {
            if (byte == '\n') {
                s_line[s_lineLen] = '\0';
                CfgProtocol_HandleLine(s_line, UsbCdc_Reply);
                s_lineLen = 0;
            } else if (byte != '\r') {
                if (s_lineLen < (USB_CDC_LINE_MAX_LEN - 1U)) {
                    s_line[s_lineLen++] = (char)byte;
                } else {
                    /* line too long: drop it and resync on next '\n' */
                    s_lineLen = 0;
                }
            }
        } else {
            osDelay(5);
        }
    }
}
