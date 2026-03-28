/* =========================================================
 * 전류 모니터링 구현 (단락/단선 감지)
 * =========================================================*/
#include "current_monitor.h"
#include "uart_protocol.h"
#include <string.h>
#include <stdio.h>

/* =========================================================
 * 초기화
 * =========================================================*/
void CurrMon_Init(CurrentMonitor_t *hmon)
{
    memset(hmon, 0, sizeof(CurrentMonitor_t));
    hmon->enabled = true;

    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        hmon->ch[i].fault         = FAULT_NONE;
        hmon->ch[i].fault_confirm = 0;
        hmon->ch[i].fault_clear   = 0;
        hmon->ch[i].avg_idx       = 0;
    }
}

/* =========================================================
 * ADC 전압 (mV) → 전류 (mA) 변환
 *
 * 공식: I(mA) = V_adc(mV) / (R_shunt(mΩ) × Gain / 1000)
 *             = V_adc(mV) × 1000 / (R_shunt(mΩ) × Gain)
 *
 * 예: V_adc = 200mV, R=100mΩ, Gain=20
 *   I = 200 × 1000 / (100 × 20) = 100mA
 * =========================================================*/
int32_t CurrMon_MvToMilliamps(int32_t adc_mv)
{
    if (adc_mv < 0) adc_mv = 0;

    /* 정수 연산: (mV × 1000) / (mΩ × Gain) */
    int32_t current_ma = (adc_mv * 1000) / (CURRENT_SHUNT_MOHM * CURRENT_AMP_GAIN_X10 / 10);
    return current_ma;
}

/* =========================================================
 * 이동 평균 계산
 * =========================================================*/
static int32_t _moving_average(CurrentCh_t *ch, int32_t new_value)
{
    ch->avg_buf[ch->avg_idx] = new_value;
    ch->avg_idx = (ch->avg_idx + 1) % CURRENT_AVG_SAMPLES;

    int64_t sum = 0;
    for (int i = 0; i < CURRENT_AVG_SAMPLES; i++) {
        sum += ch->avg_buf[i];
    }

    return (int32_t)(sum >> CURRENT_AVG_SHIFT); /* / CURRENT_AVG_SAMPLES */
}

/* =========================================================
 * 폴트 감지
 * =========================================================*/
FaultType_t CurrMon_CheckFault(CurrentCh_t *ch_data)
{
    int32_t I = ch_data->avg_current_ma;

    /* 단락 감지: 전류가 임계값 초과 */
    if (I >= FAULT_SHORT_MA) {
        return FAULT_SHORT;
    }

    /* 단선 감지: 부하 연결 상태에서 전류가 너무 낮음 */
    if (FAULT_OPEN_ENABLED && ch_data->load_connected && I < FAULT_OPEN_MA_MAX) {
        return FAULT_OPEN;
    }

    return FAULT_NONE;
}

/* =========================================================
 * 폴트 처리 (출력 차단 + 이벤트 알림)
 * =========================================================*/
void CurrMon_HandleFault(CurrentMonitor_t *hmon,
                         VoltageCtrl_t *hctrl,
                         uint8_t ch,
                         FaultType_t fault)
{
    if (ch >= VOLTAGE_NUM_CH) return;
    if (fault == FAULT_NONE) return;

    VoltageCh_t *vch = &hctrl->ch[ch];
    char event_msg[64];

    /* 채널 출력 즉시 차단 */
    VoltageCtrl_SetPwm(ch, 0);
    vch->pwm_duty  = 0;
    vch->integral  = 0.0f;

    if (fault == FAULT_SHORT) {
        vch->fault_flags |= CH_STATUS_SHORT;
        snprintf(event_msg, sizeof(event_msg),
                 "CH%d SHORT DETECTED I=%ldmA", ch + 1,
                 (long)hmon->ch[ch].avg_current_ma);
    } else {
        vch->fault_flags |= CH_STATUS_OPEN;
        snprintf(event_msg, sizeof(event_msg),
                 "CH%d OPEN DETECTED I=%ldmA", ch + 1,
                 (long)hmon->ch[ch].avg_current_ma);
    }

    hmon->ch[ch].fault = fault;
    hmon->fault_events++;

    /* PC에 폴트 이벤트 전송 */
    Proto_SendEvent("FAULT", event_msg);
}

/* =========================================================
 * 전류 모니터 업데이트 (주기적 호출)
 * =========================================================*/
void CurrMon_Update(CurrentMonitor_t *hmon,
                    MCP3465R_Handle_t *hadc,
                    VoltageCtrl_t *hctrl)
{
    if (!hmon->enabled) return;

    /* MCP3465R CH4~CH7이 전류 측정 채널 */
    static const uint8_t curr_adc_ch[VOLTAGE_NUM_CH] = {4, 5, 6, 7};

    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        CurrentCh_t *ch = &hmon->ch[i];
        uint8_t adc_ch  = curr_adc_ch[i];

        /* ADC 결과 유효성 확인 */
        if (!hadc->results[adc_ch].is_valid) continue;

        /* mV → mA 변환 */
        ch->raw_mv    = hadc->results[adc_ch].voltage_mv;
        ch->current_ma = CurrMon_MvToMilliamps(ch->raw_mv);

        /* 이동 평균 */
        ch->avg_current_ma = _moving_average(ch, ch->current_ma);

        /* 전압 제어 구조체에 전류값 반영 */
        hctrl->ch[i].current_ma = ch->avg_current_ma;

        /* 이미 폴트 상태이면 자동 복구 카운터만 관리 */
        if (ch->fault != FAULT_NONE) {
            if (ch->avg_current_ma < FAULT_SHORT_MA / 2) {
                ch->fault_clear++;
                if (ch->fault_clear >= FAULT_CLEAR_COUNT) {
                    /* 폴트 자동 해제 조건 충족 → 이벤트만 알림
                     * (실제 출력 재개는 호스트가 RESET 명령 전송 필요) */
                    char msg[48];
                    snprintf(msg, sizeof(msg), "CH%d FAULT_CLEARED", i + 1);
                    Proto_SendEvent("INFO", msg);
                    ch->fault       = FAULT_NONE;
                    ch->fault_clear = 0;
                    ch->fault_confirm = 0;
                    /* 폴트 플래그는 유지 (호스트 RESET 명령 대기) */
                }
            } else {
                ch->fault_clear = 0;
            }
            continue;
        }

        /* 폴트 감지 */
        FaultType_t detected = CurrMon_CheckFault(ch);

        if (detected != FAULT_NONE) {
            ch->fault_confirm++;
            if (ch->fault_confirm >= FAULT_CONFIRM_COUNT) {
                CurrMon_HandleFault(hmon, hctrl, i, detected);
                ch->fault_confirm = 0;
            }
        } else {
            /* 정상: 카운터 감소 (히스테리시스) */
            if (ch->fault_confirm > 0) ch->fault_confirm--;
        }
    }
}

/* =========================================================
 * 전류 및 폴트 상태 조회
 * =========================================================*/
void CurrMon_GetCurrent(CurrentMonitor_t *hmon, uint8_t ch,
                        int32_t *current_ma, FaultType_t *fault)
{
    if (ch >= VOLTAGE_NUM_CH) return;
    if (current_ma) *current_ma = hmon->ch[ch].avg_current_ma;
    if (fault)      *fault      = hmon->ch[ch].fault;
}

bool CurrMon_IsFault(CurrentMonitor_t *hmon, uint8_t ch)
{
    if (ch >= VOLTAGE_NUM_CH) return false;
    return (hmon->ch[ch].fault != FAULT_NONE);
}

const char* CurrMon_FaultStr(FaultType_t fault)
{
    switch (fault) {
        case FAULT_NONE:  return "OK";
        case FAULT_SHORT: return "SHORT";
        case FAULT_OPEN:  return "OPEN";
        default:          return "UNKNOWN";
    }
}
