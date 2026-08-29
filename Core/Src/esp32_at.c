/*
 * esp32_at.c
 *
 * esp32_at.h 구현부.
 *
 * 전체 흐름(상태 머신):
 *   INIT -> MODULE_CHECK(AT, ATE0, CWMODE) -> WIFI_CONNECTING(CWJAP)
 *        -> WIFI_CONNECTED -> TCP_CONNECTING(CIPMUX, CIPMODE, CIPSTART)
 *        -> TCP_CONNECTED (정상 통신)
 *   통신 중 "WIFI DISCONNECT" / "CLOSED" 같은 비동기 메시지나 SEND 실패를 감지하면
 *   LINK_DOWN -> RECONNECT_WAIT(지수 백오프 대기) -> 다시 MODULE_CHECK 부터 재시도.
 *
 * UART 수신은 인터럽트(1바이트 단위)로 링버퍼에 쌓고, 실제 파싱/상태전이는
 * ESP32_Process() 가 메인 루프에서 호출될 때 수행한다(ISR 부담 최소화).
 */

#include "esp32_at.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* 내부 상태 변수                                                      */
/* ------------------------------------------------------------------ */
static UART_HandleTypeDef *s_huart;

static char     s_ssid[ESP32_SSID_MAX];
static char     s_pass[ESP32_PASS_MAX];
static char     s_server_ip[ESP32_SERVER_IP_MAX];
static uint16_t s_server_port;

static ESP32_DataCallback_t  s_data_cb;
static ESP32_StateCallback_t s_state_cb;

static ESP32_State_t s_state = ESP32_STATE_INIT;
static uint8_t   s_step;               /* 상태 내부의 세부 진행 단계 */
static uint32_t  s_deadline;           /* 현재 대기 중인 응답의 타임아웃 시각 */
static uint16_t  s_reconnect_delay_ms; /* 재접속 백오프 대기 시간(점점 증가) */
static uint32_t  s_reconnect_deadline;
static uint8_t   s_fatal_retry_count;  /* 모듈 자체 무응답 연속 횟수 */

/* UART RX 링버퍼 (ISR 가 채우고 Process() 가 소비) */
static volatile uint8_t  s_rx_ring[ESP32_RX_RING_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static uint8_t  s_rx_byte; /* HAL_UART_Receive_IT() 의 목적지 (1바이트씩 재무장) */

/* 줄 단위 파서 */
static char     s_line_buf[ESP32_LINE_BUF_SIZE];
static uint16_t s_line_len;

/* +IPD,<len>: 뒤에 오는 바이너리 payload 파서 */
static bool     s_in_ipd;
static uint32_t s_ipd_remaining;
static uint8_t  s_ipd_chunk[128];
static uint16_t s_ipd_chunk_len;

/* AT 응답 매칭 플래그 (HandleLine() 이 세팅, FSM 이 읽고 소비) */
static volatile bool s_got_ok;
static volatile bool s_got_error;
static volatile bool s_got_send_ok;
static volatile bool s_got_wifi_gotip;
static volatile bool s_link_down_evt; /* WIFI DISCONNECT / CLOSED 비동기 감지 */

/* ------------------------------------------------------------------ */
/* 내부 함수 선언                                                      */
/* ------------------------------------------------------------------ */
static void DrainRing(void);
static void ParseByte(uint8_t b);
static void HandleLine(const char *line);
static void SendCmd(const char *cmd);
static void SetState(ESP32_State_t new_state);
static void GoToReconnectWait(void);
static void RetryOrFatal(void);

/* ------------------------------------------------------------------ */
/* 공개 API 구현                                                       */
/* ------------------------------------------------------------------ */

void ESP32_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;

    s_state = ESP32_STATE_INIT;
    s_step = 0;
    s_reconnect_delay_ms = ESP32_RECONNECT_BASE_MS;
    s_fatal_retry_count = 0;

    s_rx_head = 0;
    s_rx_tail = 0;
    s_line_len = 0;
    s_in_ipd = false;
    s_ipd_chunk_len = 0;

    s_got_ok = s_got_error = s_got_send_ok = s_got_wifi_gotip = false;
    s_link_down_evt = false;

    /* 1바이트 수신 인터럽트를 계속 재무장하는 방식(F1 계열에 DMA 없이도 동작) */
    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

void ESP32_SetWiFiCredentials(const char *ssid, const char *password)
{
    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    strncpy(s_pass, password, sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = '\0';
}

void ESP32_SetServer(const char *ip, uint16_t port)
{
    strncpy(s_server_ip, ip, sizeof(s_server_ip) - 1);
    s_server_ip[sizeof(s_server_ip) - 1] = '\0';
    s_server_port = port;
}

void ESP32_SetDataCallback(ESP32_DataCallback_t cb)  { s_data_cb = cb; }
void ESP32_SetStateCallback(ESP32_StateCallback_t cb) { s_state_cb = cb; }
ESP32_State_t ESP32_GetState(void) { return s_state; }

HAL_StatusTypeDef ESP32_Send(const uint8_t *data, uint16_t len)
{
    if (s_state != ESP32_STATE_TCP_CONNECTED || len == 0) {
        return HAL_ERROR;
    }

    char cmd[24];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)len);
    SendCmd(cmd);

    /* AT+CIPSEND=<len> 에 대해 모듈이 "OK" 후 '>' 프롬프트를 보내면 그 뒤에
     * 실제 데이터를 보내야 한다. '>' 는 개행이 없어 줄 파서로 못 잡으므로,
     * 여기서는 OK 확인 후 짧은 지연을 두는 실전형 방식을 쓴다. */
    uint32_t deadline = HAL_GetTick() + ESP32_CMD_TIMEOUT_MS;
    while (!s_got_ok && !s_got_error && HAL_GetTick() < deadline) {
        DrainRing();
    }
    if (!s_got_ok) {
        return HAL_TIMEOUT;
    }

    HAL_Delay(20);
    HAL_StatusTypeDef st = HAL_UART_Transmit(s_huart, (uint8_t *)data, len, 2000);
    if (st != HAL_OK) {
        return st;
    }

    s_got_send_ok = false;
    s_got_error = false;
    deadline = HAL_GetTick() + ESP32_CMD_TIMEOUT_MS;
    while (!s_got_send_ok && !s_got_error && HAL_GetTick() < deadline) {
        DrainRing();
    }

    if (s_got_error) {
        /* SEND FAIL 은 연결이 끊어졌을 가능성이 높음 -> 재접속 절차 트리거 */
        s_link_down_evt = true;
        return HAL_ERROR;
    }
    return s_got_send_ok ? HAL_OK : HAL_TIMEOUT;
}

void ESP32_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != s_huart->Instance) {
        return;
    }

    uint16_t next = (uint16_t)((s_rx_head + 1) % ESP32_RX_RING_SIZE);
    if (next != s_rx_tail) { /* 가득 차면 드롭(백오프로 다시 맞춰짐) */
        s_rx_ring[s_rx_head] = s_rx_byte;
        s_rx_head = next;
    }

    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

void ESP32_Process(void)
{
    DrainRing();
    uint32_t now = HAL_GetTick();

    switch (s_state) {

    case ESP32_STATE_INIT:
        s_step = 0;
        SetState(ESP32_STATE_MODULE_CHECK);
        break;

    case ESP32_STATE_MODULE_CHECK:
        switch (s_step) {
        case 0:
            SendCmd("AT");
            s_deadline = now + ESP32_CMD_TIMEOUT_MS;
            s_step = 1;
            break;
        case 1:
            if (s_got_ok) {
                s_fatal_retry_count = 0;
                SendCmd("ATE0"); /* 에코 off: 응답 파싱을 단순하게 유지 */
                s_deadline = now + ESP32_CMD_TIMEOUT_MS;
                s_step = 2;
            } else if (now > s_deadline) {
                RetryOrFatal();
            }
            break;
        case 2:
            if (s_got_ok || s_got_error) {
                SendCmd("AT+CWMODE=1"); /* 1 = Station 모드 */
                s_deadline = now + ESP32_CMD_TIMEOUT_MS;
                s_step = 3;
            } else if (now > s_deadline) {
                RetryOrFatal();
            }
            break;
        case 3:
            if (s_got_ok) {
                s_step = 0;
                SetState(ESP32_STATE_WIFI_CONNECTING);
            } else if (s_got_error || now > s_deadline) {
                RetryOrFatal();
            }
            break;
        }
        break;

    case ESP32_STATE_WIFI_CONNECTING:
        switch (s_step) {
        case 0: {
            char cmd[ESP32_SSID_MAX + ESP32_PASS_MAX + 24];
            snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", s_ssid, s_pass);
            SendCmd(cmd);
            s_deadline = now + ESP32_WIFI_TIMEOUT_MS; /* AP 접속은 수 초~수십 초 소요 */
            s_step = 1;
            break;
        }
        case 1:
            if (s_got_ok && s_got_wifi_gotip) {
                s_reconnect_delay_ms = ESP32_RECONNECT_BASE_MS; /* 성공 시 백오프 초기화 */
                s_step = 0;
                SetState(ESP32_STATE_WIFI_CONNECTED);
            } else if (s_got_error || now > s_deadline) {
                GoToReconnectWait();
            }
            break;
        }
        break;

    case ESP32_STATE_WIFI_CONNECTED:
        s_step = 0;
        SetState(ESP32_STATE_TCP_CONNECTING);
        break;

    case ESP32_STATE_TCP_CONNECTING:
        switch (s_step) {
        case 0:
            SendCmd("AT+CIPMUX=0"); /* 단일 연결 모드 */
            s_deadline = now + ESP32_CMD_TIMEOUT_MS;
            s_step = 1;
            break;
        case 1:
            if (s_got_ok) {
                SendCmd("AT+CIPMODE=0"); /* 일반(비-투명) 모드: CIPSEND 프레이밍 사용 */
                s_deadline = now + ESP32_CMD_TIMEOUT_MS;
                s_step = 2;
            } else if (s_got_error || now > s_deadline) {
                GoToReconnectWait();
            }
            break;
        case 2:
            if (s_got_ok) {
                char cmd[ESP32_SERVER_IP_MAX + 40];
                snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                         s_server_ip, (unsigned)s_server_port);
                SendCmd(cmd);
                s_deadline = now + ESP32_TCP_TIMEOUT_MS;
                s_step = 3;
            } else if (s_got_error || now > s_deadline) {
                GoToReconnectWait();
            }
            break;
        case 3:
            if (s_got_ok) {
                s_reconnect_delay_ms = ESP32_RECONNECT_BASE_MS;
                s_step = 0;
                SetState(ESP32_STATE_TCP_CONNECTED);
            } else if (s_got_error || now > s_deadline) {
                GoToReconnectWait();
            }
            break;
        }
        break;

    case ESP32_STATE_TCP_CONNECTED:
        if (s_link_down_evt) {
            s_link_down_evt = false;
            SetState(ESP32_STATE_LINK_DOWN);
        }
        /* 정상 상태: 송신은 ESP32_Send(), 수신은 파서가 데이터 콜백으로 전달 */
        break;

    case ESP32_STATE_LINK_DOWN:
        GoToReconnectWait();
        break;

    case ESP32_STATE_RECONNECT_WAIT:
        if (now >= s_reconnect_deadline) {
            /* 지수 백오프: 실패가 반복될수록 재시도 간격을 늘려 서버/AP 부담을 줄임 */
            uint32_t next = (uint32_t)s_reconnect_delay_ms * 2;
            s_reconnect_delay_ms = (next > ESP32_RECONNECT_MAX_MS)
                                       ? ESP32_RECONNECT_MAX_MS
                                       : (uint16_t)next;
            /* 모듈이 중간에 리셋됐을 가능성도 있으므로 처음(AT 체크)부터 다시 진행 */
            s_step = 0;
            SetState(ESP32_STATE_MODULE_CHECK);
        }
        break;

    case ESP32_STATE_FATAL_ERROR:
    default:
        /* AT 자체가 응답하지 않는 상태: 배선/전원 문제 가능성.
         * 필요하면 여기서 GPIO로 ESP32 EN 핀을 토글하거나 HAL_NVIC_SystemReset() 호출 */
        break;
    }
}

/* ------------------------------------------------------------------ */
/* 내부 함수 구현                                                      */
/* ------------------------------------------------------------------ */

static void DrainRing(void)
{
    while (s_rx_tail != s_rx_head) {
        uint8_t b = s_rx_ring[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1) % ESP32_RX_RING_SIZE);
        ParseByte(b);
    }
}

static void ParseByte(uint8_t b)
{
    if (s_in_ipd) {
        s_ipd_chunk[s_ipd_chunk_len++] = b;
        s_ipd_remaining--;

        if (s_ipd_chunk_len == sizeof(s_ipd_chunk) || s_ipd_remaining == 0) {
            if (s_data_cb) {
                s_data_cb(s_ipd_chunk, s_ipd_chunk_len);
            }
            s_ipd_chunk_len = 0;
        }
        if (s_ipd_remaining == 0) {
            s_in_ipd = false;
        }
        return;
    }

    if (b == '\n') {
        if (s_line_len > 0 && s_line_buf[s_line_len - 1] == '\r') {
            s_line_len--;
        }
        s_line_buf[s_line_len] = '\0';
        if (s_line_len > 0) {
            HandleLine(s_line_buf);
        }
        s_line_len = 0;
        return;
    }

    if (s_line_len < ESP32_LINE_BUF_SIZE - 1) {
        s_line_buf[s_line_len++] = (char)b;
    } else {
        s_line_len = 0; /* 비정상적으로 긴 줄: 오버플로 방지용 드롭 */
        return;
    }

    /* "+IPD,<len>:" 헤더는 개행 없이 바로 바이너리 데이터가 이어지므로
     * 줄 종료를 기다리지 않고 콜론을 만나는 즉시 감지해서 모드를 전환한다.
     * AT+CIPMUX=0(단일 연결) 기준 포맷이며, 멀티 커넥션(+IPD,<id>,<len>:)은
     * 다루지 않는다. */
    if (s_line_len >= 5 && memcmp(s_line_buf, "+IPD,", 5) == 0) {
        char *colon = memchr(s_line_buf, ':', s_line_len);
        if (colon != NULL) {
            long len = strtol(s_line_buf + 5, NULL, 10);
            s_ipd_remaining = (len > 0) ? (uint32_t)len : 0;
            s_ipd_chunk_len = 0;
            s_in_ipd = (s_ipd_remaining > 0);
            s_line_len = 0;
        }
    }
}

static void HandleLine(const char *line)
{
    if (strcmp(line, "OK") == 0) {
        s_got_ok = true;
    } else if (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0) {
        s_got_error = true;
    } else if (strcmp(line, "SEND OK") == 0) {
        s_got_send_ok = true;
    } else if (strcmp(line, "SEND FAIL") == 0) {
        s_got_error = true;
    } else if (strstr(line, "WIFI GOT IP") != NULL) {
        s_got_wifi_gotip = true;
    } else if (strstr(line, "WIFI DISCONNECT") != NULL) {
        s_link_down_evt = true; /* AP 접속 자체가 끊어짐 */
    } else if (strcmp(line, "CLOSED") == 0) {
        s_link_down_evt = true; /* TCP 연결이 서버/네트워크 쪽에서 종료됨 */
    }
    /* "WIFI CONNECTED", "busy p...", "ready" 등 그 외 라인은 무시 */
}

static void SendCmd(const char *cmd)
{
    s_got_ok = false;
    s_got_error = false;
    s_got_send_ok = false;
    s_got_wifi_gotip = false;

    HAL_UART_Transmit(s_huart, (uint8_t *)cmd, (uint16_t)strlen(cmd), 100);
    HAL_UART_Transmit(s_huart, (uint8_t *)"\r\n", 2, 50);
}

static void SetState(ESP32_State_t new_state)
{
    s_state = new_state;
    if (s_state_cb) {
        s_state_cb(new_state);
    }
}

static void GoToReconnectWait(void)
{
    s_reconnect_deadline = HAL_GetTick() + s_reconnect_delay_ms;
    SetState(ESP32_STATE_RECONNECT_WAIT);
}

static void RetryOrFatal(void)
{
    s_fatal_retry_count++;
    if (s_fatal_retry_count >= 5) {
        SetState(ESP32_STATE_FATAL_ERROR);
    } else {
        GoToReconnectWait();
    }
}
