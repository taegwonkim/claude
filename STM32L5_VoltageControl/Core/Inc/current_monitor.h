#ifndef CURRENT_MONITOR_H
#define CURRENT_MONITOR_H

#include "mcp3465r.h"
#include "voltage_control.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * 전류 모니터링 모듈
 * - MCP3465R CH4~CH7로 전류 측정
 * - 단락(Short) 및 단선(Open) 감지
 * - 폴트 발생 시 해당 채널 출력 차단
 *
 * 하드웨어:
 *   전류감지 저항: 0.1Ω (각 채널 직렬)
 *   앰프: INA199A1 (게인 20V/V)
 *   최대 측정: 3300mV / (20 × 0.1Ω) = 1650mA
 *   분해능: 3300mV / 8388608 / (20 × 0.1Ω) ≈ 0.197mA/LSB
 * =========================================================*/

/* ----- 전류 측정 설정 ----- */
#define CURRENT_SHUNT_MOHM     100      /* 전류감지 저항 (mΩ = 0.1Ω) */
#define CURRENT_AMP_GAIN_X10   200      /* 앰프 게인 × 10 (20.0V/V) */
#define CURRENT_VREF_MV        3300     /* ADC 기준 전압 */

/* ----- 폴트 임계값 ----- */
#define FAULT_SHORT_MA         900      /* 단락 판정 전류 임계값 (mA) */
#define FAULT_OPEN_MA_MAX      5        /* 단선 판정 전류 상한 (mA) */
                                        /* (부하 연결 시 최소 이 이상 흘러야 함) */
#define FAULT_OPEN_ENABLED     false    /* 단선 감지 활성화 여부 */
                                        /* (부하 없음이 정상인 경우 false) */
#define FAULT_CONFIRM_COUNT    3        /* 폴트 확인 반복 횟수 */
#define FAULT_CLEAR_COUNT      10       /* 폴트 자동해제 카운터 */

/* ----- 전류 이동 평균 ----- */
#define CURRENT_AVG_SAMPLES    8        /* 이동 평균 샘플 수 (2의 거듭제곱) */
#define CURRENT_AVG_SHIFT      3        /* log2(CURRENT_AVG_SAMPLES) */

/* ----- 전류 측정 채널 매핑 ----- */
/* MCP3465R 채널 → 전압 출력 채널 매핑 */
#define CURR_CH_MAP_0          MCP3465R_MUX_CH4   /* 전압CH1 전류 */
#define CURR_CH_MAP_1          MCP3465R_MUX_CH5   /* 전압CH2 전류 */
#define CURR_CH_MAP_2          MCP3465R_MUX_CH6   /* 전압CH3 전류 */
#define CURR_CH_MAP_3          MCP3465R_MUX_CH7   /* 전압CH4 전류 */

/* ----- 폴트 타입 ----- */
typedef enum {
    FAULT_NONE  = 0,
    FAULT_SHORT = 1,    /* 단락: 전류 > FAULT_SHORT_MA */
    FAULT_OPEN  = 2,    /* 단선: 전류 < FAULT_OPEN_MA_MAX (부하 연결 시) */
} FaultType_t;

/* ----- 채널 전류 데이터 ----- */
typedef struct {
    int32_t  raw_mv;              /* ADC 원시 전압 (mV) */
    int32_t  current_ma;          /* 계산된 전류 (mA) */
    int32_t  avg_buf[CURRENT_AVG_SAMPLES]; /* 이동 평균 버퍼 */
    uint8_t  avg_idx;             /* 버퍼 인덱스 */
    int32_t  avg_current_ma;      /* 이동 평균 전류 */
    FaultType_t fault;            /* 현재 폴트 타입 */
    uint8_t  fault_confirm;       /* 폴트 확인 카운터 */
    uint8_t  fault_clear;         /* 폴트 해제 카운터 */
    bool     load_connected;      /* 부하 연결 여부 */
} CurrentCh_t;

/* ----- 전류 모니터 핸들 ----- */
typedef struct {
    CurrentCh_t ch[VOLTAGE_NUM_CH];
    bool        enabled;           /* 모니터링 활성화 */
    uint32_t    fault_events;      /* 총 폴트 발생 횟수 */
} CurrentMonitor_t;

/* ----- 함수 선언 ----- */
void       CurrMon_Init(CurrentMonitor_t *hmon);
void       CurrMon_Update(CurrentMonitor_t *hmon,
                          MCP3465R_Handle_t *hadc,
                          VoltageCtrl_t *hctrl);
int32_t    CurrMon_MvToMilliamps(int32_t adc_mv);
FaultType_t CurrMon_CheckFault(CurrentCh_t *ch_data);
void       CurrMon_HandleFault(CurrentMonitor_t *hmon,
                               VoltageCtrl_t *hctrl,
                               uint8_t ch,
                               FaultType_t fault);
void       CurrMon_GetCurrent(CurrentMonitor_t *hmon, uint8_t ch,
                              int32_t *current_ma, FaultType_t *fault);
bool       CurrMon_IsFault(CurrentMonitor_t *hmon, uint8_t ch);
const char* CurrMon_FaultStr(FaultType_t fault);

#endif /* CURRENT_MONITOR_H */
