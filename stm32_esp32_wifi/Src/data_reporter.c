#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "data_reporter.h"
#include "adc_uart.h"
#include "wifi_manager.h"

#define REPORT_LINE_MAX     80
#define PC_TX_TIMEOUT_MS    50  /* 짧은 고정 길이 라인이므로 충분히 여유있는 값 */

static UART_HandleTypeDef *s_pc_huart;

/* 타이머 인터럽트(생산자)와 메인 루프(소비자) 사이에서 공유되는 버퍼.
 * DataReporter_Process()가 짧은 임계구역으로 복사해 가므로, 전송 도중
 * 다음 틱이 이 버퍼를 덮어써도 이미 복사된 로컬 사본에는 영향이 없다. */
static char              s_pending_payload[REPORT_LINE_MAX];
static volatile uint16_t s_pending_len;
static volatile bool     s_server_send_pending;

void DataReporter_Init(UART_HandleTypeDef *pc_huart)
{
    s_pc_huart = pc_huart;
    s_pending_len = 0;
    s_server_send_pending = false;
}

void DataReporter_TimerTick(void)
{
    char adc_line[ADC_UART_LINE_MAX];
    uint32_t adc_tick = 0;
    char report[REPORT_LINE_MAX];
    int len;

    if (ADC_UART_GetLatest(adc_line, sizeof(adc_line), &adc_tick)) {
        len = snprintf(report, sizeof(report), "ADC,%lu,%s\r\n",
                        (unsigned long)adc_tick, adc_line);
    } else {
        len = snprintf(report, sizeof(report), "ADC,NO_DATA\r\n");
    }
    if (len < 0) {
        return;
    }
    if ((uint16_t)len >= sizeof(report)) {
        len = sizeof(report) - 1;
    }

    /* PC 전송: WiFi 상태와 무관하게, 짧게 블로킹하는 전송으로 매초
     * 확실히 나간다. (인터럽트 컨텍스트에서 수십 ms 이내로 끝나는
     * 블로킹 전송은 허용 가능한 절충이다 - AT 명령 응답 대기처럼
     * 초 단위로 걸리는 블로킹과는 다르다.) */
    HAL_UART_Transmit(s_pc_huart, (uint8_t *)report, (uint16_t)len, PC_TX_TIMEOUT_MS);

    /* 서버 전송은 메인 루프가 처리하도록 넘긴다 */
    memcpy(s_pending_payload, report, (uint16_t)len);
    s_pending_len = (uint16_t)len;
    s_server_send_pending = true;
}

void DataReporter_Process(void)
{
    if (!s_server_send_pending) {
        return;
    }

    char payload[REPORT_LINE_MAX];
    uint16_t len;

    __disable_irq();
    len = s_pending_len;
    memcpy(payload, s_pending_payload, len);
    s_server_send_pending = false;
    __enable_irq();

    if (WiFi_Manager_IsConnected()) {
        WiFi_Manager_Send((uint8_t *)payload, len);
    }
    /* 연결되어 있지 않으면 이번 틱의 서버 전송은 건너뛴다.
     * ADC 수신과 PC 전송은 이미 완료된 뒤이므로 영향이 없다. */
}
