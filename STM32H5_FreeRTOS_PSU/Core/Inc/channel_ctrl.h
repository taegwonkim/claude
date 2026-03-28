/**
 * @file    channel_ctrl.h
 * @brief   4채널 전압/전류 제어 및 보호 로직
 *
 * 각 채널은 독립적인 PID 제어기를 가지며 다음을 수행합니다:
 *   1. 출력 전압을 설정값으로 유지 (부하 변동 보상)
 *   2. 과전류 감지 시 채널 차단
 *   3. 오픈 서킷 감지 시 경보 발생 및 전압 감소
 *   4. 상태 텔레메트리 전송
 */

#ifndef CHANNEL_CTRL_H
#define CHANNEL_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "pid.h"
#include "main.h"

/* =========================================================
 * 채널 상태 열거형
 * ========================================================= */
typedef enum {
    CH_STATUS_DISABLED = 0,         /* 비활성화 상태 */
    CH_STATUS_ENABLED,              /* 정상 출력 중 */
    CH_STATUS_FAULT_OVERCURRENT,    /* 과전류 폴트 */
    CH_STATUS_FAULT_OPEN_CIRCUIT,   /* 오픈 서킷 폴트 */
    CH_STATUS_FAULT_HARDWARE,       /* 하드웨어 폴트 */
} ChannelStatus_t;

/* =========================================================
 * 채널 구조체
 * ========================================================= */
typedef struct {
    uint8_t         id;                 /* 채널 ID (1~4) */
    bool            enabled;            /* 출력 활성화 여부 */
    ChannelStatus_t status;             /* 현재 상태 */

    /* --- 설정값 --- */
    float           v_setpoint_mv;      /* 목표 전압 (mV) */
    float           i_limit_ma;         /* 전류 제한값 (mA) */

    /* --- 측정값 --- */
    float           v_meas_mv;          /* 측정 전압 (mV) */
    float           i_meas_ma;          /* 측정 전류 (mA) */

    /* --- DAC 출력 --- */
    uint32_t        dac_value;          /* 현재 DAC 코드 (0~4095) */

    /* --- PID 제어기 --- */
    PID_t           pid;

    /* --- 오픈 서킷 감지 --- */
    uint32_t        oc_detect_start_ms; /* 오픈 서킷 시작 타임스탬프 */
    bool            oc_detecting;       /* 오픈 서킷 감지 중 */
    bool            oc_alert_sent;      /* 경보 이미 전송 여부 */

    /* --- 과전류 자동복구 --- */
    uint32_t        ovc_trip_time_ms;   /* 과전류 발생 타임스탬프 */

    /* --- 통계 (이동 평균) --- */
    float           v_avg_mv;
    float           i_avg_ma;
    uint32_t        fault_count;
} Channel_t;

/* =========================================================
 * 명령 구조체 (UART RX -> Control Task 큐)
 * ========================================================= */
typedef enum {
    CMD_SET_VOLTAGE = 0,
    CMD_GET_STATUS,
    CMD_ENABLE,
    CMD_DISABLE,
    CMD_SET_KP,
    CMD_SET_KI,
    CMD_SET_KD,
    CMD_SET_ILIMIT,
    CMD_GET_ALL,
} CmdType_t;

typedef struct {
    CmdType_t   type;
    uint8_t     channel;    /* 1~4, 0=ALL */
    float       value;
} Command_t;

/* =========================================================
 * 전송 문자열 큐 항목 (Control -> UART TX)
 * ========================================================= */
#define TX_MSG_MAX_LEN  128
typedef struct {
    char msg[TX_MSG_MAX_LEN];
} TxMessage_t;

/* =========================================================
 * 전역 채널 배열
 * ========================================================= */
extern Channel_t g_channels[NUM_CHANNELS];

/* =========================================================
 * 함수 프로토타입
 * ========================================================= */

/**
 * @brief 전체 채널 초기화 (PID, 기본값 설정)
 */
void Channel_Init(void);

/**
 * @brief 채널 전압 설정값 변경
 * @param ch_idx  채널 인덱스 (0~3)
 * @param v_mv    목표 전압 (mV)
 */
void Channel_SetVoltage(uint8_t ch_idx, float v_mv);

/**
 * @brief 채널 활성화 (DAC 출력 시작, PID 리셋)
 */
void Channel_Enable(uint8_t ch_idx);

/**
 * @brief 채널 비활성화 (DAC 출력 0으로)
 */
void Channel_Disable(uint8_t ch_idx);

/**
 * @brief ADC 측정값 업데이트 (ADC Task에서 호출)
 * @param ch_idx  채널 인덱스 (0~3)
 * @param v_mv    측정 전압 (mV)
 * @param i_ma    측정 전류 (mA)
 */
void Channel_UpdateMeasurements(uint8_t ch_idx, float v_mv, float i_ma);

/**
 * @brief PID 제어 루프 실행 (Control Task에서 매 주기 호출)
 * @param ch_idx  채널 인덱스 (0~3)
 */
void Channel_RunControl(uint8_t ch_idx);

/**
 * @brief 폴트 감지 및 처리 (Fault Monitor Task에서 호출)
 * @param ch_idx  채널 인덱스 (0~3)
 */
void Channel_CheckFaults(uint8_t ch_idx);

/**
 * @brief 폴트 초기화 및 재활성화 시도
 */
void Channel_ClearFault(uint8_t ch_idx);

/**
 * @brief 채널 상태 문자열 반환
 */
const char *Channel_GetStatusStr(ChannelStatus_t status);

#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_CTRL_H */
