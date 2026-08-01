#include <string.h>
#include "app_pc_uart.h"
#include "app_cfg_protocol.h"
#include "ring_buffer.h"
#include "cmsis_os2.h"

static uint8_t      s_rxStorage[PC_UART_RX_RINGBUF_SIZE];
static RingBuffer_t  s_rxRing;
static uint8_t       s_rxByte;              /* 1-byte IT scratch buffer */

static char          s_line[PC_UART_LINE_MAX_LEN];
static uint16_t      s_lineLen;

static void PcUart_Reply(const char *msg)
{
    HAL_UART_Transmit(PC_UART_HANDLE, (uint8_t *)msg, (uint16_t)strlen(msg), 100U);
}

void App_PcUart_Init(void)
{
    RingBuffer_Init(&s_rxRing, s_rxStorage, PC_UART_RX_RINGBUF_SIZE);
    s_lineLen = 0;

    HAL_UART_Receive_IT(PC_UART_HANDLE, &s_rxByte, 1U);
}

void App_PcUart_UART_RxCpltCallback(void)
{
    RingBuffer_PutByte(&s_rxRing, s_rxByte);
    HAL_UART_Receive_IT(PC_UART_HANDLE, &s_rxByte, 1U);
}

void App_PcUart_Task(void *argument)
{
    (void)argument;
    uint8_t byte;

    for (;;) {
        if (RingBuffer_GetByte(&s_rxRing, &byte)) {
            if (byte == '\n') {
                s_line[s_lineLen] = '\0';
                CfgProtocol_HandleLine(s_line, PcUart_Reply);
                s_lineLen = 0;
            } else if (byte != '\r') {
                if (s_lineLen < (PC_UART_LINE_MAX_LEN - 1U)) {
                    s_line[s_lineLen++] = (char)byte;
                } else {
                    /* line too long: drop it and resync on next '\n' */
                    s_lineLen = 0;
                }
            }
        } else {
            osDelay(5);
        }
    }
}
