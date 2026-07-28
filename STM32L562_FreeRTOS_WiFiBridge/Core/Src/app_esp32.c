#include <string.h>
#include <stdio.h>
#include "app_esp32.h"
#include "app_eeprom.h"
#include "ring_buffer.h"
#include "cmsis_os2.h"

typedef enum {
    ESP32_REQ_CONNECT = 0,
    ESP32_REQ_SEND,
} Esp32ReqType_t;

typedef struct {
    Esp32ReqType_t type;
    const uint8_t *data;
    uint16_t       len;
} Esp32Request_t;

static uint8_t      s_rxStorage[ESP32_UART_RX_RINGBUF_SIZE];
static RingBuffer_t  s_rxRing;
static uint8_t       s_rxByte;

static char          s_atBuf[ESP32_AT_LINE_MAX_LEN + 1];
static uint16_t      s_atLen;

static osMessageQueueId_t s_reqQueue;
static osSemaphoreId_t    s_sendDoneSem;
static osEventFlagsId_t   s_connFlags;
static volatile bool      s_lastSendResult;

/* ------------------------------------------------------------------------ *
 * Low level AT helpers - only ever called from App_Esp32_Task's context,
 * so no locking is required around s_atBuf / the UART.
 * ------------------------------------------------------------------------ */
static void Esp32_FlushRx(void)
{
    uint8_t b;
    while (RingBuffer_GetByte(&s_rxRing, &b)) {
        /* discard */
    }
}

static bool Esp32_WaitForToken(const char *token, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    s_atLen = 0;
    s_atBuf[0] = '\0';

    for (;;) {
        uint8_t b;
        bool got = false;
        while (RingBuffer_GetByte(&s_rxRing, &b)) {
            got = true;
            if (s_atLen < ESP32_AT_LINE_MAX_LEN) {
                s_atBuf[s_atLen++] = (char)b;
                s_atBuf[s_atLen] = '\0';
            } else {
                /* rolling window: drop oldest half to keep scanning cheap */
                memmove(s_atBuf, s_atBuf + (ESP32_AT_LINE_MAX_LEN / 2),
                        ESP32_AT_LINE_MAX_LEN / 2);
                s_atLen = ESP32_AT_LINE_MAX_LEN / 2;
                s_atBuf[s_atLen++] = (char)b;
                s_atBuf[s_atLen] = '\0';
            }
            if (strstr(s_atBuf, token) != NULL) {
                return true;
            }
            if (strstr(s_atBuf, "ERROR") != NULL) {
                return false;
            }
        }
        if (!got) {
            if ((HAL_GetTick() - start) >= timeout_ms) {
                return false;
            }
            osDelay(2);
        }
    }
}

static bool Esp32_SendCommand(const char *cmd, const char *expect_token, uint32_t timeout_ms)
{
    Esp32_FlushRx();
    HAL_UART_Transmit(ESP32_UART_HANDLE, (uint8_t *)cmd, (uint16_t)strlen(cmd), 200U);
    HAL_UART_Transmit(ESP32_UART_HANDLE, (uint8_t *)"\r\n", 2U, 100U);
    return Esp32_WaitForToken(expect_token, timeout_ms);
}

static bool Esp32_Ping(void)
{
    for (int i = 0; i < 3; i++) {
        if (Esp32_SendCommand("AT", "OK", 1000U)) {
            return true;
        }
        osDelay(200);
    }
    return false;
}

/* ------------------------------------------------------------------------ *
 * Connect sequence: join AP (DHCP or static), open TCP session to server.
 * ------------------------------------------------------------------------ */
static bool Esp32_DoConnect(const AppConfig_t *cfg)
{
    char cmd[192];

    if (cfg->ap_ssid[0] == '\0') {
        return false; /* nothing configured yet */
    }

    if (!Esp32_Ping())                                    return false;
    if (!Esp32_SendCommand("ATE0", "OK", 1000U))          return false;
    if (!Esp32_SendCommand("AT+CWMODE=1", "OK", 1000U))   return false;

    if (cfg->dhcp_enable) {
        if (!Esp32_SendCommand("AT+CWDHCP=1,1", "OK", 1000U)) return false;
    } else {
        if (!Esp32_SendCommand("AT+CWDHCP=1,0", "OK", 1000U)) return false;
        snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"",
                 cfg->static_ip, cfg->static_gw, cfg->static_mask);
        if (!Esp32_SendCommand(cmd, "OK", 2000U)) return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", cfg->ap_ssid, cfg->ap_pass);
    if (!Esp32_SendCommand(cmd, "OK", ESP32_WIFI_JOIN_TIMEOUT_MS)) {
        return false;
    }

    if (!Esp32_SendCommand("AT+CIPMUX=0", "OK", 1000U)) return false;

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
             cfg->server_ip, (unsigned)cfg->server_port);
    if (!Esp32_SendCommand(cmd, "OK", ESP32_TCP_CONNECT_TIMEOUT_MS)) {
        return false;
    }

    return true;
}

static bool Esp32_DoSend(const uint8_t *data, uint16_t len)
{
    char cmd[24];

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)len);
    if (!Esp32_SendCommand(cmd, ">", 2000U)) {
        return false;
    }

    Esp32_FlushRx();
    HAL_UART_Transmit(ESP32_UART_HANDLE, (uint8_t *)data, len, ESP32_SEND_TIMEOUT_MS);

    return Esp32_WaitForToken("SEND OK", ESP32_SEND_TIMEOUT_MS);
}

/* ------------------------------------------------------------------------ */

void App_Esp32_Init(void)
{
    RingBuffer_Init(&s_rxRing, s_rxStorage, ESP32_UART_RX_RINGBUF_SIZE);
    s_atLen = 0;

    s_reqQueue    = osMessageQueueNew(4, sizeof(Esp32Request_t), NULL);
    s_sendDoneSem = osSemaphoreNew(1, 0, NULL);
    s_connFlags   = osEventFlagsNew(NULL);
    s_lastSendResult = false;

    HAL_UART_Receive_IT(ESP32_UART_HANDLE, &s_rxByte, 1U);
}

void App_Esp32_UART_RxCpltCallback(void)
{
    RingBuffer_PutByte(&s_rxRing, s_rxByte);
    HAL_UART_Receive_IT(ESP32_UART_HANDLE, &s_rxByte, 1U);
}

bool App_Esp32_RequestConnect(void)
{
    Esp32Request_t req = { .type = ESP32_REQ_CONNECT, .data = NULL, .len = 0 };
    return (osMessageQueuePut(s_reqQueue, &req, 0, 0) == osOK);
}

bool App_Esp32_SendMeasurementData(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    /* drain any stale completion signal from a previous timed-out call */
    osSemaphoreAcquire(s_sendDoneSem, 0);

    Esp32Request_t req = { .type = ESP32_REQ_SEND, .data = data, .len = len };
    if (osMessageQueuePut(s_reqQueue, &req, 0, 100U) != osOK) {
        return false;
    }

    if (osSemaphoreAcquire(s_sendDoneSem, timeout_ms) != osOK) {
        return false; /* timed out waiting for esp32 task */
    }
    return s_lastSendResult;
}

bool App_Esp32_IsConnected(void)
{
    return (osEventFlagsGet(s_connFlags) & ESP32_WIFI_CONNECTED_BIT) != 0U;
}

void App_Esp32_Task(void *argument)
{
    (void)argument;
    Esp32Request_t req;

    for (;;) {
        if (osMessageQueueGet(s_reqQueue, &req, NULL, osWaitForever) != osOK) {
            continue;
        }

        switch (req.type) {
        case ESP32_REQ_CONNECT: {
            AppConfig_t cfg;
            App_Eeprom_LoadConfig(&cfg);
            osEventFlagsClear(s_connFlags, ESP32_WIFI_CONNECTED_BIT);
            if (Esp32_DoConnect(&cfg)) {
                osEventFlagsSet(s_connFlags, ESP32_WIFI_CONNECTED_BIT);
            }
            break;
        }

        case ESP32_REQ_SEND: {
            bool ok = App_Esp32_IsConnected();
            if (!ok) {
                AppConfig_t cfg;
                App_Eeprom_LoadConfig(&cfg);
                ok = Esp32_DoConnect(&cfg);
                if (ok) {
                    osEventFlagsSet(s_connFlags, ESP32_WIFI_CONNECTED_BIT);
                } else {
                    osEventFlagsClear(s_connFlags, ESP32_WIFI_CONNECTED_BIT);
                }
            }
            if (ok) {
                ok = Esp32_DoSend(req.data, req.len);
                if (!ok) {
                    /* link likely dropped mid-transfer */
                    osEventFlagsClear(s_connFlags, ESP32_WIFI_CONNECTED_BIT);
                }
            }
            s_lastSendResult = ok;
            osSemaphoreRelease(s_sendDoneSem);
            break;
        }

        default:
            break;
        }
    }
}
