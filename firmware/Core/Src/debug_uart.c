#include "debug_uart.h"
#include "voltage_control.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static UART_HandleTypeDef *s_huart_dbg = NULL;
static char s_dbg_buf[DEBUG_TX_BUF_SIZE];
static uint8_t s_debug_enabled = 1;

/* External reference for channel status printing */
extern VoltCtrl_Handle_t g_vctrl;

void Debug_Init(UART_HandleTypeDef *huart_debug)
{
    s_huart_dbg = huart_debug;
    s_debug_enabled = 1;
}

void Debug_Print(const char *tag, const char *fmt, ...)
{
    if (!s_debug_enabled || s_huart_dbg == NULL) return;

    /* Build prefix: [DBG][TAG] */
    int offset = snprintf(s_dbg_buf, DEBUG_TX_BUF_SIZE, "[DBG][%-5s] ", tag);
    if (offset < 0 || offset >= DEBUG_TX_BUF_SIZE) return;

    /* Append user message */
    va_list args;
    va_start(args, fmt);
    int msg_len = vsnprintf(s_dbg_buf + offset, DEBUG_TX_BUF_SIZE - offset, fmt, args);
    va_end(args);

    if (msg_len < 0) return;

    int total = offset + msg_len;
    if (total > DEBUG_TX_BUF_SIZE - 3) {
        total = DEBUG_TX_BUF_SIZE - 3;
    }

    /* Append \r\n */
    s_dbg_buf[total++] = '\r';
    s_dbg_buf[total++] = '\n';

    HAL_UART_Transmit(s_huart_dbg, (uint8_t *)s_dbg_buf, (uint16_t)total, 50);
}

void Debug_Raw(const char *fmt, ...)
{
    if (!s_debug_enabled || s_huart_dbg == NULL) return;

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(s_dbg_buf, DEBUG_TX_BUF_SIZE, fmt, args);
    va_end(args);

    if (len > 0) {
        if (len > DEBUG_TX_BUF_SIZE) len = DEBUG_TX_BUF_SIZE;
        HAL_UART_Transmit(s_huart_dbg, (uint8_t *)s_dbg_buf, (uint16_t)len, 50);
    }
}

void Debug_PrintChannelStatus(void)
{
    if (!s_debug_enabled || s_huart_dbg == NULL) return;

    static const char *state_names[] = {"OK", "SHORT", "OPEN", "OFF", "CAL"};

    Debug_Raw("---- Channel Status ----\r\n");
    Debug_Raw("CH  TGT(V)  ACT(V)  I(mA)   DAC   STATE\r\n");

    for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
        ChannelData_t data;
        VoltCtrl_GetChannelData(&g_vctrl, ch, &data);

        const char *st = (data.state <= CH_STATE_CALIBRATING) ? state_names[data.state] : "???";

        Debug_Raw("%d   %5.3f   %5.3f   %6.2f  %5u  %s\r\n",
                  ch, data.target_voltage, data.actual_voltage,
                  data.current_mA, data.dac_code, st);
    }
    Debug_Raw("------------------------\r\n");
}

uint8_t Debug_IsEnabled(void)
{
    return s_debug_enabled;
}

void Debug_SetEnabled(uint8_t en)
{
    s_debug_enabled = en ? 1 : 0;
}
