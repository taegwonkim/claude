/**
  ******************************************************************************
  * @file    pc_link_conf.h
  * @brief   PC 연결 (FDCAN1, CAN FD) 설정
  ******************************************************************************
  * 하드웨어
  *   FDCAN1_RX = PD0, FDCAN1_TX = PD1 (AF9, NUCLEO-H743ZI2 CN9)
  *   → CAN FD 트랜시버 (TCAN1042V / MCP2562FD / TJA1051T/3 등, 2 Mbps 이상 지원)
  *   → PC 의 CAN FD 어댑터 (PEAK PCAN-USB FD, Kvaser, Vector, candleLight FD 등)
  *   버스 양 끝에 120 Ω 종단. 차량 CAN 버스가 아니라 PC 전용 버스로 연결하세요.
  *
  * 비트 타이밍 (FDCAN 커널 클럭 = PLL1Q 80 MHz, HSE 8 MHz 기반)
  *   Nominal 500 kbit/s : 80 MHz / (1 x (1 + 127 + 32)) , sample point 80 %
  *   Data      2 Mbit/s : 80 MHz / (1 x (1 + 31 + 8))   , sample point 80 %
  *   PC 어댑터도 반드시 같은 비트레이트 / sample point 로 설정해야 합니다.
  ******************************************************************************
  */

#ifndef PC_LINK_CONF_H
#define PC_LINK_CONF_H

#define APP_PCLINK_ENABLE           1U

/* CAN ID (11 bit 표준 ID). 장치 → PC 는 BASE + 0x00 ~ 0x3F, PC → 장치 는 BASE + 0x40 */
#define APP_PCLINK_ID_BASE          0x700U

/* 주기 전송 [ms] (0 = 끔). PC 명령으로 바꿀 수 있음 */
#define APP_PCLINK_STATUS_MS        1000U   /* STATUS + ETH_STATS            */
#define APP_PCLINK_DUMP_MS          10000U  /* ECU / 흐름 / 이벤트 / 서비스 전체 */

/* 한 번 호출에서 보낼 최대 덤프 프레임 수 (메인 루프 지연 제한) */
#define APP_PCLINK_DUMP_BURST       4U
/* Tx FIFO 에 이만큼 빈 자리가 있을 때만 덤프 프레임 추가 (주기 프레임용 여유) */
#define APP_PCLINK_TX_RESERVE       4U

/* 비트 타이밍은 main.c 의 MX_FDCAN1_Init() (= .ioc 의 FDCAN1 설정) 에 있습니다.
 * 송신 지연 보상(TDC) 오프셋은 APP_FDCAN_Init() 에서 DataPrescaler x DataTimeSeg1 로 계산합니다. */

#endif /* PC_LINK_CONF_H */
