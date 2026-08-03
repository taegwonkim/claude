/**
 * measurement_msg.h
 *
 * FPGA_Task -> ESP32_Task 큐(g_measQueueId)로 전달되는 측정값 메시지.
 * (PC 미러 출력은 FPGA_Task가 PcComm_BroadcastLine()으로 직접, 큐와 별개로 즉시 수행한다.)
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
 * @brief "DATA <seq> <timestamp_ms> <sample0> ..." 라인을 out에 NUL 종단하여 채운다
 *        (docs/프로토콜_명세.md §2). PC 미러 출력과 서버 전송(ESP32 AT+CIPSEND payload)에 공용으로 사용.
 * @return 기록된 문자 수(NUL 제외, snprintf와 동일한 클램프 동작)
 */
int MeasurementMsg_BuildDataLine(char *out, size_t out_size, const MeasurementMsg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_MSG_H */
