/**
 * @file    voltage_control.c
 * @brief   4채널 전압 PID 제어 로직 구현
 */
#include "voltage_control.h"
#include "main.h"

/* 기본 설정점 배열 */
static const float k_default_setpoints[VCTRL_NUM_CHANNELS] = {
    VCTRL_DEFAULT_SETPOINT_CH0,
    VCTRL_DEFAULT_SETPOINT_CH1,
    VCTRL_DEFAULT_SETPOINT_CH2,
    VCTRL_DEFAULT_SETPOINT_CH3,
};

/* ------------------------------------------------------------------ */
/*  초기화                                                               */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef VCtrl_Init(VCtrl_HandleTypeDef *hctrl,
                              SPI_HandleTypeDef   *hspi1,
                              SPI_HandleTypeDef   *hspi2)
{
    if (!hctrl || !hspi1 || !hspi2) return HAL_ERROR;
    HAL_StatusTypeDef st;

    /* --- AD5641 DAC 초기화 ---
     *  Vref = 2.5V, Gain = 1  → 출력 범위 0 ~ 2.5V
     *  실제 전력단과 연결된 경우 이 값을 전력단 스케일에 맞게 조정 */
    st = AD5641_Init(&hctrl->dac, hspi1, 2.5f, 1.0f);
    if (st != HAL_OK) return st;

    /* CS 핀 등록 */
    AD5641_SetCS(&hctrl->dac, 0, CS_DAC_CH0_PORT, CS_DAC_CH0_PIN);
    AD5641_SetCS(&hctrl->dac, 1, CS_DAC_CH1_PORT, CS_DAC_CH1_PIN);
    AD5641_SetCS(&hctrl->dac, 2, CS_DAC_CH2_PORT, CS_DAC_CH2_PIN);
    AD5641_SetCS(&hctrl->dac, 3, CS_DAC_CH3_PORT, CS_DAC_CH3_PIN);

    /* 모든 DAC 출력 0V로 초기화 */
    for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
        AD5641_SetVoltage(&hctrl->dac, i, 0.0f);
    }

    /* --- MCP3465R ADC 초기화 --- */
    st = MCP3465R_Init(&hctrl->adc, hspi2,
                        CS_ADC_PORT,  CS_ADC_PIN,
                        ADC_INT_PORT, ADC_INT_PIN);
    if (st != HAL_OK) return st;

    /* --- 채널별 PID 및 상태 초기화 --- */
    for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
        VCtrl_ChannelTypeDef *ch = &hctrl->ch[i];

        ch->setpoint   = k_default_setpoints[i];
        ch->measured   = 0.0f;
        ch->dac_output = 0.0f;
        ch->error      = 0.0f;
        ch->enabled    = true;

        PID_Init(&ch->pid,
                 VCTRL_DEFAULT_KP,
                 VCTRL_DEFAULT_KI,
                 VCTRL_DEFAULT_KD,
                 VCTRL_DAC_OUTPUT_MIN,
                 VCTRL_DAC_OUTPUT_MAX,
                 VCTRL_SAMPLE_TIME_S);

        PID_SetSetpoint(&ch->pid, ch->setpoint);
    }

    /* --- FreeRTOS 뮤텍스 생성 --- */
    hctrl->data_mutex = osMutexNew(NULL);
    if (hctrl->data_mutex == NULL) return HAL_ERROR;

    hctrl->loop_count  = 0;
    hctrl->error_count = 0;

    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  설정점 변경 (스레드 안전)                                              */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef VCtrl_SetSetpoint(VCtrl_HandleTypeDef *hctrl,
                                     uint8_t channel, float setpoint)
{
    if (channel >= VCTRL_NUM_CHANNELS) return HAL_ERROR;

    /* DAC 출력 범위 내로 클램핑 */
    if (setpoint < VCTRL_DAC_OUTPUT_MIN) setpoint = VCTRL_DAC_OUTPUT_MIN;
    if (setpoint > VCTRL_DAC_OUTPUT_MAX) setpoint = VCTRL_DAC_OUTPUT_MAX;

    osMutexAcquire(hctrl->data_mutex, osWaitForever);
    hctrl->ch[channel].setpoint = setpoint;
    PID_SetSetpoint(&hctrl->ch[channel].pid, setpoint);
    PID_Reset(&hctrl->ch[channel].pid);  /* 급격한 설정값 변경 시 리셋 */
    osMutexRelease(hctrl->data_mutex);

    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  PID 이득 변경 (스레드 안전)                                           */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef VCtrl_SetGains(VCtrl_HandleTypeDef *hctrl,
                                  uint8_t channel,
                                  float Kp, float Ki, float Kd)
{
    if (channel >= VCTRL_NUM_CHANNELS) return HAL_ERROR;

    osMutexAcquire(hctrl->data_mutex, osWaitForever);
    PID_SetGains(&hctrl->ch[channel].pid, Kp, Ki, Kd);
    PID_Reset(&hctrl->ch[channel].pid);
    osMutexRelease(hctrl->data_mutex);

    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  채널 활성/비활성                                                      */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef VCtrl_EnableChannel(VCtrl_HandleTypeDef *hctrl,
                                       uint8_t channel, bool enable)
{
    if (channel >= VCTRL_NUM_CHANNELS) return HAL_ERROR;

    osMutexAcquire(hctrl->data_mutex, osWaitForever);
    hctrl->ch[channel].enabled = enable;
    if (!enable) {
        /* 비활성화 시 DAC 즉시 0V */
        AD5641_SetVoltage(&hctrl->dac, channel, 0.0f);
        hctrl->ch[channel].dac_output = 0.0f;
        PID_Reset(&hctrl->ch[channel].pid);
    }
    osMutexRelease(hctrl->data_mutex);

    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  채널 상태 읽기 (스레드 안전 복사)                                      */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef VCtrl_GetChannelStatus(VCtrl_HandleTypeDef  *hctrl,
                                          uint8_t               channel,
                                          VCtrl_ChannelTypeDef *out)
{
    if (channel >= VCTRL_NUM_CHANNELS || !out) return HAL_ERROR;

    osMutexAcquire(hctrl->data_mutex, osWaitForever);
    *out = hctrl->ch[channel];
    osMutexRelease(hctrl->data_mutex);

    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  PID 제어 루프 1회 실행                                                */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef VCtrl_Update(VCtrl_HandleTypeDef *hctrl)
{
    float voltages[VCTRL_NUM_CHANNELS] = {0.0f};

    /* 1. 4채널 ADC 순차 읽기 (~4 × 0.5ms = ~2ms) */
    HAL_StatusTypeDef adc_st = MCP3465R_ReadAllChannels(&hctrl->adc, voltages);

    osMutexAcquire(hctrl->data_mutex, osWaitForever);

    if (adc_st != HAL_OK) {
        hctrl->error_count++;
        osMutexRelease(hctrl->data_mutex);
        return adc_st;
    }

    /* 2. 채널별 PID 계산 및 DAC 갱신 */
    for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
        VCtrl_ChannelTypeDef *c = &hctrl->ch[ch];

        c->measured = voltages[ch];
        c->error    = c->setpoint - c->measured;

        if (!c->enabled) continue;

        float output = PID_Compute(&c->pid, c->measured);
        c->dac_output = output;

        AD5641_SetVoltage(&hctrl->dac, ch, output);
    }

    hctrl->loop_count++;
    osMutexRelease(hctrl->data_mutex);
    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  비상 정지                                                             */
/* ------------------------------------------------------------------ */
void VCtrl_EmergencyStop(VCtrl_HandleTypeDef *hctrl)
{
    for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
        AD5641_PowerDown(&hctrl->dac, ch, AD5641_PD_HIIZ);
        hctrl->ch[ch].enabled    = false;
        hctrl->ch[ch].dac_output = 0.0f;
        PID_Reset(&hctrl->ch[ch].pid);
    }
}
