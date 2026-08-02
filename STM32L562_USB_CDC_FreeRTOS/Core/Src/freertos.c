#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "main.h"
#include "usb_serial.h"
#include <stdio.h>
#include <string.h>

static void USB_Serial_EchoTask(void *argument);
static void USB_Serial_HeartbeatTask(void *argument);

static osThreadId_t usbEchoTaskHandle;
static const osThreadAttr_t usbEchoTask_attributes = {
    .name = "usbEchoTask",
    .stack_size = 512U * 4U,
    .priority = (osPriority_t)osPriorityNormal,
};

static osThreadId_t usbHeartbeatTaskHandle;
static const osThreadAttr_t usbHeartbeatTask_attributes = {
    .name = "usbHeartbeatTask",
    .stack_size = 256U * 4U,
    .priority = (osPriority_t)osPriorityLow,
};

void MX_FREERTOS_Init(void)
{
    usbEchoTaskHandle = osThreadNew(USB_Serial_EchoTask, NULL, &usbEchoTask_attributes);
    usbHeartbeatTaskHandle = osThreadNew(USB_Serial_HeartbeatTask, NULL, &usbHeartbeatTask_attributes);
}

/* 수신한 바이트를 그대로 에코 전송 */
static void USB_Serial_EchoTask(void *argument)
{
    uint8_t rxBuf[64];

    (void)argument;

    for (;;) {
        uint16_t n = USB_Serial_Receive(rxBuf, sizeof(rxBuf), UINT32_MAX);
        if (n > 0U) {
            USB_Serial_Transmit(rxBuf, n, 100U);
        }
    }
}

/* 1초마다 하트비트 메시지 송신 (여러 태스크가 동시에 송신해도
   usb_serial.c의 뮤텍스가 직렬화하므로 안전함) */
static void USB_Serial_HeartbeatTask(void *argument)
{
    char msg[32];
    uint32_t seq = 0U;

    (void)argument;

    while (USB_Serial_IsConnected() == 0U) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    for (;;) {
        int len = snprintf(msg, sizeof(msg), "heartbeat #%lu\r\n", (unsigned long)seq++);
        USB_Serial_Transmit((uint8_t *)msg, (uint16_t)len, 100U);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
