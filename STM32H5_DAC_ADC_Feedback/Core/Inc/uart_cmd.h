/**
 * @file    uart_cmd.h
 * @brief   PC ↔ STM32H5 UART 명령/피드백 프로토콜 정의
 *
 * ============================================================
 * PC → STM32 명령 (115200 baud, \r\n 또는 \n 종료)
 * ============================================================
 *
 *  SET <ch> <volt>           채널 목표 전압 설정
 *    예) SET 0 2.500
 *
 *  GET <ch>                  특정 채널 상태 응답 요청
 *    예) GET 2
 *
 *  GET ALL                   전체 채널 상태 응답 요청
 *
 *  PID <ch> <Kp> <Ki> <Kd>  PID 파라미터 변경
 *    예) PID 0 200.0 50.0 5.0
 *
 *  ENABLE  <ch>              채널 제어 활성화
 *  DISABLE <ch>              채널 제어 비활성화 (출력 0)
 *
 *  STREAM ON                 주기 자동 피드백 스트림 시작
 *  STREAM OFF                주기 자동 피드백 스트림 중단
 *
 *  HELP                      명령어 목록 출력
 *
 * ============================================================
 * STM32 → PC 응답 형식
 * ============================================================
 *
 *  OK: <명령>                처리 성공
 *  ERR: <사유>               처리 실패
 *
 *  단일 채널 상태:
 *    #CH<n>,SET=<V>,FB=<V>,ERR=<V>,OUT=<raw>,TYPE=<DAC|PWM>\r\n
 *
 *  전체 스트림 (STREAM ON 또는 주기 출력):
 *    @<tick>,<ch0_fb>,<ch1_fb>,<ch2_fb>,<ch3_fb>,
 *            <ch0_out>,<ch1_out>,<ch2_out>,<ch3_out>\r\n
 *
 *  예)  @12345,1.000,1.500,2.000,2.500,1241,1861,2482,3103\r\n
 */

#ifndef UART_CMD_H
#define UART_CMD_H

#include <stdint.h>
#include <stdbool.h>
#include "voltage_ctrl.h"

/* ======================================================================== */
/*  상수                                                                      */
/* ======================================================================== */

#define UCMD_RX_BUF_SIZE    128U    /**< DMA 수신 버퍼 크기 */
#define UCMD_LINE_MAX       96U     /**< 최대 명령어 줄 길이 */
#define UCMD_TX_BUF_SIZE    256U    /**< UART 송신 임시 버퍼 */

/* ======================================================================== */
/*  명령 처리 결과 코드                                                       */
/* ======================================================================== */

typedef enum {
    UCMD_OK = 0,
    UCMD_ERR_UNKNOWN,       /**< 알 수 없는 명령 */
    UCMD_ERR_PARAM,         /**< 파라미터 범위/형식 오류 */
    UCMD_ERR_CH_RANGE,      /**< 채널 번호 범위 초과 */
} UCmdResult_t;

/* ======================================================================== */
/*  스트림 모드 상태 (외부에서 읽기 위해 공개)                               */
/* ======================================================================== */

extern volatile bool g_stream_enabled; /**< STREAM ON/OFF 상태 */

/* ======================================================================== */
/*  API                                                                       */
/* ======================================================================== */

/**
 * @brief  수신된 한 줄(line)을 파싱하고 처리 후 응답을 UART로 송신한다.
 * @param  line   NULL-terminated 명령 문자열 (trailing \r\n 제거된 상태)
 */
void UCmd_Process(const char *line);

/**
 * @brief  단일 채널 상태를 "#CH..." 형식으로 UART 출력한다.
 * @param  ch_idx  채널 인덱스 (0 ~ VCTRL_NUM_CHANNELS-1)
 */
void UCmd_PrintChannelStatus(uint8_t ch_idx);

/**
 * @brief  전체 채널을 "@tick,..." 스트림 형식으로 UART 출력한다.
 *         STREAM ON 상태이거나 vMonitorTask에서 주기적으로 호출.
 */
void UCmd_PrintStream(void);

/**
 * @brief  HELP 메시지를 UART로 출력한다.
 */
void UCmd_PrintHelp(void);

#endif /* UART_CMD_H */
