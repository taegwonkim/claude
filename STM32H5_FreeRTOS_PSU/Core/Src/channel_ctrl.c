/**
 * @file    channel_ctrl.c
 * @brief   4채널 전압 제어 로직 구현
 *
 * 부하 변동 보상 원리:
 *   출력 전압이 설정값 아래로 떨어지면 PID가 DAC 코드를 증가시켜
 *   전압을 복원. 부하가 줄면 전압 상승 → PID가 DAC를 감소.
 *   적분항이 정상상태 오차를 0으로 수렴시킴.
 *
 * 오픈 서킷 처리:
 *   전압이 출력 중인데 전류가 OPEN_CIRCUIT_THRESHOLD_MA 이하로
 *   OPEN_CIRCUIT_TIMEOUT_MS 동안 지속되면:
 *     1. ALERT:ch,OPEN 경보 전송
 *     2. 전압을 안전 수준(10%)으로 감소
 *     3. CH_STATUS_FAULT_OPEN_CIRCUIT 설정
 *
 * 과전류 처리:
 *   전류가 MAX_CURRENT_MA 초과 시:
 *     1. 즉시 DAC 출력 0
 *     2. ALERT:ch,OC 전송
 *     3. OVERCURRENT_LATCH=1이면 수동 복구 필요
 *     4. OVERCURRENT_LATCH=0이면 OVERCURRENT_RECOVER_DELAY 후 자동 복구
 */

#include "channel_ctrl.h"
#include "dac_drv.h"
#include "uart_protocol.h"
#include <string.h>

/* =========================================================
 * 기본 PID 이득 (실험을 통해 튜닝 필요)
 * Kp=2.0, Ki=0.5, Kd=0.0 (PI 동작)
 * dt = CONTROL_PERIOD_MS / 1000.0f
 * ========================================================= */
#define DEFAULT_KP      2.0f
#define DEFAULT_KI      0.5f
#define DEFAULT_KD      0.0f

/* 오픈 서킷 감지 후 출력 전압 감소 비율 (10%) */
#define OPEN_CIRCUIT_SAFE_RATIO     0.1f

/* =========================================================
 * 전역 채널 배열
 * ========================================================= */
Channel_t g_channels[NUM_CHANNELS];

/* =========================================================
 * 내부 헬퍼
 * ========================================================= */
static void ApplyDacOutput(uint8_t ch_idx)
{
    Channel_t *ch = &g_channels[ch_idx];
    DAC_Drv_SetChannel(ch_idx, ch->dac_value);
}

static void SendAlert(uint8_t ch_idx, const char *type)
{
    UartProto_SendAlert(ch_idx, type);
    g_channels[ch_idx].fault_count++;
}

/* =========================================================
 * 공개 함수 구현
 * ========================================================= */

void Channel_Init(void)
{
    float dt = (float)CONTROL_PERIOD_MS / 1000.0f;

    for (uint8_t i = 0U; i < NUM_CHANNELS; i++) {
        Channel_t *ch = &g_channels[i];
        memset(ch, 0, sizeof(Channel_t));

        ch->id          = i + 1U;           /* 채널 번호 1~4 */
        ch->enabled     = false;
        ch->status      = CH_STATUS_DISABLED;
        ch->v_setpoint_mv = 0.0f;
        ch->i_limit_ma  = MAX_CURRENT_MA;
        ch->dac_value   = 0U;

        /* PID 초기화: 출력 범위는 DAC 코드 (0~4095) */
        PID_Init(&ch->pid, DEFAULT_KP, DEFAULT_KI, DEFAULT_KD,
                 0.0f, (float)DAC_MAX_CODE, dt);

        /* DAC 출력 0 */
        DAC_Drv_SetChannel(i, 0U);
    }
}

void Channel_SetVoltage(uint8_t ch_idx, float v_mv)
{
    if (ch_idx >= NUM_CHANNELS) {
        return;
    }

    /* 전압 범위 클리핑 */
    if (v_mv < 0.0f) {
        v_mv = 0.0f;
    }
    if (v_mv > VDAC_MAX_MV) {
        v_mv = VDAC_MAX_MV;
    }

    xSemaphoreTake(xChannelMutex, portMAX_DELAY);
    g_channels[ch_idx].v_setpoint_mv = v_mv;

    /* 오픈 서킷 상태에서 새 설정 시 폴트 클리어 */
    if (g_channels[ch_idx].status == CH_STATUS_FAULT_OPEN_CIRCUIT) {
        Channel_ClearFault(ch_idx);
    }

    /* 피드포워드: 설정값에 해당하는 DAC 코드를 PID 초기값으로 설정 */
    if (g_channels[ch_idx].enabled) {
        uint32_t ff_code = DAC_Drv_VoltageMvToCode(v_mv);
        g_channels[ch_idx].dac_value = ff_code;
        g_channels[ch_idx].pid.integral = (float)ff_code;
        ApplyDacOutput(ch_idx);
    }
    xSemaphoreGive(xChannelMutex);
}

void Channel_Enable(uint8_t ch_idx)
{
    if (ch_idx >= NUM_CHANNELS) {
        return;
    }

    xSemaphoreTake(xChannelMutex, portMAX_DELAY);
    Channel_t *ch = &g_channels[ch_idx];

    if (ch->status == CH_STATUS_FAULT_OVERCURRENT && OVERCURRENT_LATCH) {
        /* 래치된 과전류 폴트는 수동으로 ClearFault 후 Enable 가능 */
        xSemaphoreGive(xChannelMutex);
        return;
    }

    ch->enabled  = true;
    ch->status   = CH_STATUS_ENABLED;

    /* PID 리셋 후 피드포워드로 초기화 */
    PID_Reset(&ch->pid);
    ch->pid.integral = (float)DAC_Drv_VoltageMvToCode(ch->v_setpoint_mv);

    ch->dac_value = DAC_Drv_VoltageMvToCode(ch->v_setpoint_mv);
    ch->oc_detecting    = false;
    ch->oc_alert_sent   = false;

    ApplyDacOutput(ch_idx);
    xSemaphoreGive(xChannelMutex);
}

void Channel_Disable(uint8_t ch_idx)
{
    if (ch_idx >= NUM_CHANNELS) {
        return;
    }

    xSemaphoreTake(xChannelMutex, portMAX_DELAY);
    Channel_t *ch = &g_channels[ch_idx];

    ch->enabled   = false;
    ch->status    = CH_STATUS_DISABLED;
    ch->dac_value = 0U;
    PID_Reset(&ch->pid);
    ApplyDacOutput(ch_idx);
    xSemaphoreGive(xChannelMutex);
}

void Channel_UpdateMeasurements(uint8_t ch_idx, float v_mv, float i_ma)
{
    if (ch_idx >= NUM_CHANNELS) {
        return;
    }

    Channel_t *ch = &g_channels[ch_idx];

    /* 음수 방지 */
    if (v_mv < 0.0f) { v_mv = 0.0f; }
    if (i_ma < 0.0f) { i_ma = 0.0f; }

    xSemaphoreTake(xChannelMutex, portMAX_DELAY);
    ch->v_meas_mv = v_mv;
    ch->i_meas_ma = i_ma;

    /* 이동 평균 (α=0.1 저역통과 필터) */
    ch->v_avg_mv = ch->v_avg_mv * 0.9f + v_mv * 0.1f;
    ch->i_avg_ma = ch->i_avg_ma * 0.9f + i_ma * 0.1f;
    xSemaphoreGive(xChannelMutex);
}

/* =========================================================
 * PID 제어 루프 (Control Task에서 CONTROL_PERIOD_MS마다 호출)
 *
 * 동작 방식:
 *   1. 측정 전압을 PID에 입력
 *   2. PID 출력 = 새 DAC 코드
 *   3. DAC 업데이트
 * ========================================================= */
void Channel_RunControl(uint8_t ch_idx)
{
    if (ch_idx >= NUM_CHANNELS) {
        return;
    }

    Channel_t *ch = &g_channels[ch_idx];

    xSemaphoreTake(xChannelMutex, portMAX_DELAY);

    if (!ch->enabled || ch->status != CH_STATUS_ENABLED) {
        xSemaphoreGive(xChannelMutex);
        return;
    }

    /* PID 연산: setpoint(mV) vs measurement(mV) → DAC 코드 출력 */
    float new_code = PID_Compute(&ch->pid,
                                 ch->v_setpoint_mv,
                                 ch->v_meas_mv);

    ch->dac_value = (uint32_t)new_code;
    ApplyDacOutput(ch_idx);

    xSemaphoreGive(xChannelMutex);
}

/* =========================================================
 * 폴트 감지 (Fault Monitor Task에서 매 ms 호출)
 * ========================================================= */
void Channel_CheckFaults(uint8_t ch_idx)
{
    if (ch_idx >= NUM_CHANNELS) {
        return;
    }

    Channel_t *ch = &g_channels[ch_idx];

    /* 비활성화 채널은 건너뜀 */
    if (!ch->enabled) {
        return;
    }

    uint32_t now_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* ---------------------------------------------------------
     * 1. 과전류 감지
     * --------------------------------------------------------- */
    if (ch->i_meas_ma > ch->i_limit_ma) {
        /* 즉시 차단 */
        ch->dac_value   = 0U;
        ch->enabled     = false;
        ch->status      = CH_STATUS_FAULT_OVERCURRENT;
        ch->ovc_trip_time_ms = now_ms;
        ApplyDacOutput(ch_idx);
        PID_Reset(&ch->pid);

        /* 경보 전송 */
        SendAlert(ch_idx, "OC");
    }

    /* ---------------------------------------------------------
     * 2. 과전류 자동 복구 (LATCH=0인 경우)
     * --------------------------------------------------------- */
#if OVERCURRENT_LATCH == 0
    if (ch->status == CH_STATUS_FAULT_OVERCURRENT) {
        if ((now_ms - ch->ovc_trip_time_ms) >= OVERCURRENT_RECOVER_DELAY) {
            Channel_ClearFault(ch_idx);
            Channel_Enable(ch_idx);
        }
    }
#endif

    /* ---------------------------------------------------------
     * 3. 오픈 서킷 감지
     *    조건: 설정전압 >= 최소값 AND 전류 < 임계값
     * --------------------------------------------------------- */
    if (ch->status == CH_STATUS_ENABLED &&
        ch->v_setpoint_mv >= OPEN_CIRCUIT_MIN_VOLTAGE_MV &&
        ch->i_meas_ma < OPEN_CIRCUIT_THRESHOLD_MA)
    {
        if (!ch->oc_detecting) {
            /* 처음 감지: 타이머 시작 */
            ch->oc_detecting       = true;
            ch->oc_detect_start_ms = now_ms;
        } else {
            /* 타임아웃 경과 확인 */
            if (!ch->oc_alert_sent &&
                (now_ms - ch->oc_detect_start_ms) >= OPEN_CIRCUIT_TIMEOUT_MS)
            {
                /* 오픈 서킷 폴트 처리 */
                ch->status = CH_STATUS_FAULT_OPEN_CIRCUIT;
                ch->oc_alert_sent = true;

                /* 전압을 안전 수준(설정값의 10%)으로 감소 */
                float safe_mv = ch->v_setpoint_mv * OPEN_CIRCUIT_SAFE_RATIO;
                uint32_t safe_code = DAC_Drv_VoltageMvToCode(safe_mv);
                ch->dac_value = safe_code;
                ApplyDacOutput(ch_idx);

                /* 경보 전송 */
                SendAlert(ch_idx, "OPEN");
            }
        }
    } else {
        /* 전류가 다시 흐르면 오픈 서킷 타이머 리셋 */
        if (ch->status == CH_STATUS_FAULT_OPEN_CIRCUIT) {
            /* 부하 재연결 감지: 폴트 자동 복구 */
            Channel_ClearFault(ch_idx);
        }
        ch->oc_detecting   = false;
        ch->oc_alert_sent  = false;
    }
}

void Channel_ClearFault(uint8_t ch_idx)
{
    if (ch_idx >= NUM_CHANNELS) {
        return;
    }
    Channel_t *ch = &g_channels[ch_idx];
    ch->status         = CH_STATUS_DISABLED;
    ch->enabled        = false;
    ch->oc_detecting   = false;
    ch->oc_alert_sent  = false;
    ch->dac_value      = 0U;
    PID_Reset(&ch->pid);
    ApplyDacOutput(ch_idx);
}

const char *Channel_GetStatusStr(ChannelStatus_t status)
{
    switch (status) {
        case CH_STATUS_DISABLED:           return "DISABLED";
        case CH_STATUS_ENABLED:            return "OK";
        case CH_STATUS_FAULT_OVERCURRENT:  return "FAULT_OC";
        case CH_STATUS_FAULT_OPEN_CIRCUIT: return "FAULT_OPEN";
        case CH_STATUS_FAULT_HARDWARE:     return "FAULT_HW";
        default:                           return "UNKNOWN";
    }
}
