/**
 ******************************************************************************
 * @file    fdcan_comm.h
 * @brief   FDCAN 통신 모듈
 *
 * CAN FD 프레임 정의:
 *   ID 0x100 : 4ch 전압 데이터 (8 bytes)
 *              Byte0-1: CH1 mV, Byte2-3: CH2 mV,
 *              Byte4-5: CH3 mV, Byte6-7: CH4 mV
 *
 *   ID 0x101 : 4ch 전류 데이터 (8 bytes)
 *              Byte0-1: CH1 mA, Byte2-3: CH2 mA,
 *              Byte4-5: CH3 mA, Byte6-7: CH4 mA
 *
 *   ID 0x102 : 결함 상태 및 목표값 (12 bytes, FDCAN FD)
 *              Byte0:   FaultStatus 비트필드 (CH1~4)
 *              Byte1:   Channel enable 비트필드
 *              Byte2-3: CH1 target mV
 *              Byte4-5: CH2 target mV
 *              Byte6-7: CH3 target mV
 *              Byte8-9: CH4 target mV
 *              Byte10:  CRC8 (Byte0~9)
 *              Byte11:  예비
 *
 *   ID 0x110 : PC→MCU 명령 수신 프레임 (4 bytes)
 *              Byte0: 명령 타입 (CmdType_t)
 *              Byte1: 채널 (1~4)
 *              Byte2-3: 값 (전압 mV)
 ******************************************************************************
 */

#ifndef __FDCAN_COMM_H
#define __FDCAN_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* -------------------------------------------------------------------------
 * CAN ID 정의
 * ------------------------------------------------------------------------- */
#define FDCAN_ID_VOLTAGE        0x100U
#define FDCAN_ID_CURRENT        0x101U
#define FDCAN_ID_STATUS         0x102U
#define FDCAN_ID_CMD_RX         0x110U  /* 수신 명령 */

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * @brief  FDCAN 필터 설정 및 시작
 *         - ID 0x110 프레임 수신 활성화 (FIFO0)
 */
HAL_StatusTypeDef FDCAN_CommInit(void);

/**
 * @brief  전압 데이터 프레임 전송 (ID 0x100)
 */
HAL_StatusTypeDef FDCAN_SendVoltage(void);

/**
 * @brief  전류 데이터 프레임 전송 (ID 0x101)
 */
HAL_StatusTypeDef FDCAN_SendCurrent(void);

/**
 * @brief  상태/결함 프레임 전송 (ID 0x102)
 */
HAL_StatusTypeDef FDCAN_SendStatus(void);

/**
 * @brief  수신 프레임 처리 (FDCAN_IT_RX_FIFO0_NEW_MESSAGE 콜백에서 호출)
 *         명령 파싱 후 xQueueUARTCmd에 UartCmd_t 추가
 */
void FDCAN_RxProcess(void);

/**
 * @brief  CRC8 계산 (Dallas/Maxim 방식)
 */
uint8_t FDCAN_CRC8(const uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __FDCAN_COMM_H */
