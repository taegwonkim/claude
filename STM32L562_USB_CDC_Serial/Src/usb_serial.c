#include "usb_serial.h"
#include "usbd_cdc_if.h"
#include "usbd_def.h"
#include "stm32l5xx_hal.h"

#define USB_SERIAL_RX_RING_SIZE 512U
#define USB_SERIAL_TX_TIMEOUT_MS 100U

extern USBD_HandleTypeDef hUsbDeviceFS;

static volatile uint8_t rxRing[USB_SERIAL_RX_RING_SIZE];
static volatile uint16_t rxHead;
static volatile uint16_t rxTail;

void USB_Serial_Init(void)
{
    rxHead = 0U;
    rxTail = 0U;
}

uint8_t USB_Serial_IsConnected(void)
{
    return (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? 1U : 0U;
}

int32_t USB_Serial_Transmit(const uint8_t *data, uint16_t len)
{
    uint8_t result;
    uint32_t waited = 0U;

    do {
        result = CDC_Transmit_FS((uint8_t *)data, len);
        if (result == USBD_BUSY) {
            HAL_Delay(1U);
            waited++;
        }
    } while ((result == USBD_BUSY) && (waited < USB_SERIAL_TX_TIMEOUT_MS));

    return (result == USBD_OK) ? (int32_t)len : -1;
}

/* CDC_Receive_FS (usbd_cdc_if.c)에서 새 USB 패킷이 도착할 때마다 호출 */
void USB_Serial_PushRxData(uint8_t *data, uint32_t len)
{
    uint32_t i;

    for (i = 0U; i < len; i++) {
        uint16_t next = (uint16_t)((rxHead + 1U) % USB_SERIAL_RX_RING_SIZE);
        if (next != rxTail) {
            rxRing[rxHead] = data[i];
            rxHead = next;
        }
        /* 버퍼가 가득 차면 오래된 데이터를 덮어쓰지 않고 새 바이트를 버림 */
    }
}

uint16_t USB_Serial_Available(void)
{
    return (uint16_t)((rxHead + USB_SERIAL_RX_RING_SIZE - rxTail) % USB_SERIAL_RX_RING_SIZE);
}

uint16_t USB_Serial_Read(uint8_t *buf, uint16_t maxLen)
{
    uint16_t count = 0U;

    while ((rxTail != rxHead) && (count < maxLen)) {
        buf[count++] = rxRing[rxTail];
        rxTail = (uint16_t)((rxTail + 1U) % USB_SERIAL_RX_RING_SIZE);
    }

    return count;
}
