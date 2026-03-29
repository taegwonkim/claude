#ifndef FDCAN_PROTOCOL_H
#define FDCAN_PROTOCOL_H

/* =========================================================
 * FDCAN 프로토콜 모듈
 *
 * 역할:
 *  - 전압/전류 측정값을 CAN 버스로 PC에 주기 전송
 *  - PC(USB-CAN 어댑터)에서 전압 설정 명령 수신
 *  - UART와 동일한 명령체계를 CAN으로도 지원
 *
 * 하드웨어:
 *  FDCAN1 TX : PB9  (AF9)
 *  FDCAN1 RX : PB8  (AF9)
 *  CAN 트랜시버: SN65HVD230 (3.3V) 또는 TJA1051T
 *
 * CAN 메시지 ID (11비트 Standard ID):
 *  0x100~0x103 : 채널1~4 전압/전류 상태 (TX, 주기 전송)
 *  0x110       : 전체 채널 요약 (TX, 주기 전송)
 *  0x200       : 폴트 이벤트   (TX, 비동기)
 *  0x300       : SET 명령      (RX, PC→STM32)
 *  0x301       : GET 요청      (RX, PC→STM32)
 *  0x302       : ENABLE 명령   (RX, PC→STM32)
 *  0x303       : RESET 명령    (RX, PC→STM32)
 *
 * Nominal 비트레이트: 500 kbps (Classic CAN 호환)
 * Data    비트레이트: 2 Mbps   (FDCAN BRS 사용 시)
 * =========================================================*/

#include "main.h"
#include "voltage_control.h"
#include "current_monitor.h"
#include <stdint.h>
#include <stdbool.h>

/* ----- FDCAN 핸들 ----- */
extern FDCAN_HandleTypeDef hfdcan1;

/* ----- CAN 메시지 ID 정의 ----- */
#define FDCAN_ID_CH1_STATUS     0x100U  /* 채널1 전압/전류 상태 */
#define FDCAN_ID_CH2_STATUS     0x101U
#define FDCAN_ID_CH3_STATUS     0x102U
#define FDCAN_ID_CH4_STATUS     0x103U
#define FDCAN_ID_ALL_STATUS     0x110U  /* 전체 채널 요약 (16바이트, FDCAN만) */
#define FDCAN_ID_FAULT_EVENT    0x200U  /* 폴트 이벤트 알림 */
#define FDCAN_ID_CMD_SET        0x300U  /* SET <ch> <mv> 명령 수신 */
#define FDCAN_ID_CMD_GET        0x301U  /* GET <ch> 명령 수신 */
#define FDCAN_ID_CMD_ENABLE     0x302U  /* ENABLE <ch> <0|1> 수신 */
#define FDCAN_ID_CMD_RESET      0x303U  /* RESET 명령 수신 */

/* ----- 메시지 DLC (데이터 길이) ----- */
#define FDCAN_DLC_CH_STATUS     8U      /* 채널 상태: 8바이트 */
#define FDCAN_DLC_ALL_STATUS    16U     /* 전체 요약: 16바이트 (FDCAN 전용) */
#define FDCAN_DLC_FAULT         4U      /* 폴트 이벤트: 4바이트 */
#define FDCAN_DLC_CMD_SET       4U      /* SET 명령: 4바이트 */
#define FDCAN_DLC_CMD_GET       2U
#define FDCAN_DLC_CMD_ENABLE    2U
#define FDCAN_DLC_CMD_RESET     2U

/* ----- 채널 상태 메시지 레이아웃 (8바이트) ----- */
/*
 * Byte 0    : 채널 번호 (1~4)
 * Byte 1    : 상태 플래그 (CH_STATUS_*)
 * Byte 2~3  : 목표 전압 (mV, uint16, Big-Endian)
 * Byte 4~5  : 측정 전압 (mV, int16, Big-Endian)
 * Byte 6~7  : 측정 전류 (mA, int16, Big-Endian)
 */

/* ----- 전체 요약 메시지 레이아웃 (16바이트, FDCAN only) ----- */
/*
 * Byte 0~3  : CH1~CH4 목표전압 (각 1바이트 × 33 = 0~3300mV → 0~100%)
 * Byte 4~7  : CH1~CH4 측정전압 (각 1바이트, %)
 * Byte 8~11 : CH1~CH4 전류 (각 1바이트, 0~255 = 0~1000mA)
 * Byte 12~15: CH1~CH4 상태 플래그 (각 1바이트)
 */

/* ----- 폴트 이벤트 레이아웃 (4바이트) ----- */
/*
 * Byte 0 : 채널 번호 (1~4)
 * Byte 1 : 폴트 타입 (0=NONE, 1=SHORT, 2=OPEN)
 * Byte 2~3 : 폴트 발생 시 전류 (mA, uint16)
 */

/* ----- SET 명령 레이아웃 (4바이트, PC→STM32) ----- */
/*
 * Byte 0 : 채널 번호 (1~4)
 * Byte 1 : 예약 (0)
 * Byte 2~3 : 목표 전압 (mV, uint16, 0~3300)
 */

/* ----- GET 명령 레이아웃 (2바이트, PC→STM32) ----- */
/*
 * Byte 0 : 채널 번호 (0xFF = 전체)
 * Byte 1 : 예약 (0)
 */

/* ----- ENABLE 명령 레이아웃 (2바이트) ----- */
/*
 * Byte 0 : 채널 번호 (1~4, 0xFF=전체)
 * Byte 1 : 0=비활성화, 1=활성화
 */

/* ----- RESET 명령 레이아웃 (2바이트) ----- */
/*
 * Byte 0 : 채널 번호 (1~4, 0xFF=전체)
 * Byte 1 : 예약 (0)
 */

/* ----- 전송 주기 ----- */
#define FDCAN_TX_PERIOD_MS      50U     /* 채널 상태 주기 전송 간격 (50ms = 20Hz) */
#define FDCAN_RXFIFO_DEPTH      8U      /* RX FIFO 큐 깊이 */

/* ----- FDCAN RX 수신 큐 아이템 ----- */
typedef struct {
    uint32_t id;             /* CAN 메시지 ID */
    uint8_t  dlc;            /* 데이터 길이 */
    uint8_t  data[8];        /* 데이터 페이로드 */
} FdcanRxItem_t;

/* ----- 함수 선언 ----- */
HAL_StatusTypeDef FdcanProto_Init(void);

/* TX: 주기 전송 (vFdcanTask에서 호출) */
HAL_StatusTypeDef FdcanProto_SendChannelStatus(VoltageCtrl_t *hctrl, uint8_t ch);
HAL_StatusTypeDef FdcanProto_SendAllStatus(VoltageCtrl_t *hctrl);
HAL_StatusTypeDef FdcanProto_SendFaultEvent(uint8_t ch, FaultType_t fault, int32_t current_ma);

/* RX: 수신 메시지 처리 (큐에서 꺼내어 실행) */
void FdcanProto_ProcessRx(FdcanRxItem_t *item,
                           VoltageCtrl_t *hctrl,
                           CurrentMonitor_t *hmon);

/* ISR 콜백에서 호출 (RX FIFO0 수신 알림) */
void FdcanProto_RxCallback(void);

/* FreeRTOS 태스크 */
void vFdcanTask(void *argument);

#endif /* FDCAN_PROTOCOL_H */
