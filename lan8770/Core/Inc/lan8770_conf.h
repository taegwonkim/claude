/**
  ******************************************************************************
  * @file    lan8770_conf.h
  * @brief   LAN8770 + STM32H7 프로젝트 사용자 설정
  *          이 파일만 고치면 대부분의 보드 차이를 흡수할 수 있습니다.
  ******************************************************************************
  */

#ifndef LAN8770_CONF_H
#define LAN8770_CONF_H

#include "lan8770.h"

/* ---------------------------------------------------------------------------
 *  PHY 설정
 * ------------------------------------------------------------------------ */
/* PHY 주소 : EVB strap 값을 모르면 LAN8770_ADDR_AUTO (0~31 자동 탐색) */
#define APP_PHY_ADDR            LAN8770_ADDR_AUTO

/* 100BASE-T1 역할 : 상대편이 MASTER 면 이쪽은 SLAVE, 반대도 마찬가지.
 *   LAN8770_ROLE_MASTER / LAN8770_ROLE_SLAVE / LAN8770_ROLE_KEEP(strap 유지) */
#define APP_PHY_ROLE            LAN8770_ROLE_MASTER

/* nRESET 핀 : CubeMX 에서 GPIO Output 으로 만들고 User Label 을 "PHY_RST" 로
 * 주면 main.h 에 PHY_RST_Pin / PHY_RST_GPIO_Port 가 생기고 자동으로 사용됩니다. */
#define APP_PHY_RESET_LOW_MS    10U    /* 리셋 유지 시간  */
#define APP_PHY_RESET_WAIT_MS   50U    /* 리셋 해제 후 대기 */

/* PHY 초기화 실패 시 재시도 주기 [ms] (EVB 전원이 늦게 들어오는 경우 대비) */
#define APP_PHY_RETRY_MS        1000U

/* ---------------------------------------------------------------------------
 *  MAC 주소 (로컬 관리 주소 : 첫 바이트 bit1 = 1)
 * ------------------------------------------------------------------------ */
#define APP_MAC_ADDR0           0x02U
#define APP_MAC_ADDR1           0x80U
#define APP_MAC_ADDR2           0xE1U
#define APP_MAC_ADDR3           0x87U
#define APP_MAC_ADDR4           0x70U
#define APP_MAC_ADDR5           0x01U

/* ---------------------------------------------------------------------------
 *  애플리케이션
 * ------------------------------------------------------------------------ */
#define APP_UDP_ECHO_PORT       7U      /* UDP 에코 서버 포트 */
#define APP_STATUS_PERIOD_MS    2000U   /* 상태 로그 주기     */

/* printf 출력 : 1 이면 huart3 (NUCLEO-H743ZI2 의 ST-LINK VCP, PD8/PD9) 로 출력 */
#define APP_USE_UART_LOG        1U

#endif /* LAN8770_CONF_H */
