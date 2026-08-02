#include "usb_serial.h"
#include "usbd_cdc_if.h"
#include "usbd_def.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"
#include <stdint.h>

#define USB_RX_STREAM_BUFFER_SIZE 512U
#define USB_RX_TRIGGER_LEVEL 1U

extern USBD_HandleTypeDef hUsbDeviceFS;

static StreamBufferHandle_t rxStreamBuffer;
static SemaphoreHandle_t txMutex;
static SemaphoreHandle_t txDoneSem;

static TickType_t ToTicks(uint32_t timeoutMs)
{
    return (timeoutMs == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
}

void USB_Serial_RTOS_Init(void)
{
    rxStreamBuffer = xStreamBufferCreate(USB_RX_STREAM_BUFFER_SIZE, USB_RX_TRIGGER_LEVEL);
    txMutex = xSemaphoreCreateMutex();
    txDoneSem = xSemaphoreCreateBinary();
}

uint8_t USB_Serial_IsConnected(void)
{
    return (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? 1U : 0U;
}

int32_t USB_Serial_Transmit(const uint8_t *data, uint16_t len, uint32_t timeoutMs)
{
    TickType_t ticks = ToTicks(timeoutMs);
    int32_t ret = -1;

    if (xSemaphoreTake(txMutex, ticks) != pdTRUE) {
        return -1;
    }

    if (CDC_Transmit_FS((uint8_t *)data, len) == USBD_OK) {
        if (xSemaphoreTake(txDoneSem, ticks) == pdTRUE) {
            ret = (int32_t)len;
        }
    }

    xSemaphoreGive(txMutex);
    return ret;
}

uint16_t USB_Serial_Receive(uint8_t *buf, uint16_t maxLen, uint32_t timeoutMs)
{
    return (uint16_t)xStreamBufferReceive(rxStreamBuffer, buf, maxLen, ToTicks(timeoutMs));
}

/* CDC_Receive_FS (usbd_cdc_if.c)에서 호출. USB 인터럽트 컨텍스트 */
void USB_Serial_PushRxDataFromISR(uint8_t *data, uint32_t len)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xStreamBufferSendFromISR(rxStreamBuffer, data, len, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* CDC_TransmitCplt_FS (usbd_cdc_if.c)에서 호출. USB 인터럽트 컨텍스트 */
void USB_Serial_NotifyTxCpltFromISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR(txDoneSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
