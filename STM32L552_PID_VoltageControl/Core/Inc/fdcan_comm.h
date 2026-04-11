/* fdcan_comm.h - FDCAN 통신 인터페이스 (500 kbps, Classic CAN) */
#ifndef FDCAN_COMM_H
#define FDCAN_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32l5xx_hal.h"
#include "main.h"

/* ==========================================================
 * CAN 메시지 ID 정의
 *
 *  방향       ID         설명
 *  --------   ---------  ----------------------------------------
 *  MCU→PC     0x100+ch   채널 전압/전류 텔레메트리 (ch=0~3)
 *  MCU→PC     0x110      시스템 상태 (heartbeat)
 *  PC→MCU     0x200      채널 명령 (목표 전압 설정 등)
 *  PC→MCU     0x210      PID 게인 설정
 * ========================================================== */
#define FDCAN_ID_TELEM_BASE     0x100U   /* 텔레메트리 베이스 ID */
#define FDCAN_ID_HEARTBEAT      0x110U   /* 시스템 상태 */
#define FDCAN_ID_CMD            0x200U   /* 채널 명령 */
#define FDCAN_ID_PID_CFG        0x210U   /* PID 설정 */

/* ==========================================================
 * 텔레메트리 메시지 (0x100~0x103) 페이로드 레이아웃
 *
 *  Byte  필드               단위
 *  ----  -----------------  --------
 *   0    채널 인덱스 (0~3)  -
 *   1    상태 비트          CH_STATUS_*
 *  2~3   목표 전압          mV  (uint16, little-endian)
 *  4~5   측정 전압          mV  (uint16, little-endian)
 *  6~7   측정 전류          mA  (uint16, little-endian)
 * ========================================================== */
typedef struct __attribute__((packed)) {
    uint8_t  ch_index;
    uint8_t  status;
    uint16_t setpoint_mV;
    uint16_t voltage_mV;
    uint16_t current_mA;
} FDCAN_TelemMsg_t;

/* ==========================================================
 * 명령 메시지 (0x200) 페이로드 레이아웃
 *
 *  Byte  필드               설명
 *  ----  -----------------  ----------------------------------------
 *   0    채널 인덱스 (0~3)  0xFF = 브로드캐스트(전체 채널)
 *   1    명령 코드          아래 CMD_* 참조
 *  2~3   uint16 파라미터    단순 값 (mV 등)
 *  4~7   float 파라미터     PID 게인 등 (IEEE-754 LE)
 * ========================================================== */
typedef struct __attribute__((packed)) {
    uint8_t  ch_index;
    uint8_t  cmd_code;
    uint16_t param_u16;
    float    param_f32;
} FDCAN_CmdMsg_t;

/* 명령 코드 */
#define CMD_DISABLE         0x00U   /* 채널 비활성화 */
#define CMD_ENABLE          0x01U   /* 채널 활성화 */
#define CMD_SET_VOLTAGE     0x02U   /* 목표 전압 설정 (param_u16 = mV) */
#define CMD_SET_KP          0x10U   /* PID Kp 설정  (param_f32) */
#define CMD_SET_KI          0x11U   /* PID Ki 설정  (param_f32) */
#define CMD_SET_KD          0x12U   /* PID Kd 설정  (param_f32) */
#define CMD_PID_RESET       0x13U   /* PID 적분/미분 리셋 */

/* ==========================================================
 * 수신 명령 큐 항목
 * ========================================================== */
typedef struct {
    FDCAN_CmdMsg_t msg;
} FDCAN_RxCmd_t;

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */

/** @brief FDCAN 필터 설정 및 수신 인터럽트 활성화 */
HAL_StatusTypeDef FDCANComm_Init(void);

/**
 * @brief 한 채널의 텔레메트리 메시지 전송
 * @param ch  채널 번호 (0~3)
 * @return HAL_OK / HAL_BUSY / HAL_ERROR
 */
HAL_StatusTypeDef FDCANComm_SendTelemetry(uint8_t ch);

/** @brief 시스템 상태(heartbeat) 메시지 전송 */
HAL_StatusTypeDef FDCANComm_SendHeartbeat(void);

/**
 * @brief 수신 인터럽트 콜백에서 명령 큐에 삽입
 *        HAL_FDCAN_RxFifo0Callback() 에서 호출됨
 */
void FDCANComm_RxCallback(FDCAN_HandleTypeDef *hfdcan);

/** @brief 명령 큐에서 미처리 명령 꺼내어 적용 (RX 태스크 루프) */
void FDCANComm_ProcessCommands(void);

#ifdef __cplusplus
}
#endif

#endif /* FDCAN_COMM_H */
