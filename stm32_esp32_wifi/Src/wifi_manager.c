#include <string.h>
#include <stdio.h>
#include "wifi_manager.h"
#include "esp32_at.h"

/* ---- 타이밍 상수 ---- */
#define AT_TIMEOUT_SHORT_MS        3000
#define AT_TIMEOUT_AP_JOIN_MS      20000
#define AT_TIMEOUT_SERVER_MS       10000
#define HEALTH_CHECK_INTERVAL_MS   5000
#define RETRY_BACKOFF_INITIAL_MS   2000
#define RETRY_BACKOFF_MAX_MS       60000

static wifi_ap_config_t     s_ap_cfg;
static wifi_ip_config_t     s_ip_cfg;
static wifi_server_config_t s_server_cfg;
static bool                 s_server_enabled;

static wifi_state_t s_state;
static wifi_state_t s_retry_target;
static uint32_t     s_retry_delay_ms;
static uint32_t     s_state_enter_tick;
static uint32_t     s_last_health_check_tick;

/* ---------------------------------------------------------------- */

static void enter_state(wifi_state_t next)
{
    s_state = next;
    s_state_enter_tick = HAL_GetTick();
}

static void backoff_reset(void)
{
    s_retry_delay_ms = RETRY_BACKOFF_INITIAL_MS;
}

static void backoff_grow(void)
{
    s_retry_delay_ms *= 2;
    if (s_retry_delay_ms > RETRY_BACKOFF_MAX_MS) {
        s_retry_delay_ms = RETRY_BACKOFF_MAX_MS;
    }
}

/* AP 접속 실패/서버 접속 실패 시 retry_target 상태로 지수 백오프 후 재시도 */
static void schedule_retry(wifi_state_t retry_target)
{
    s_retry_target = retry_target;
    backoff_grow();
    enter_state(WIFI_STATE_RETRY_WAIT);
}

/* AT 명령 문자열에 들어가는 SSID/PASSWORD 등은 " , \ 를 이스케이프해야 한다 */
static void escape_at_string(const char *in, char *out, uint16_t out_size)
{
    uint16_t w = 0;
    for (uint16_t i = 0; in[i] != '\0' && w < (out_size - 2); i++) {
        char c = in[i];
        if (c == '"' || c == ',' || c == '\\') {
            out[w++] = '\\';
        }
        out[w++] = c;
    }
    out[w] = '\0';
}

void WiFi_Manager_Init(UART_HandleTypeDef *huart,
                        const wifi_ap_config_t *ap_cfg,
                        const wifi_ip_config_t *ip_cfg,
                        const wifi_server_config_t *server_cfg)
{
    ESP32_AT_Init(huart);

    s_ap_cfg = *ap_cfg;
    s_ip_cfg = *ip_cfg;

    if (server_cfg != NULL) {
        s_server_cfg = *server_cfg;
        s_server_enabled = true;
    } else {
        s_server_enabled = false;
    }

    backoff_reset();
    enter_state(WIFI_STATE_INIT);
}

wifi_state_t WiFi_Manager_GetState(void)
{
    return s_state;
}

bool WiFi_Manager_IsConnected(void)
{
    return s_state == WIFI_STATE_CONNECTED;
}

bool WiFi_Manager_Send(const uint8_t *data, uint16_t len)
{
    char cmd[32];

    if (s_state != WIFI_STATE_CONNECTED) {
        return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)len);
    if (ESP32_AT_SendCommand(cmd, ">", AT_TIMEOUT_SHORT_MS) != AT_OK) {
        return false;
    }

    ESP32_AT_SendRaw(data, len);

    char line[ESP32_AT_LINE_MAX];
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < AT_TIMEOUT_SHORT_MS) {
        if (ESP32_AT_PopLine(line, sizeof(line))) {
            if (strstr(line, "SEND OK") != NULL) {
                return true;
            }
            if (strstr(line, "ERROR") != NULL || strstr(line, "SEND FAIL") != NULL) {
                return false;
            }
        }
    }
    return false;
}

/* AT+CIPSTATUS 응답의 "STATUS:<n>" 라인에서 상태 코드를 읽는다.
 *   2 = station 접속됨, TCP/UDP 연결 없음
 *   3 = TCP/UDP 연결 됨
 *   4 = TCP/UDP 연결 끊김 (AP는 살아있음)
 *   5 = WiFi(AP) 연결 안됨
 *  그 외 / 명령 자체 실패 = -1 */
static int query_cip_status(void)
{
    char status_line[ESP32_AT_LINE_MAX] = { 0 };

    at_status_t r = ESP32_AT_SendCommandEx("AT+CIPSTATUS", "OK", AT_TIMEOUT_SHORT_MS,
                                            "STATUS:", status_line, sizeof(status_line));
    if (r != AT_OK || status_line[0] == '\0') {
        return -1;
    }

    int code = -1;
    if (sscanf(status_line, "STATUS:%d", &code) != 1) {
        return -1;
    }
    return code;
}

/* 대기 중(URC) 라인을 훑어 즉시 재접속이 필요한 이벤트를 감지한다.
 * 감지 즉시 상태를 바꾸면 true를 반환한다(이번 Process() 호출은 여기서 종료). */
static bool check_async_disconnect_events(void)
{
    char line[ESP32_AT_LINE_MAX];
    bool ap_lost = false;
    bool link_lost = false;

    while (ESP32_AT_PopLine(line, sizeof(line))) {
        if (strstr(line, "WIFI DISCONNECT") != NULL) {
            ap_lost = true;
        } else if (strstr(line, "+CIPCLOSED") != NULL || strstr(line, "CLOSED") != NULL) {
            link_lost = true;
        }
    }

    if (ap_lost) {
        backoff_reset();
        schedule_retry(WIFI_STATE_CONNECT_AP);
        return true;
    }
    if (link_lost && s_server_enabled) {
        backoff_reset();
        schedule_retry(WIFI_STATE_CONNECT_SERVER);
        return true;
    }
    return false;
}

void WiFi_Manager_Process(void)
{
    char cmd[160];
    char esc[80];
    at_status_t r;

    /* CONNECTED 상태가 아닐 때도 URC는 계속 비워준다(라인 큐 오버플로 방지) */
    if (s_state != WIFI_STATE_CONNECTED) {
        char discard[ESP32_AT_LINE_MAX];
        while (ESP32_AT_PopLine(discard, sizeof(discard))) {
            /* 아직 감시 대상이 아니므로 버린다 */
        }
    }

    switch (s_state) {

    case WIFI_STATE_INIT:
        enter_state(WIFI_STATE_MODULE_CHECK);
        break;

    case WIFI_STATE_MODULE_CHECK:
        r = ESP32_AT_SendCommand("AT", "OK", AT_TIMEOUT_SHORT_MS);
        if (r == AT_OK) {
            /* 에코 끄기: 실패해도 치명적이지 않으므로 결과를 무시한다 */
            ESP32_AT_SendCommand("ATE0", "OK", AT_TIMEOUT_SHORT_MS);
            enter_state(WIFI_STATE_SET_STATION_MODE);
        } else {
            schedule_retry(WIFI_STATE_MODULE_CHECK);
        }
        break;

    case WIFI_STATE_SET_STATION_MODE:
        r = ESP32_AT_SendCommand("AT+CWMODE=1", "OK", AT_TIMEOUT_SHORT_MS);
        if (r == AT_OK) {
            enter_state(WIFI_STATE_SET_DHCP);
        } else {
            schedule_retry(WIFI_STATE_SET_STATION_MODE);
        }
        break;

    case WIFI_STATE_SET_DHCP:
        /* AT+CWDHCP=<mode>,<enable> : mode 1 = station 인터페이스 */
        snprintf(cmd, sizeof(cmd), "AT+CWDHCP=1,%d", s_ip_cfg.dhcp_enable ? 1 : 0);
        r = ESP32_AT_SendCommand(cmd, "OK", AT_TIMEOUT_SHORT_MS);
        if (r != AT_OK) {
            schedule_retry(WIFI_STATE_SET_DHCP);
            break;
        }
        if (s_ip_cfg.dhcp_enable) {
            enter_state(WIFI_STATE_CONNECT_AP);
        } else {
            enter_state(WIFI_STATE_SET_STATIC_IP);
        }
        break;

    case WIFI_STATE_SET_STATIC_IP:
        snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"",
                 s_ip_cfg.ip, s_ip_cfg.gateway, s_ip_cfg.netmask);
        r = ESP32_AT_SendCommand(cmd, "OK", AT_TIMEOUT_SHORT_MS);
        if (r == AT_OK) {
            enter_state(WIFI_STATE_CONNECT_AP);
        } else {
            schedule_retry(WIFI_STATE_SET_STATIC_IP);
        }
        break;

    case WIFI_STATE_CONNECT_AP: {
        char esc_pw[80];
        escape_at_string(s_ap_cfg.ssid, esc, sizeof(esc));
        escape_at_string(s_ap_cfg.password, esc_pw, sizeof(esc_pw));
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", esc, esc_pw);
        /* AT+CWJAP은 접속에 시간이 걸리므로 긴 타임아웃을 사용한다.
         * 이 호출 동안은 블로킹되지만, AP 접속 단계에서만 발생한다. */
        r = ESP32_AT_SendCommand(cmd, "WIFI GOT IP", AT_TIMEOUT_AP_JOIN_MS);
        if (r == AT_OK) {
            backoff_reset();
            enter_state(WIFI_STATE_SET_SINGLE_CONN);
        } else {
            schedule_retry(WIFI_STATE_CONNECT_AP);
        }
        break;
    }

    case WIFI_STATE_SET_SINGLE_CONN:
        r = ESP32_AT_SendCommand("AT+CIPMUX=0", "OK", AT_TIMEOUT_SHORT_MS);
        if (r != AT_OK) {
            schedule_retry(WIFI_STATE_SET_SINGLE_CONN);
            break;
        }
        if (s_server_enabled) {
            enter_state(WIFI_STATE_CONNECT_SERVER);
        } else {
            enter_state(WIFI_STATE_CONNECTED);
            s_last_health_check_tick = HAL_GetTick();
        }
        break;

    case WIFI_STATE_CONNECT_SERVER: {
        char note[40] = { 0 };
        snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                 s_server_cfg.ip, (unsigned)s_server_cfg.port);
        r = ESP32_AT_SendCommandEx(cmd, "CONNECT", AT_TIMEOUT_SERVER_MS,
                                    "ALREADY", note, sizeof(note));
        /* "이미 연결됨(ALREADY CONNECTED)" 에러는 실질적으로 성공과 같다 */
        if (r == AT_OK || note[0] != '\0') {
            backoff_reset();
            enter_state(WIFI_STATE_CONNECTED);
            s_last_health_check_tick = HAL_GetTick();
        } else {
            schedule_retry(WIFI_STATE_CONNECT_SERVER);
        }
        break;
    }

    case WIFI_STATE_CONNECTED:
        if (check_async_disconnect_events()) {
            break;
        }
        if ((HAL_GetTick() - s_last_health_check_tick) >= HEALTH_CHECK_INTERVAL_MS) {
            s_last_health_check_tick = HAL_GetTick();
            int status = query_cip_status();
            if (status == 5) {
                backoff_reset();
                schedule_retry(WIFI_STATE_CONNECT_AP);
            } else if (status == 4 || (status == 2 && s_server_enabled)) {
                backoff_reset();
                schedule_retry(WIFI_STATE_CONNECT_SERVER);
            } else if (status < 0) {
                /* 모듈이 응답하지 않음: AP부터 다시 확인 */
                backoff_reset();
                schedule_retry(WIFI_STATE_CONNECT_AP);
            }
            /* status == 3, 혹은 서버 미사용 상태의 2 는 정상이므로 유지 */
        }
        break;

    case WIFI_STATE_RETRY_WAIT:
        if ((HAL_GetTick() - s_state_enter_tick) >= s_retry_delay_ms) {
            enter_state(s_retry_target);
        }
        break;

    default:
        break;
    }
}
