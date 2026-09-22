/**
  ******************************************************************************
  * @file    wifi_config.h
  * @brief   Wi-Fi / TCP 서버 접속 설정  ―  이 파일만 고치면 됩니다.
  *
  *   - AP  : SSID / PASSWORD
  *   - 서버: IP / PORT (TCP 클라이언트로 접속)
  *   - DHCP: 1 = AP 로부터 IP 자동 할당
  *           0 = 고정 IP (STATIC_IP / GATEWAY / NETMASK 사용)
  ******************************************************************************
  */
#ifndef __WIFI_CONFIG_H
#define __WIFI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. AP (공유기) 접속 정보
 * ======================================================================= */
#define WIFI_SSID                 "MyAccessPoint"
#define WIFI_PASSWORD             "MyPassword123"

/* =========================================================================
 * 2. 접속할 TCP 서버
 *    IP 대신 도메인 이름("example.com")도 가능 (ESP32 가 DNS 로 해석)
 * ======================================================================= */
#define SERVER_IP                 "192.168.0.10"
#define SERVER_PORT               5000U

/* TCP keep-alive 간격(초). 0 이면 사용 안 함.
   서버가 소리 없이 죽었을 때(케이블 뽑힘 등) ESP32 가 스스로 연결 끊김을
   감지해서 "CLOSED" 를 올려주므로 재접속이 훨씬 빨라진다. */
#define SERVER_TCP_KEEPALIVE_S    10U

/* =========================================================================
 * 3. IP 할당 방식
 *    WIFI_USE_DHCP = 1 : DHCP ON  (AP 가 IP 를 준다)
 *    WIFI_USE_DHCP = 0 : DHCP OFF (아래 고정 IP 사용)
 * ======================================================================= */
#define WIFI_USE_DHCP             1U

#define WIFI_STATIC_IP            "192.168.0.50"
#define WIFI_STATIC_GATEWAY       "192.168.0.1"
#define WIFI_STATIC_NETMASK       "255.255.255.0"

/* =========================================================================
 * 4. 재시도 정책
 * ======================================================================= */
#define WIFI_AP_RETRY_MS          5000U   /* AP 접속 실패 후 재시도 간격          */
#define WIFI_AP_FAIL_LIMIT        5U      /* 연속 실패 n회 → 모듈 하드웨어 리셋    */

#define WIFI_SERVER_RETRY_MS      3000U   /* 서버 접속 실패 후 재시도 간격         */
#define WIFI_SERVER_FAIL_LIMIT    5U      /* 연속 실패 n회 → AP 상태 재확인       */
#define WIFI_SERVER_FAIL_RESET    15U     /* 연속 실패 n회 → 모듈 하드웨어 리셋    */

#define WIFI_MODULE_RESET_WAIT_MS 2000U   /* 하드웨어 리셋 전 대기                */
#define WIFI_STATUS_CHECK_MS      10000U  /* 접속 중 주기적 링크 상태 확인 간격     */

/* =========================================================================
 * 5. 데모 애플리케이션 (main.c)
 *    접속되어 있는 동안 주기적으로 서버에 문자열을 보낸다. 0 이면 안 보냄.
 * ======================================================================= */
#define APP_TX_PERIOD_MS          5000U

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_CONFIG_H */
