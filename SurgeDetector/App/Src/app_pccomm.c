/**
 * app_pccomm.c — PC 설정 read/write 처리 (USART3 RS485 + USB CDC) 구현
 *
 * 태스크 구성:
 *  - pcUartTask     : USART3(RS485) 커맨드 수신/응답 (IDLE 라인 검출 DMA)
 *  - pcUsbTask      : USB CDC 커맨드 수신/응답 (usbd_cdc_if.c 훅이 큐로 전달)
 *  - pcDataPushTask : fpgaTask가 넣어준 서지 데이터(qFpgaToPc)를 UART/USB 양쪽에 통지
 */
#include "app_pccomm.h"
#include "app_config.h"
#include "main.h"
#include <string.h>

extern UART_HandleTypeDef huart3;
/* USB_DEVICE 미들웨어(usbd_cdc_if.c)가 제공하는 송신 함수 */
extern uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

#define PCCOMM_RX_BUF_LEN   PC_FRAME_MAX_SIZE

osMessageQueueId_t qFpgaToPc;
osSemaphoreId_t    semUart3Event;

static osThreadId_t s_pcUartTaskHandle;
static osThreadId_t s_pcUsbTaskHandle;
static osThreadId_t s_pcDataPushTaskHandle;

static uint8_t          s_uart3RxBuf[PCCOMM_RX_BUF_LEN];
static volatile uint16_t s_uart3RxLen;

typedef struct
{
    uint8_t  data[PCCOMM_RX_BUF_LEN];
    uint32_t len;
} UsbRxMsg_t;

static osMessageQueueId_t s_qUsbRx;

/* -------------------------------------------------------------------------- */
/* 공용 커맨드 처리기: 요청 프레임 -> 응답 프레임 (WIFI/SERVER/NET 설정 + 상태조회) */
static void ProcessCommand(const PcFrame_t *req, PcFrame_t *resp)
{
    SystemConfig_t cfg;
    App_Config_GetCopy(&cfg);

    switch (req->cmd)
    {
    case CMD_WRITE_WIFI_CFG:
        if (req->len == (CFG_SSID_MAXLEN + CFG_PASSWORD_MAXLEN))
        {
            memcpy(cfg.ssid, &req->payload[0], CFG_SSID_MAXLEN);
            memcpy(cfg.password, &req->payload[CFG_SSID_MAXLEN], CFG_PASSWORD_MAXLEN);
            cfg.ssid[CFG_SSID_MAXLEN - 1] = 0;
            cfg.password[CFG_PASSWORD_MAXLEN - 1] = 0;
            resp->cmd = App_Config_Save(&cfg) ? CMD_ACK : CMD_NACK;
        }
        else
        {
            resp->cmd = CMD_NACK;
            resp->payload[0] = ERR_BAD_LEN;
            resp->len = 1;
            return;
        }
        resp->len = 0;
        break;

    case CMD_READ_WIFI_CFG:
        resp->cmd = CMD_READ_WIFI_CFG;
        memcpy(&resp->payload[0], cfg.ssid, CFG_SSID_MAXLEN);
        memcpy(&resp->payload[CFG_SSID_MAXLEN], cfg.password, CFG_PASSWORD_MAXLEN);
        resp->len = CFG_SSID_MAXLEN + CFG_PASSWORD_MAXLEN;
        break;

    case CMD_WRITE_SERVER_CFG:
        if (req->len == 6u)
        {
            memcpy(cfg.serverIp, &req->payload[0], 4);
            cfg.serverPort = ((uint16_t)req->payload[4] << 8) | req->payload[5];
            resp->cmd = App_Config_Save(&cfg) ? CMD_ACK : CMD_NACK;
        }
        else
        {
            resp->cmd = CMD_NACK;
            resp->payload[0] = ERR_BAD_LEN;
            resp->len = 1;
            return;
        }
        resp->len = 0;
        break;

    case CMD_READ_SERVER_CFG:
        resp->cmd = CMD_READ_SERVER_CFG;
        memcpy(&resp->payload[0], cfg.serverIp, 4);
        resp->payload[4] = (uint8_t)(cfg.serverPort >> 8);
        resp->payload[5] = (uint8_t)(cfg.serverPort & 0xFFu);
        resp->len = 6;
        break;

    case CMD_WRITE_NET_CFG:
        if (req->len == 13u)
        {
            cfg.dhcpEnable = req->payload[0];
            memcpy(cfg.moduleIp, &req->payload[1], 4);
            memcpy(cfg.gateway, &req->payload[5], 4);
            memcpy(cfg.netmask, &req->payload[9], 4);
            resp->cmd = App_Config_Save(&cfg) ? CMD_ACK : CMD_NACK;
        }
        else
        {
            resp->cmd = CMD_NACK;
            resp->payload[0] = ERR_BAD_LEN;
            resp->len = 1;
            return;
        }
        resp->len = 0;
        break;

    case CMD_READ_NET_CFG:
        resp->cmd = CMD_READ_NET_CFG;
        resp->payload[0] = cfg.dhcpEnable;
        memcpy(&resp->payload[1], cfg.moduleIp, 4);
        memcpy(&resp->payload[5], cfg.gateway, 4);
        memcpy(&resp->payload[9], cfg.netmask, 4);
        resp->len = 13;
        break;

    case CMD_READ_STATUS:
    {
        uint32_t flags = osEventFlagsGet(egSysStatus);
        resp->cmd = CMD_READ_STATUS;
        resp->payload[0] = (uint8_t)(flags & 0xFFu);
        resp->len = 1;
        break;
    }

    default:
        resp->cmd = CMD_NACK;
        resp->payload[0] = ERR_UNKNOWN_CMD;
        resp->len = 1;
        break;
    }

    /* 재접속이 필요한 쓰기 커맨드였다면 wifiTask에 통지 (WIFI/SERVER/NET 셋 다 재접속 필요) */
    if (req->cmd == CMD_WRITE_WIFI_CFG || req->cmd == CMD_WRITE_SERVER_CFG || req->cmd == CMD_WRITE_NET_CFG)
    {
        extern osMessageQueueId_t qWifiCmd;
        WifiCmdMsg_t m = { .id = WIFI_CMD_RECONFIGURE };
        osMessageQueuePut(qWifiCmd, &m, 0, 0);
    }
}

/* -------------------------------------------------------------------------- */
void App_PcComm_OnUart3RxEvent(uint16_t size)
{
    s_uart3RxLen = size;
    osSemaphoreRelease(semUart3Event);
}

void App_PcComm_OnUsbRx(const uint8_t *data, uint32_t len)
{
    if (len > PCCOMM_RX_BUF_LEN)
    {
        len = PCCOMM_RX_BUF_LEN;
    }

    UsbRxMsg_t msg;
    memcpy(msg.data, data, len);
    msg.len = len;

    /* USB 인터럽트/태스크 컨텍스트에서 호출되므로 non-blocking put */
    osMessageQueuePut(s_qUsbRx, &msg, 0, 0);
}

/* -------------------------------------------------------------------------- */
static void App_PcUart_Task(void *argument)
{
    (void)argument;
    PcFrame_t req, resp;
    PcErrCode_t err;
    uint8_t txBuf[PC_FRAME_MAX_SIZE];

    osEventFlagsWait(egSysStatus, EVT_CONFIG_LOADED, osFlagsWaitAll, osWaitForever);

    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart3, s_uart3RxBuf, PCCOMM_RX_BUF_LEN);

    for (;;)
    {
        if (osSemaphoreAcquire(semUart3Event, osWaitForever) != osOK)
        {
            continue;
        }

        if (Protocol_DecodePcFrame(s_uart3RxBuf, s_uart3RxLen, &req, &err))
        {
            ProcessCommand(&req, &resp);
        }
        else
        {
            resp.cmd = CMD_NACK;
            resp.payload[0] = (uint8_t)err;
            resp.len = 1;
        }

        uint32_t n = Protocol_EncodePcFrame(&resp, txBuf, sizeof(txBuf));
        if (n > 0)
        {
            /* USART3 DE(Driver Enable)는 CubeMX Advanced Parameter로 HW 자동제어 */
            HAL_UART_Transmit(&huart3, txBuf, (uint16_t)n, 500);
        }

        (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart3, s_uart3RxBuf, PCCOMM_RX_BUF_LEN);
    }
}

static void App_PcUsb_Task(void *argument)
{
    (void)argument;
    UsbRxMsg_t rx;
    PcFrame_t req, resp;
    PcErrCode_t err;
    uint8_t txBuf[PC_FRAME_MAX_SIZE];

    osEventFlagsWait(egSysStatus, EVT_CONFIG_LOADED, osFlagsWaitAll, osWaitForever);

    for (;;)
    {
        if (osMessageQueueGet(s_qUsbRx, &rx, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        if (Protocol_DecodePcFrame(rx.data, rx.len, &req, &err))
        {
            ProcessCommand(&req, &resp);
        }
        else
        {
            resp.cmd = CMD_NACK;
            resp.payload[0] = (uint8_t)err;
            resp.len = 1;
        }

        uint32_t n = Protocol_EncodePcFrame(&resp, txBuf, sizeof(txBuf));
        if (n > 0)
        {
            CDC_Transmit_FS(txBuf, (uint16_t)n);
        }
    }
}

static void App_PcDataPush_Task(void *argument)
{
    (void)argument;
    SurgeData_t data;
    PcFrame_t frame;
    uint8_t txBuf[PC_FRAME_MAX_SIZE];

    for (;;)
    {
        if (osMessageQueueGet(qFpgaToPc, &data, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        frame.cmd = CMD_DATA_PUSH;
        frame.len = (uint8_t)sizeof(SurgeData_t);
        memcpy(frame.payload, &data, sizeof(SurgeData_t));

        uint32_t n = Protocol_EncodePcFrame(&frame, txBuf, sizeof(txBuf));
        if (n > 0)
        {
            HAL_UART_Transmit(&huart3, txBuf, (uint16_t)n, 200);   /* RS485로 통지 */
            CDC_Transmit_FS(txBuf, (uint16_t)n);                   /* USB로 통지 */
        }
    }
}

void App_PcComm_Init(void)
{
    const osMessageQueueAttr_t qAttr1 = { .name = "qFpgaToPc" };
    qFpgaToPc = osMessageQueueNew(8, sizeof(SurgeData_t), &qAttr1);

    const osMessageQueueAttr_t qAttr2 = { .name = "qUsbRx" };
    s_qUsbRx = osMessageQueueNew(4, sizeof(UsbRxMsg_t), &qAttr2);

    const osSemaphoreAttr_t sAttr = { .name = "semUart3Event" };
    semUart3Event = osSemaphoreNew(1, 0, &sAttr);

    const osThreadAttr_t tAttr1 = { .name = "pcUartTask", .priority = osPriorityNormal, .stack_size = 512 * 4 };
    s_pcUartTaskHandle = osThreadNew(App_PcUart_Task, NULL, &tAttr1);

    const osThreadAttr_t tAttr2 = { .name = "pcUsbTask", .priority = osPriorityNormal, .stack_size = 512 * 4 };
    s_pcUsbTaskHandle = osThreadNew(App_PcUsb_Task, NULL, &tAttr2);

    const osThreadAttr_t tAttr3 = { .name = "pcDataPushTask", .priority = osPriorityNormal, .stack_size = 384 * 4 };
    s_pcDataPushTaskHandle = osThreadNew(App_PcDataPush_Task, NULL, &tAttr3);
}
