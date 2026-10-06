#include "fpga_link.h"
#include "uart_line_rx.h"
#include "app_config.h"
#include "app_main.h" /* App_PushMeasurement */
#include "measurement_msg.h"
#include "pc_comm.h"
#include "esp32_at.h" /* Esp32_GetCachedNetInfo (PC 프레임의 DC IP/MAC) */
#include "main.h" /* FROM_FPGA_Pin (CubeMX 핀 라벨 "FROM_FPGA", PH1) */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static uint8_t s_dmaBuf[APP_UART_RB_SIZE];
static uint8_t s_rbStorage[APP_UART_RB_SIZE * 2U];
static UartLineRx_t s_rx;
static volatile bool s_triggerPending = false;
static uint32_t s_dropCount = 0U;

void FpgaLink_Init(void)
{
    UartLineRx_Init(&s_rx, &huart2, s_dmaBuf, sizeof(s_dmaBuf), s_rbStorage, sizeof(s_rbStorage));
    UartLineRx_Start(&s_rx);

    /* 부팅 후 1회, FPGA에 측정 개시 명령 전송 (USART2 TX, PA2) */
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)FPGA_START_CMD, (uint16_t)(sizeof(FPGA_START_CMD) - 1U),
                             FPGA_START_CMD_TX_TIMEOUT_MS);
}

void FpgaLink_OnExti(uint16_t gpio_pin)
{
    if (gpio_pin == FROM_FPGA_Pin) {
        s_triggerPending = true;
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

void FpgaLink_Poll(void)
{
    if (!s_triggerPending) {
        return;
    }
    s_triggerPending = false;

    char line[APP_PC_LINE_MAX];
    MeasurementMsg_t msg;
    uint32_t start = HAL_GetTick();
    bool got_line = false;

    /* RTOS가 없으므로 이 최대 200ms 대기 동안 App_Run()의 다른 폴링(ESP32/PC통신)은
     * 진행되지 않는다 (firmware-no-rtos/README.md 참고). */
    while ((HAL_GetTick() - start) < FPGA_LINE_TIMEOUT_MS) {
        if (RingBuffer_PopLine(&s_rx.rb, line, sizeof(line), '\n')) {
            got_line = true;
            break;
        }
    }

    if (!got_line) {
        s_dropCount++;
        return;
    }

    TrimTrailingCrLf(line);
    if (!ParseAdcLine(line, &msg)) {
        s_dropCount++;
        return;
    }

    char dc_ip[16];
    char dc_mac[18];
    Esp32_GetCachedNetInfo(dc_ip, sizeof(dc_ip), dc_mac, sizeof(dc_mac));

    char out[APP_PC_LINE_MAX];
    MeasurementMsg_BuildPcCsvFields(out, sizeof(out), &msg, dc_ip, dc_mac);
    PcComm_BroadcastFrame(out); /* PC(USART3+USB) 즉시 미러, STX+CRLF 프레이밍은 내부에서 처리 */

    (void)App_PushMeasurement(&msg); /* Esp32_Poll -> 서버 전송, 큐 가득 차면 드롭 */
}
