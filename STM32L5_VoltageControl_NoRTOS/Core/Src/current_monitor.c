/* =========================================================
 * 전류 모니터링 - Non-RTOS
 * =========================================================*/
#include "current_monitor.h"
#include "uart_protocol.h"
#include <string.h>

void CurrMon_Init(CurrentMonitor_t *hmon)
{
    memset(hmon, 0, sizeof(CurrentMonitor_t));
    hmon->enabled = true;
}

int32_t CurrMon_MvToMilliamps(int32_t adc_mv)
{
    if (adc_mv < 0) adc_mv = 0;
    /* I(mA) = Vadc(mV) × 1000 / (Rshunt(mΩ) × Gain) */
    return (adc_mv * 1000) / (CURRENT_SHUNT_MOHM * CURRENT_AMP_GAIN_X10 / 10);
}

static int32_t _moving_avg(CurrentCh_t *ch, int32_t val)
{
    ch->avg_buf[ch->avg_idx] = val;
    ch->avg_idx = (ch->avg_idx + 1) % CURRENT_AVG_SAMPLES;
    int64_t sum = 0;
    for (int i = 0; i < CURRENT_AVG_SAMPLES; i++) sum += ch->avg_buf[i];
    return (int32_t)(sum >> CURRENT_AVG_SHIFT);
}

void CurrMon_Update(CurrentMonitor_t *hmon,
                    MCP3465R_Handle_t *hadc,
                    VoltageCtrl_t *hctrl)
{
    if (!hmon->enabled) return;

    static const uint8_t curr_adc_ch[VOLTAGE_NUM_CH] = {4, 5, 6, 7};

    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        CurrentCh_t *ch   = &hmon->ch[i];
        uint8_t      ach  = curr_adc_ch[i];

        if (!hadc->results[ach].is_valid) continue;

        int32_t mv = hadc->results[ach].voltage_mv;
        ch->current_ma     = CurrMon_MvToMilliamps(mv);
        ch->avg_current_ma = _moving_avg(ch, ch->current_ma);
        hctrl->ch[i].current_ma = ch->avg_current_ma;

        /* 폴트 자동 해제 감시 */
        if (ch->fault != FAULT_NONE) {
            if (ch->avg_current_ma < FAULT_SHORT_MA / 2) {
                if (++ch->fault_clear >= FAULT_CLEAR_COUNT) {
                    char msg[48];
                    uint8_t len = 0;
                    msg[len++]='C'; msg[len++]='H';
                    msg[len++]='0'+i+1;
                    const char *s=" FAULT_CLEARED"; while(*s) msg[len++]=*s++;
                    msg[len]='\0';
                    Proto_SendEvent("INFO", msg);
                    ch->fault       = FAULT_NONE;
                    ch->fault_clear = 0;
                    ch->fault_confirm = 0;
                }
            } else {
                ch->fault_clear = 0;
            }
            continue;
        }

        /* 단락 감지 */
        if (ch->avg_current_ma >= FAULT_SHORT_MA) {
            if (++ch->fault_confirm >= FAULT_CONFIRM_COUNT) {
                /* 즉시 출력 차단 */
                VoltageCtrl_SetPwm(i, 0);
                hctrl->ch[i].pwm_duty  = 0;
                hctrl->ch[i].integral  = 0.0f;
                hctrl->ch[i].fault_flags |= CH_STATUS_SHORT;
                ch->fault         = FAULT_SHORT;
                ch->fault_confirm = 0;
                hmon->fault_events++;

                char msg[48];
                /* sprintf 없이 간단히 구성 */
                const char *prefix = "CH? SHORT I=";
                int pi = 0;
                while (prefix[pi]) msg[pi++] = prefix[pi];
                msg[2] = '0' + i + 1; /* CH 번호 */
                /* 전류값 문자열 추가 */
                int32_t mA = ch->avg_current_ma;
                char num[8]; int ni = 0;
                if (mA == 0) { num[ni++]='0'; }
                else { int32_t tmp=mA; while(tmp>0){num[ni++]='0'+(tmp%10);tmp/=10;} }
                for (int k = 0; k < ni/2; k++) {
                    char t=num[k]; num[k]=num[ni-1-k]; num[ni-1-k]=t;
                }
                for (int k = 0; k < ni; k++) msg[pi++] = num[k];
                msg[pi++]='m'; msg[pi++]='A'; msg[pi]='\0';

                Proto_SendEvent("FAULT", msg);
            }
        } else {
            if (ch->fault_confirm > 0) ch->fault_confirm--;
        }
    }
}

const char *CurrMon_FaultStr(FaultType_t f)
{
    switch (f) {
        case FAULT_NONE:  return "OK";
        case FAULT_SHORT: return "SHORT";
        case FAULT_OPEN:  return "OPEN";
        default:          return "UNKNOWN";
    }
}
