#include "usbd_cdc_if.h"
#include "usb_serial.h"

#define APP_RX_DATA_SIZE 2048U
#define APP_TX_DATA_SIZE 2048U

extern USBD_HandleTypeDef hUsbDeviceFS;

static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS
};

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0U);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
    return (USBD_OK);
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    UNUSED(pbuf);
    UNUSED(length);

    switch (cmd)
    {
        case CDC_SEND_ENCAPSULATED_COMMAND:
        case CDC_GET_ENCAPSULATED_RESPONSE:
        case CDC_SET_COMM_FEATURE:
        case CDC_GET_COMM_FEATURE:
        case CDC_CLEAR_COMM_FEATURE:
        case CDC_SET_LINE_CODING:
        case CDC_GET_LINE_CODING:
        case CDC_SET_CONTROL_LINE_STATE:
        case CDC_SEND_BREAK:
        default:
            break;
    }

    return (USBD_OK);
}

/* USB 인터럽트(USBD_LL_DataOutStage) 컨텍스트에서 호출됨 */
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    USB_Serial_PushRxDataFromISR(Buf, *Len);

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);

    return (USBD_OK);
}

/* 태스크 컨텍스트에서 usb_serial.c의 USB_Serial_Transmit()을 통해 호출됨 */
int8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    uint8_t result = USBD_OK;

    if (hcdc == NULL) {
        return (int8_t)USBD_FAIL;
    }

    if (hcdc->TxState != 0U) {
        return (int8_t)USBD_BUSY;
    }

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);

    return (int8_t)result;
}

/* USB 인터럽트 컨텍스트에서 호출됨 */
static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    UNUSED(Buf);
    UNUSED(Len);
    UNUSED(epnum);

    USB_Serial_NotifyTxCpltFromISR();

    return (USBD_OK);
}
