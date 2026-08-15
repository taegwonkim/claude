/**
 * app_wifi.c — ESP32-C3 AT 명령 드라이버 + wifiTask 구현
 */
#include "app_wifi.h"
#include "app_config.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;

#define WIFI_RX_BUF_LEN        256u
#define WIFI_AT_TIMEOUT_MS     3000u
#define WIFI_RECONNECT_DELAY_MS 5000u

osMessageQueueId_t qFpgaToWifi;
osMessageQueueId_t qWifiCmd;
osMutexId_t        mutexWifiUart;
osSemaphoreId_t    semUart1Event;

static osThreadId_t   s_wifiTaskHandle;
static uint8_t        s_rxBuf[WIFI_RX_BUF_LEN];
static volatile uint16_t s_rxLen;

typedef enum
{
    WIFI_STATE_RESET = 0,
    WIFI_STATE_AT_TEST,
    WIFI_STATE_SET_MODE,
    WIFI_STATE_JOIN_AP,
    WIFI_STATE_CONNECT_SERVER,
    WIFI_STATE_CONNECTED,
} WifiState_t;

static WifiState_t s_state = WIFI_STATE_RESET;

/* ------------------------------------------------------------------------ */
/* USART1 IDLE/RxEvent 콜백에서 호출 (stm32l5xx_it.c 또는 HAL_UARTEx_RxEventCallback) */
void App_WiFi_OnUartRxEvent(uint16_t size)
{
    s_rxLen = size;
    osSemaphoreRelease(semUart1Event);
}

/* AT 명령 송신 + "expect" 문자열(예 "OK")이 응답에 포함될 때까지 대기.
 * 반환: true = expect 수신, false = timeout/에러 응답("ERROR") */
static bool WiFi_SendAtCmd(const char *cmd, const char *expect, uint32_t timeoutMs)
{
    bool ok = false;

    osMutexAcquire(mutexWifiUart, osWaitForever);

    /* 이전 응답 잔재 제거 후 새 수신 시작 */
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_rxBuf, WIFI_RX_BUF_LEN);

    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 200);

    uint32_t start = osKernelGetTickCount();
    while ((osKernelGetTickCount() - start) < timeoutMs)
    {
        uint32_t remain = timeoutMs - (osKernelGetTickCount() - start);
        if (osSemaphoreAcquire(semUart1Event, remain) != osOK)
        {
            break; /* timeout */
        }

        s_rxBuf[(s_rxLen < WIFI_RX_BUF_LEN) ? s_rxLen : (WIFI_RX_BUF_LEN - 1)] = 0;

        if (strstr((char *)s_rxBuf, expect) != NULL)
        {
            ok = true;
            break;
        }
        if (strstr((char *)s_rxBuf, "ERROR") != NULL)
        {
            ok = false;
            break;
        }
        /* expect 미포함 응답(URC 등) -> 다음 이벤트까지 계속 대기 */
        (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_rxBuf, WIFI_RX_BUF_LEN);
    }

    HAL_UART_AbortReceive(&huart1);
    osMutexRelease(mutexWifiUart);
    return ok;
}

static void WiFi_HardReset(void)
{
    HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_RESET);
    osDelay(150);
    HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_SET);
    osDelay(1000); /* 모듈 부팅 대기 */
}

static bool WiFi_JoinAndConnect(void)
{
    SystemConfig_t cfg;
    char cmd[160];

    App_Config_GetCopy(&cfg);

    if (!WiFi_SendAtCmd("ATE0\r\n", "OK", WIFI_AT_TIMEOUT_MS)) return false;
    if (!WiFi_SendAtCmd("AT+CWMODE=1\r\n", "OK", WIFI_AT_TIMEOUT_MS)) return false;

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", cfg.ssid, cfg.password);
    if (!WiFi_SendAtCmd(cmd, "OK", 15000u)) return false;
    osEventFlagsSet(egSysStatus, EVT_WIFI_JOINED);

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%u.%u.%u.%u\",%u\r\n",
             cfg.serverIp[0], cfg.serverIp[1], cfg.serverIp[2], cfg.serverIp[3], cfg.serverPort);
    if (!WiFi_SendAtCmd(cmd, "OK", 10000u)) return false;

    osEventFlagsSet(egSysStatus, EVT_SERVER_CONNECTED);
    return true;
}

static bool WiFi_SendSurgeData(const SurgeData_t *d)
{
    /* 서버 프로토콜: 콤마 구분 텍스트 라인 (실제 서버 스펙에 맞춰 조정) */
    char payload[64];
    int len = snprintf(payload, sizeof(payload), "SEQ:%u,CH:%u,%u,%u,%u,%u,%u\r\n",
                        d->seq, d->channel[0], d->channel[1], d->channel[2],
                        d->channel[3], d->channel[4], d->channel[5]);

    char sendCmd[32];
    snprintf(sendCmd, sizeof(sendCmd), "AT+CIPSEND=%d\r\n", len);

    if (!WiFi_SendAtCmd(sendCmd, ">", 2000u))
    {
        return false;
    }

    osMutexAcquire(mutexWifiUart, osWaitForever);
    HAL_UART_Transmit(&huart1, (uint8_t *)payload, (uint16_t)len, 500);
    osMutexRelease(mutexWifiUart);

    return true;
}

static void App_WiFi_Task(void *argument)
{
    (void)argument;
    SurgeData_t data;
    WifiCmdMsg_t cmdMsg;

    osEventFlagsWait(egSysStatus, EVT_CONFIG_LOADED, osFlagsWaitAll, osWaitForever);

    for (;;)
    {
        switch (s_state)
        {
        case WIFI_STATE_RESET:
            WiFi_HardReset();
            s_state = WIFI_STATE_AT_TEST;
            break;

        case WIFI_STATE_AT_TEST:
            s_state = WiFi_SendAtCmd("AT\r\n", "OK", WIFI_AT_TIMEOUT_MS) ? WIFI_STATE_SET_MODE : WIFI_STATE_RESET;
            break;

        case WIFI_STATE_SET_MODE:
        case WIFI_STATE_JOIN_AP:
        case WIFI_STATE_CONNECT_SERVER:
            if (WiFi_JoinAndConnect())
            {
                s_state = WIFI_STATE_CONNECTED;
            }
            else
            {
                osEventFlagsClear(egSysStatus, EVT_WIFI_JOINED | EVT_SERVER_CONNECTED);
                osDelay(WIFI_RECONNECT_DELAY_MS);
                s_state = WIFI_STATE_RESET;
            }
            break;

        case WIFI_STATE_CONNECTED:
            /* 재접속 지시가 있으면 우선 처리 */
            if (osMessageQueueGet(qWifiCmd, &cmdMsg, NULL, 0) == osOK)
            {
                osEventFlagsClear(egSysStatus, EVT_WIFI_JOINED | EVT_SERVER_CONNECTED);
                (void)WiFi_SendAtCmd("AT+CIPCLOSE\r\n", "OK", 2000u);
                s_state = WIFI_STATE_RESET;
                break;
            }

            if (osMessageQueueGet(qFpgaToWifi, &data, NULL, 1000u) == osOK)
            {
                if (!WiFi_SendSurgeData(&data))
                {
                    /* 전송 실패 -> 연결 끊김으로 간주, 재접속 */
                    osEventFlagsClear(egSysStatus, EVT_SERVER_CONNECTED);
                    s_state = WIFI_STATE_RESET;
                }
            }
            break;
        }
    }
}

void App_WiFi_Init(void)
{
    const osMessageQueueAttr_t qAttr1 = { .name = "qFpgaToWifi" };
    qFpgaToWifi = osMessageQueueNew(8, sizeof(SurgeData_t), &qAttr1);

    const osMessageQueueAttr_t qAttr2 = { .name = "qWifiCmd" };
    qWifiCmd = osMessageQueueNew(4, sizeof(WifiCmdMsg_t), &qAttr2);

    const osMutexAttr_t mAttr = { .name = "mutexWifiUart" };
    mutexWifiUart = osMutexNew(&mAttr);

    const osSemaphoreAttr_t sAttr = { .name = "semUart1Event" };
    semUart1Event = osSemaphoreNew(1, 0, &sAttr);

    const osThreadAttr_t tAttr = {
        .name = "wifiTask",
        .priority = osPriorityAboveNormal,
        .stack_size = 768 * 4,
    };
    s_wifiTaskHandle = osThreadNew(App_WiFi_Task, NULL, &tAttr);
}
