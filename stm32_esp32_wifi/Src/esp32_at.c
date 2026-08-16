#include <string.h>
#include "esp32_at.h"

static UART_HandleTypeDef *s_huart;
static uint8_t s_rx_byte;

/* 조립 중인 한 줄 */
static char     s_line_buf[ESP32_AT_LINE_MAX];
static uint16_t s_line_len;

/* 완성된 라인을 담는 단일 생산자(ISR) / 단일 소비자(메인루프) 원형 큐 */
static char     s_line_queue[ESP32_AT_LINE_QUEUE][ESP32_AT_LINE_MAX];
static volatile uint8_t s_q_head;
static volatile uint8_t s_q_tail;

static void line_queue_push(const char *line)
{
    uint8_t next = (uint8_t)((s_q_head + 1) % ESP32_AT_LINE_QUEUE);
    if (next == s_q_tail) {
        /* 큐가 가득 찼으면 가장 오래된 라인을 버린다 */
        s_q_tail = (uint8_t)((s_q_tail + 1) % ESP32_AT_LINE_QUEUE);
    }
    strncpy(s_line_queue[s_q_head], line, ESP32_AT_LINE_MAX - 1);
    s_line_queue[s_q_head][ESP32_AT_LINE_MAX - 1] = '\0';
    s_q_head = next;
}

bool ESP32_AT_PopLine(char *out, uint16_t out_size)
{
    if (s_q_tail == s_q_head) {
        return false;
    }
    strncpy(out, s_line_queue[s_q_tail], out_size - 1);
    out[out_size - 1] = '\0';
    s_q_tail = (uint8_t)((s_q_tail + 1) % ESP32_AT_LINE_QUEUE);
    return true;
}

void ESP32_AT_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_line_len = 0;
    s_q_head = 0;
    s_q_tail = 0;
    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

void ESP32_AT_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != s_huart->Instance) {
        return;
    }

    char c = (char)s_rx_byte;

    /* "'>'"는 CIPSEND의 데이터 입력 프롬프트로, 개행 없이 단독으로 온다 */
    if (c == '>' && s_line_len == 0) {
        line_queue_push(">");
    } else if (c == '\n') {
        if (s_line_len > 0) {
            s_line_buf[s_line_len] = '\0';
            line_queue_push(s_line_buf);
            s_line_len = 0;
        }
    } else if (c != '\r') {
        if (s_line_len < (ESP32_AT_LINE_MAX - 1)) {
            s_line_buf[s_line_len++] = c;
        } else {
            /* 라인이 비정상적으로 길면 버리고 다시 시작 */
            s_line_len = 0;
        }
    }

    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

static void uart_send_line(const char *cmd)
{
    HAL_UART_Transmit(s_huart, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000);
    HAL_UART_Transmit(s_huart, (uint8_t *)"\r\n", 2, 100);
}

void ESP32_AT_SendRaw(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(s_huart, (uint8_t *)data, len, 2000);
}

at_status_t ESP32_AT_SendCommand(const char *cmd, const char *expected, uint32_t timeout_ms)
{
    return ESP32_AT_SendCommandEx(cmd, expected, timeout_ms, NULL, NULL, 0);
}

at_status_t ESP32_AT_SendCommandEx(const char *cmd, const char *expected, uint32_t timeout_ms,
                                    const char *capture_substr, char *capture_out, uint16_t capture_out_size)
{
    char line[ESP32_AT_LINE_MAX];

    /* 이전 명령의 잔여 응답이 섞이지 않도록 큐를 비운다 */
    while (ESP32_AT_PopLine(line, sizeof(line))) {
        /* discard */
    }

    if (expected == NULL) {
        expected = "OK";
    }

    uart_send_line(cmd);

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        if (ESP32_AT_PopLine(line, sizeof(line))) {
            if (capture_substr != NULL && strstr(line, capture_substr) != NULL) {
                strncpy(capture_out, line, capture_out_size - 1);
                capture_out[capture_out_size - 1] = '\0';
            }
            if (strstr(line, expected) != NULL) {
                return AT_OK;
            }
            if (strstr(line, "ERROR") != NULL || strstr(line, "FAIL") != NULL) {
                return AT_ERROR;
            }
            if (strstr(line, "busy") != NULL) {
                return AT_BUSY;
            }
        }
    }
    return AT_TIMEOUT;
}
