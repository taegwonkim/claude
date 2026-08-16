#include <string.h>
#include "adc_uart.h"

static UART_HandleTypeDef *s_huart;
static uint8_t  s_rx_byte;

/* 조립 중인 한 줄 (RX 인터럽트 컨텍스트에서만 접근) */
static char     s_line_buf[ADC_UART_LINE_MAX];
static uint16_t s_line_len;

/* 가장 최근에 완성된 한 줄. 생산자는 RX 인터럽트, 소비자는 메인 루프
 * (또는 다른 인터럽트)이므로 GetLatest()에서 짧게 인터럽트를 막고 복사한다. */
static char              s_latest_line[ADC_UART_LINE_MAX];
static volatile uint32_t s_latest_tick;
static volatile bool     s_has_data;

void ADC_UART_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_line_len = 0;
    s_has_data = false;
    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

void ADC_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != s_huart->Instance) {
        return;
    }

    char c = (char)s_rx_byte;

    if (c == '\n') {
        if (s_line_len > 0) {
            s_line_buf[s_line_len] = '\0';
            memcpy(s_latest_line, s_line_buf, s_line_len + 1);
            s_latest_tick = HAL_GetTick();
            s_has_data = true;
            s_line_len = 0;
        }
    } else if (c != '\r') {
        if (s_line_len < (ADC_UART_LINE_MAX - 1)) {
            s_line_buf[s_line_len++] = c;
        } else {
            /* 비정상적으로 긴 라인은 버리고 다시 시작 */
            s_line_len = 0;
        }
    }

    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

bool ADC_UART_GetLatest(char *out, uint16_t out_size, uint32_t *out_tick)
{
    if (!s_has_data) {
        return false;
    }

    /* s_latest_line은 최대 ADC_UART_LINE_MAX바이트라 복사가 매우 짧다.
     * RX 인터럽트가 이 값을 갱신하는 도중에 읽지 않도록만 잠깐 막는다. */
    __disable_irq();
    strncpy(out, s_latest_line, out_size - 1);
    out[out_size - 1] = '\0';
    if (out_tick != NULL) {
        *out_tick = s_latest_tick;
    }
    __enable_irq();

    return true;
}
