/**
 * measurement_msg.h
 *
 * FpgaLink_Poll() -> Esp32_Poll() 큐(app_main.c의 App_PushMeasurement())로 전달되는 측정값 메시지.
 * (PC 미러 출력은 FpgaLink_Poll()이 PcComm_BroadcastFrame()으로 직접, 큐와 별개로 즉시 수행한다.)
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
 *        PC 시리얼 링크(USART3/USB)에는 사용하지 않는다 - 그쪽은 MeasurementMsg_BuildPcCsvFields()
 *        전용 포맷을 사용한다.
 * @return 기록된 문자 수(NUL 제외, snprintf와 동일한 클램프 동작)
 */
int MeasurementMsg_BuildDataLine(char *out, size_t out_size, const MeasurementMsg_t *msg);

/**
 * @brief "<dc_ip>,<dc_mac>,<sample0>,...,<sampleN-1>" (콤마 구분) 필드 문자열을 out에 채운다.
 *        PC 시리얼 링크 전용 포맷(docs/프로토콜_명세.md §1-§2, "DATA" 태그 없이 DC IP/MAC으로
 *        시작). PcComm_BroadcastFrame()에 그대로 넘기면 STX/CR/LF 프레이밍이 씌워진다.
 *        dc_ip/dc_mac은 Esp32_GetCachedNetInfo()로 얻은 문자열을 그대로 전달하면 된다.
 * @return 기록된 문자 수(NUL 제외, snprintf와 동일한 클램프 동작)
 */
int MeasurementMsg_BuildPcCsvFields(char *out, size_t out_size, const MeasurementMsg_t *msg,
                                     const char *dc_ip, const char *dc_mac);

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_MSG_H */
