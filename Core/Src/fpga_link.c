#include "fpga_link.h"
#include "uart_line_rx.h"
#include "app_config.h"
#include "app_freertos.h" /* g_measQueueId */
#include "measurement_msg.h"
#include "pc_comm.h"
#include "main.h" /* FROM_FPGA_Pin (CubeMX 핀 라벨 "FROM_FPGA", PH1) */
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static uint8_t s_dmaBuf[APP_UART_RB_SIZE];
static uint8_t s_rbStorage[APP_UART_RB_SIZE * 2U];
static UartLineRx_t s_rx;
static osSemaphoreId_t s_trigSem;
static uint32_t s_dropCount = 0U;

void FpgaLink_Init(void)
{
    UartLineRx_Init(&s_rx, &huart2, s_dmaBuf, sizeof(s_dmaBuf), s_rbStorage, sizeof(s_rbStorage));
    UartLineRx_Start(&s_rx);

    s_trigSem = osSemaphoreNew(1U, 0U, NULL);
}

void FpgaLink_OnExti(uint16_t gpio_pin)
{
    /* s_trigSem은 FpgaLink_Init()에서 생성됨. GPIO/EXTI NVIC는 main()에서 그보다 먼저
     * 활성화되므로, 극히 이른 타이밍의 트리거에 대비해 NULL 체크를 둔다. */
    if (gpio_pin == FROM_FPGA_Pin && s_trigSem != NULL) {
        osSemaphoreRelease(s_trigSem);
    }
}

void FpgaLink_OnUartRxEvent(UART_HandleTypeDef *huart, uint16_t pos)
{
    if (huart->Instance == huart2.Instance) {
        UartLineRx_HandleEvent(&s_rx, pos);
    }
}

/* line: "ADC <seq> <sample0> [sample1 ...]" (호출 전 개행은 이미 제거됨) */
static bool ParseAdcLine(char *line, MeasurementMsg_t *msg)
{
    char *saveptr = NULL;
    char *tok = strtok_r(line, " ", &saveptr);

    if (tok == NULL || strcmp(tok, "ADC") != 0) {
        return false;
    }

    tok = strtok_r(NULL, " ", &saveptr);
    if (tok == NULL) {
        return false;
    }
    msg->seq = (uint32_t)strtoul(tok, NULL, 10);

    for (uint32_t i = 0; i < FPGA_ADC_SAMPLE_COUNT; i++) {
        tok = strtok_r(NULL, " ", &saveptr);
        if (tok == NULL) {
            return false;
        }
        msg->samples[i] = (uint16_t)strtoul(tok, NULL, 10);
    }

    msg->timestamp_ms = HAL_GetTick();
    return true;
}

static void TrimTrailingCrLf(char *s)
{
    size_t len = strlen(s);
    while (len > 0U && (s[len - 1U] == '\r' || s[len - 1U] == '\n')) {
        s[--len] = '\0';
    }
}

void FpgaLink_Task(void *argument)
{
    (void)argument;
    char line[APP_PC_LINE_MAX];
    MeasurementMsg_t msg;

    for (;;) {
        if (osSemaphoreAcquire(s_trigSem, osWaitForever) != osOK) {
            continue;
        }

        uint32_t start = HAL_GetTick();
        bool got_line = false;

        while ((HAL_GetTick() - start) < FPGA_LINE_TIMEOUT_MS) {
            if (RingBuffer_PopLine(&s_rx.rb, line, sizeof(line), '\n')) {
                got_line = true;
                break;
            }
            osDelay(1);
        }

        if (!got_line) {
            s_dropCount++;
            continue;
        }

        TrimTrailingCrLf(line);
        if (!ParseAdcLine(line, &msg)) {
            s_dropCount++;
            continue;
        }

        char out[APP_PC_LINE_MAX];
        MeasurementMsg_BuildDataLine(out, sizeof(out), &msg);
        PcComm_BroadcastLine(out); /* PC(USART3+USB) 즉시 미러 */

        (void)osMessageQueuePut(g_measQueueId, &msg, 0U, 0U); /* ESP32_Task -> 서버 전송, 큐 가득 차면 드롭 */
    }
}
