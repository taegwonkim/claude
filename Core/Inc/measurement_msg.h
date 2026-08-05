/**
 * measurement_msg.h
 *
 * FPGA_Task -> ESP32_Task 큐(g_measQueueId)로 전달되는 측정값 메시지.
 * (PC 미러 출력은 FPGA_Task가 PcComm_BroadcastFrame()으로 직접, 큐와 별개로 즉시 수행한다.)
 */
#ifndef MEASUREMENT_MSG_H
#define MEASUREMENT_MSG_H

#include <stdint.h>
#include <stddef.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t seq;
    uint32_t timestamp_ms;
    uint16_t samples[FPGA_ADC_SAMPLE_COUNT];
} MeasurementMsg_t;

/**
 * @brief "DATA <seq> <timestamp_ms> <sample0> ..." (공백 구분) 라인을 out에 NUL 종단하여 채운다.
 *        ESP32 AT+CIPSEND로 서버에 보내는 TCP payload 전용 (docs/프로토콜_명세.md §2).
 *        PC 시리얼 링크(USART3/USB)에는 사용하지 않는다 - 그쪽은 STX+CSV+CRLF 프레임
 *        (MeasurementMsg_BuildCsvFields) 전용.
 * @return 기록된 문자 수(NUL 제외, snprintf와 동일한 클램프 동작)
 */
int MeasurementMsg_BuildDataLine(char *out, size_t out_size, const MeasurementMsg_t *msg);

/**
 * @brief "DATA,<seq>,<timestamp_ms>,<sample0>,..." (콤마 구분) 필드 문자열을 out에 채운다.
 *        PC 시리얼 링크로 보낼 때 PcComm_BroadcastFrame()에 그대로 넘기면
 *        STX/CR/LF 프레이밍이 씌워진다 (docs/프로토콜_명세.md §1-§2).
 * @return 기록된 문자 수(NUL 제외, snprintf와 동일한 클램프 동작)
 */
int MeasurementMsg_BuildCsvFields(char *out, size_t out_size, const MeasurementMsg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_MSG_H */
