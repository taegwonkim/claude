/**
  ******************************************************************************
  * @file    vehicle_net_conf.h
  * @brief   차량 이더넷 기능 설정 : VLAN / 네트워크 모니터 / SOME/IP
  ******************************************************************************
  * ECU 를 아직 모를 때 권장 순서
  *   1) 모니터만 켜고(기본값) 연결 → 어떤 VLAN ID, EtherType, MAC 이 보이는지 확인
  *   2) VLAN 을 맞추고 SOME/IP-SD 수신으로 어떤 서비스가 Offer 되는지 확인
  *
  * ⚠ 송신 기능(SOME/IP FindService)은 기본값이 꺼져 있습니다.
  *   주행 중이거나 시동이 걸린 차량에서는 켜지 마세요. 벤치(ECU 단품)에서 시험하세요.
  ******************************************************************************
  */

#ifndef VEHICLE_NET_CONF_H
#define VEHICLE_NET_CONF_H

/* ===========================================================================
 *  1. VLAN (IEEE 802.1Q)
 * ======================================================================== */
/* 1 : 송신 프레임에 VLAN 태그를 붙이고, 수신은 APP_VLAN_ID 만 받음
 * 0 : 태그 없이 동작 (태그 붙은 프레임은 lwIP 로 넘기지 않음, 모니터 통계에는 잡힘) */
#define APP_VLAN_ENABLE             0U
#define APP_VLAN_ID                 2U      /* 1 ~ 4094                         */
#define APP_VLAN_PRIORITY           0U      /* PCP 0 ~ 7                        */
/* VLAN 사용 중에도 태그 없는 프레임을 받을지 (1 = 받음) */
#define APP_VLAN_ACCEPT_UNTAGGED    1U

/* ===========================================================================
 *  2. 네트워크 모니터 (수신 전용, 아무것도 송신하지 않음)
 * ======================================================================== */
#define APP_NETMON_ENABLE           1U
/* 1 : 다른 MAC 주소로 가는 프레임도 수신 (스위치가 흘려보내는 트래픽 관찰용) */
#define APP_NETMON_PROMISCUOUS      0U
#define APP_NETMON_PERIOD_MS        10000U  /* 통계 출력 주기                   */
#define APP_NETMON_MAX_VLANS        16U     /* 기록할 VLAN ID 개수              */
#define APP_NETMON_MAX_ETHERTYPES   16U     /* 기록할 EtherType 개수            */
#define APP_NETMON_MAX_MACS         16U     /* 기록할 송신자 MAC 개수           */

/* ===========================================================================
 *  3. SOME/IP + SOME/IP-SD
 * ======================================================================== */
#define APP_SOMEIP_ENABLE           1U
#define APP_SOMEIP_SD_PORT          30490U
/* SD 멀티캐스트 주소 (AUTOSAR 기본값 예시, OEM 마다 다름) */
#define APP_SOMEIP_SD_MCAST_ADDR    224, 224, 224, 245
/* 추가로 수신할 SOME/IP 서비스 UDP 포트 (유니캐스트/멀티캐스트 이벤트). 0 은 무시 */
#define APP_SOMEIP_EXTRA_PORTS      { 0U, 0U, 0U, 0U }
#define APP_SOMEIP_MAX_SERVICES     32U     /* SD 로 발견한 서비스 테이블 크기  */
#define APP_SOMEIP_LOG_PAYLOAD      16U     /* 로그에 보여줄 payload 바이트 수  */
#define APP_SOMEIP_TABLE_PERIOD_MS  10000U  /* 서비스 테이블 출력 주기          */
/* 1 : SD 멀티캐스트 그룹에 IGMP 가입 (IGMP 리포트를 송신함)
 * 0 : 송신 없이 모든 멀티캐스트를 받음 (MAC 필터를 열어 둠) ← 기본 */
#define APP_SOMEIP_IGMP_JOIN        0U

/* ⚠ 송신 : FindService(모든 서비스) 를 주기적으로 멀티캐스트 */
#define APP_SOMEIP_SEND_FIND        0U
#define APP_SOMEIP_FIND_PERIOD_MS   5000U
#define APP_SOMEIP_CLIENT_ID        0x0000U

/* ===========================================================================
 *  4. ECU 테이블 (송신자 IP 별로 MAC / VLAN / 흐름 / SOME/IP 이벤트 / 주기 집계)
 * ======================================================================== */
#define APP_ECU_TABLE_ENABLE        1U
#define APP_ECU_MAX                 16U     /* 기록할 ECU(송신 IP) 수            */
#define APP_ECU_MAX_FLOWS           8U      /* ECU 당 (프로토콜, 포트) 흐름 수    */
#define APP_ECU_MAX_EVENTS          16U     /* ECU 당 SOME/IP (서비스, 이벤트) 수 */
#define APP_ECU_MAX_OFFERS          8U      /* ECU 당 SD Offer 서비스 수          */
#define APP_ECU_PAYLOAD_BYTES       8U      /* 이벤트별로 저장할 마지막 payload   */
/* 1 초에 ECU 하나씩 돌아가며 출력 (UART 가 막히지 않도록) */
#define APP_ECU_PRINT_INTERVAL_MS   1000U
/* 전체 ECU 한 줄 요약 주기 */
#define APP_ECU_SUMMARY_PERIOD_MS   30000U
/* 이 시간 동안 아무것도 안 보낸 ECU 는 출력에서 (idle) 표시 */
#define APP_ECU_IDLE_MS             5000U

/* ===========================================================================
 *  로그
 * ======================================================================== */
/* 초당 출력할 수 있는 로그 줄 수 (UART 115200 이 막히지 않도록) */
#define APP_LOG_LINES_PER_SEC       20U

#endif /* VEHICLE_NET_CONF_H */
