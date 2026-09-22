/**
  ******************************************************************************
  * @file    wifi_mgr.c
  * @brief   Wi-Fi AP / TCP 서버 연결 상태 머신 + 자동 재접속
  *
  *  사용하는 AT 명령 (ESP-AT v2.x 이상, ESP32-C3)
  *
  *   초기화  : AT, ATE0, AT+GMR, AT+SYSSTORE=0, AT+CWMODE=1, AT+CWAUTOCONN=0,
  *             AT+CWRECONNCFG=0,0, AT+CIPMUX=0, AT+CIPMODE=0, AT+CIPRECVMODE=0,
  *             AT+CIPDINFO=0, AT+CIPSTAMAC?
  *   IP 설정 : AT+CWDHCP=1,1                      (DHCP ON)
  *             AT+CWDHCP=0,1 + AT+CIPSTA="ip","gw","mask"  (DHCP OFF)
  *   AP 접속 : AT+CWJAP="ssid","pwd"  → AT+CIPSTA?
  *   서버    : AT+CIPSTART="TCP","ip",port[,keepalive]
  *   상태    : AT+CIPSTATUS  (구형/신형 펌웨어 모두 대응: AT+CWSTATE?/AT+CIPSTATE? 폴백)
  *   데이터  : AT+CIPSEND=<len>  /  +IPD,<len>:<data>
  *
  *  자동 재접속 (ESP32 의 자체 재접속 기능은 꺼 두고 STM32 가 전부 제어한다)
  *
  *   - AP 끊김 ("WIFI DISCONNECT" 또는 상태 조회로 감지)
  *       → WIFI_AP_RETRY_MS 후 AP 재접속, 연속 WIFI_AP_FAIL_LIMIT 회 실패 시 모듈 리셋
  *   - 서버 끊김 ("CLOSED", SEND FAIL 또는 상태 조회로 감지)
  *       → WIFI_SERVER_RETRY_MS 후 서버 재접속,
  *         연속 WIFI_SERVER_FAIL_LIMIT 회 실패 시 AP 상태 재확인,
  *         연속 WIFI_SERVER_FAIL_RESET 회 실패 시 모듈 리셋
  *   - 모듈 무응답 (AT 명령 타임아웃) → 모듈 하드웨어 리셋 후 처음부터
  *   - 모듈이 스스로 재부팅 ("ready" 수신) → INIT 부터 다시
  ******************************************************************************
  */
#include "wifi_mgr.h"
#include "esp32_at.h"
#include "debug_log.h"

#include <string.h>
#include <stdio.h>

/* ---- 명령 타임아웃 ------------------------------------------------------------ */
#define TMO_SHORT_MS        1000U    /* AT, ATE0, 설정 명령                     */
#define TMO_SYNC_MS         500U     /* AT 동기화 시도 1회                       */
#define TMO_CWJAP_MS        20000U   /* AP 접속 (ESP-AT 내부 타임아웃 ~15s)      */
#define TMO_CIPSTART_MS     15000U   /* TCP 접속                                 */
#define TMO_STATUS_MS       2000U

#define AT_SYNC_ATTEMPTS    5U

/* ---- 링크 상태 (AT+CIPSTATUS 결과 요약) ------------------------------------------ */
typedef enum
{
  LINK_NO_AP = 0,     /* AP 에 붙어 있지 않음                 */
  LINK_AP_ONLY,       /* AP 는 붙었지만 TCP 는 끊김           */
  LINK_TCP,           /* AP + TCP 모두 정상                   */
  LINK_NO_RESPONSE,   /* 모듈이 응답하지 않음                 */
} LinkStatus_t;

/* ---- 내부 상태 ---------------------------------------------------------------- */
static WIFI_Config_t s_cfg;
static WIFI_State_t  s_state;
static WIFI_State_t  s_next_state;       /* WAIT 다음에 갈 상태                    */
static uint32_t      s_wait_until;

static uint8_t       s_ap_fail;          /* 연속 AP 접속 실패 횟수                  */
static uint8_t       s_srv_fail;         /* 연속 서버 접속 실패 횟수                */
static uint8_t       s_reset_count;      /* 연속 리셋 횟수 (로그용)                 */
static uint32_t      s_last_status_check;

static char          s_mac[24];
static char          s_ip[16];
static char          s_fw[64];

static const char *const s_state_name[] =
{
  "RESET", "INIT", "AP_CONNECT", "SERVER_CONNECT", "ONLINE", "WAIT",
};

/* ---- 유틸 -------------------------------------------------------------------- */

static void SetState(WIFI_State_t st)
{
  if (st != s_state)
  {
    LOG("WIFI: %s -> %s", s_state_name[s_state], s_state_name[st]);
    s_state = st;
  }
}

/* ms 뒤에 next 로 간다 (논블로킹 대기) */
static void GoWait(WIFI_State_t next, uint32_t ms)
{
  s_next_state = next;
  s_wait_until = HAL_GetTick() + ms;
  LOG("WIFI: retry %s in %lu ms", s_state_name[next], (unsigned long)ms);
  SetState(WIFI_STATE_WAIT);
}

static void GoReset(void)
{
  s_ap_fail  = 0U;
  s_srv_fail = 0U;
  GoWait(WIFI_STATE_RESET, WIFI_MODULE_RESET_WAIT_MS);
}

/* ---- 링크 상태 조회 ------------------------------------------------------------ */

/**
  * @brief  AT+CIPSTATUS 로 AP / TCP 상태를 묻는다.
  *
  *   STATUS:2  AP 접속 + IP 획득
  *   STATUS:3  TCP/UDP/SSL 연결됨
  *   STATUS:4  TCP/UDP/SSL 끊김
  *   STATUS:5  AP 미접속
  *   0/1       station 미초기화 / Wi-Fi 미시작
  *
  *  최신 ESP-AT 에서 CIPSTATUS 가 ERROR 를 내면 AT+CWSTATE? / AT+CIPSTATE? 로 대체.
  */
static LinkStatus_t QueryLinkStatus(void)
{
  ESP_Result_t r;
  int32_t      st;

  r = ESP_Cmd("AT+CIPSTATUS", TMO_STATUS_MS);
  if (r == ESP_TIMEOUT)
  {
    return LINK_NO_RESPONSE;
  }
  if ((r == ESP_OK) && ESP_GetInt("STATUS:", &st))
  {
    switch (st)
    {
      case 3:  return LINK_TCP;
      case 2:
      case 4:  return LINK_AP_ONLY;
      default: return LINK_NO_AP;
    }
  }

  /* 폴백: 신형 펌웨어 */
  r = ESP_Cmd("AT+CWSTATE?", TMO_STATUS_MS);
  if (r == ESP_TIMEOUT)
  {
    return LINK_NO_RESPONSE;
  }
  if ((r != ESP_OK) || !ESP_GetInt("+CWSTATE:", &st) || (st != 2))
  {
    return LINK_NO_AP;          /* 2 = 접속 + IP 획득, 그 외는 미접속으로 본다 */
  }

  r = ESP_Cmd("AT+CIPSTATE?", TMO_STATUS_MS);
  if (r == ESP_TIMEOUT)
  {
    return LINK_NO_RESPONSE;
  }
  if ((r == ESP_OK) && ESP_ResponseContains("+CIPSTATE:"))
  {
    return LINK_TCP;
  }
  return LINK_AP_ONLY;
}

/* ---- 상태별 처리 --------------------------------------------------------------- */

static void State_Reset(void)
{
  s_reset_count++;
  LOG("WIFI: module reset #%u", (unsigned)s_reset_count);

  s_mac[0] = '\0';
  s_ip[0]  = '\0';

  ESP_HardReset();
  SetState(WIFI_STATE_INIT);
}

/**
  * @brief  AT 동기화 → 에코 끄기 → 기본 설정 → MAC 읽기
  */
static void State_Init(void)
{
  uint8_t i;
  bool    synced = false;

  /* 1) 동기화: 부팅 직후엔 첫 명령이 씹히기도 해서 여러 번 시도 */
  for (i = 0U; i < AT_SYNC_ATTEMPTS; i++)
  {
    if (ESP_Cmd("AT", TMO_SYNC_MS) == ESP_OK)
    {
      synced = true;
      break;
    }
    ESP_DelayPoll(200U);
  }
  if (!synced)
  {
    LOG("WIFI: module not responding to AT");
    GoReset();
    return;
  }

  /* 2) 에코 끄기 (안 끄면 보낸 명령이 그대로 되돌아와 파싱이 지저분해진다) */
  if (ESP_Cmd("ATE0", TMO_SHORT_MS) != ESP_OK)
  {
    GoReset();
    return;
  }

  /* 3) 펌웨어 버전 (정보용) */
  if (ESP_Cmd("AT+GMR", TMO_SHORT_MS) == ESP_OK)
  {
    const char *p = ESP_Response();
    const char *e = strchr(p, '\n');
    size_t n = (e != NULL) ? (size_t)(e - p) : strlen(p);
    if (n >= sizeof(s_fw))
    {
      n = sizeof(s_fw) - 1U;
    }
    memcpy(s_fw, p, n);
    s_fw[n] = '\0';
    LOG("WIFI: firmware: %s", s_fw);
  }

  /* 4) 설정을 플래시에 저장하지 않게 (매 부팅마다 CWJAP 등을 다시 쓰므로
        플래시 수명 보호). 구형 펌웨어는 ERROR → 무시 */
  (void)ESP_Cmd("AT+SYSSTORE=0", TMO_SHORT_MS);

  /* 5) Station 모드 */
  if (ESP_Cmd("AT+CWMODE=1", TMO_SHORT_MS) != ESP_OK)
  {
    LOG("WIFI: CWMODE failed");
    GoReset();
    return;
  }

  /* 6) 모듈 자체의 자동 접속/재접속은 끈다 (STM32 가 재접속을 전담) */
  (void)ESP_Cmd("AT+CWAUTOCONN=0",   TMO_SHORT_MS);
  (void)ESP_Cmd("AT+CWRECONNCFG=0,0", TMO_SHORT_MS);

  /* 7) 단일 연결, 일반(비-투명) 전송, 수동 수신 모드 OFF(+IPD 로 바로 받음) */
  if ((ESP_Cmd("AT+CIPMUX=0",  TMO_SHORT_MS) != ESP_OK) ||
      (ESP_Cmd("AT+CIPMODE=0", TMO_SHORT_MS) != ESP_OK))
  {
    LOG("WIFI: CIPMUX/CIPMODE failed");
    GoReset();
    return;
  }
  (void)ESP_Cmd("AT+CIPRECVMODE=0", TMO_SHORT_MS);
  (void)ESP_Cmd("AT+CIPDINFO=0",    TMO_SHORT_MS);

  /* 8) MAC 주소 읽기 */
  if ((ESP_Cmd("AT+CIPSTAMAC?", TMO_SHORT_MS) == ESP_OK) &&
      ESP_GetQuoted("+CIPSTAMAC:", s_mac, sizeof(s_mac)))
  {
    LOG("WIFI: MAC address = %s", s_mac);
  }
  else
  {
    LOG("WIFI: failed to read MAC address");
    strcpy(s_mac, "??:??:??:??:??:??");
  }

  s_reset_count = 0U;
  SetState(WIFI_STATE_AP_CONNECT);
}

/**
  * @brief  DHCP / 고정 IP 설정 후 AP 접속
  */
static void State_ApConnect(void)
{
  ESP_Result_t r;
  int32_t      code;

  (void)ESP_TakeEvents();

  /* 1) IP 할당 방식 */
  if (s_cfg.use_dhcp)
  {
    LOG("WIFI: DHCP on");
    r = ESP_Cmd("AT+CWDHCP=1,1", TMO_SHORT_MS);
  }
  else
  {
    LOG("WIFI: DHCP off, static ip=%s gw=%s mask=%s",
        s_cfg.static_ip, s_cfg.gateway, s_cfg.netmask);
    r = ESP_Cmd("AT+CWDHCP=0,1", TMO_SHORT_MS);
    if (r == ESP_OK)
    {
      r = ESP_CmdFmt(TMO_SHORT_MS, "AT+CIPSTA=\"%s\",\"%s\",\"%s\"",
                     s_cfg.static_ip, s_cfg.gateway, s_cfg.netmask);
    }
  }
  if (r == ESP_TIMEOUT)
  {
    GoReset();
    return;
  }
  if (r != ESP_OK)
  {
    LOG("WIFI: IP config failed (check static IP format)");
    /* 설정이 틀린 건 재시도해도 같으니 그대로 진행해 본다 */
  }

  /* 2) AP 접속 */
  LOG("WIFI: connecting to AP '%s' ...", s_cfg.ssid);
  r = ESP_CmdFmt(TMO_CWJAP_MS, "AT+CWJAP=\"%s\",\"%s\"", s_cfg.ssid, s_cfg.password);

  if (r == ESP_OK)
  {
    s_ap_fail = 0U;
    (void)ESP_TakeEvents();     /* CWJAP 중 올라온 WIFI CONNECTED/GOT IP 소비 */

    /* 3) 할당(설정)된 IP 확인 */
    if ((ESP_Cmd("AT+CIPSTA?", TMO_SHORT_MS) == ESP_OK) &&
        ESP_GetQuoted("+CIPSTA:ip:", s_ip, sizeof(s_ip)))
    {
      LOG("WIFI: AP connected, ip=%s", s_ip);
    }
    else
    {
      LOG("WIFI: AP connected (ip unknown)");
      s_ip[0] = '\0';
    }
    SetState(WIFI_STATE_SERVER_CONNECT);
    return;
  }

  if (r == ESP_TIMEOUT)
  {
    LOG("WIFI: CWJAP no response");
    GoReset();
    return;
  }

  /* 실패 사유 (+CWJAP:<code>)  1: timeout  2: wrong password
                                3: AP not found  4: connect fail */
  if (ESP_GetInt("+CWJAP:", &code))
  {
    static const char *const reason[] =
    {
      "?", "timeout", "wrong password", "AP not found", "connect fail"
    };
    LOG("WIFI: AP connect failed: %s (%ld)",
        ((code >= 1) && (code <= 4)) ? reason[code] : "unknown", (long)code);
  }
  else
  {
    LOG("WIFI: AP connect failed");
  }

  s_ap_fail++;
  if (s_ap_fail >= WIFI_AP_FAIL_LIMIT)
  {
    LOG("WIFI: %u consecutive AP failures -> module reset", (unsigned)s_ap_fail);
    GoReset();
  }
  else
  {
    GoWait(WIFI_STATE_AP_CONNECT, WIFI_AP_RETRY_MS);
  }
}

/**
  * @brief  TCP 서버 접속
  */
static void State_ServerConnect(void)
{
  ESP_Result_t r;
  uint32_t     ev;

  (void)ESP_TakeEvents();

  LOG("WIFI: connecting to server %s:%u ...", s_cfg.server_ip, (unsigned)s_cfg.server_port);

  if (s_cfg.tcp_keepalive_s > 0U)
  {
    r = ESP_CmdFmt(TMO_CIPSTART_MS, "AT+CIPSTART=\"TCP\",\"%s\",%u,%u",
                   s_cfg.server_ip, (unsigned)s_cfg.server_port,
                   (unsigned)s_cfg.tcp_keepalive_s);
  }
  else
  {
    r = ESP_CmdFmt(TMO_CIPSTART_MS, "AT+CIPSTART=\"TCP\",\"%s\",%u",
                   s_cfg.server_ip, (unsigned)s_cfg.server_port);
  }

  if ((r == ESP_OK) || ESP_ResponseContains("ALREADY CONNECTED"))
  {
    (void)ESP_TakeEvents();     /* CONNECT 이벤트 소비 */
    s_srv_fail           = 0U;
    s_last_status_check  = HAL_GetTick();
    LOG("WIFI: server connected");
    SetState(WIFI_STATE_ONLINE);
    return;
  }

  if (r == ESP_TIMEOUT)
  {
    LOG("WIFI: CIPSTART no response");
    GoReset();
    return;
  }

  s_srv_fail++;
  LOG("WIFI: server connect failed (%u)", (unsigned)s_srv_fail);

  /* 접속 시도 중 AP 가 끊겼으면 AP 부터 다시 */
  ev = ESP_TakeEvents();
  if ((ev & ESP_EVT_WIFI_DISCONNECT) != 0U)
  {
    LOG("WIFI: AP lost during server connect");
    GoWait(WIFI_STATE_AP_CONNECT, WIFI_AP_RETRY_MS);
    return;
  }

  if (s_srv_fail >= WIFI_SERVER_FAIL_RESET)
  {
    LOG("WIFI: %u consecutive server failures -> module reset", (unsigned)s_srv_fail);
    GoReset();
    return;
  }

  if ((s_srv_fail % WIFI_SERVER_FAIL_LIMIT) == 0U)
  {
    /* 여러 번 실패했으면 AP 가 살아 있는지 확인 */
    LinkStatus_t ls = QueryLinkStatus();
    if (ls == LINK_NO_RESPONSE)
    {
      GoReset();
      return;
    }
    if (ls == LINK_NO_AP)
    {
      LOG("WIFI: AP not connected -> reconnect AP");
      GoWait(WIFI_STATE_AP_CONNECT, WIFI_AP_RETRY_MS);
      return;
    }
  }

  GoWait(WIFI_STATE_SERVER_CONNECT, WIFI_SERVER_RETRY_MS);
}

/**
  * @brief  접속 유지 감시: URC 이벤트 + 주기적 상태 조회
  */
static void State_Online(void)
{
  uint32_t ev = ESP_TakeEvents();

  if ((ev & ESP_EVT_READY) != 0U)
  {
    LOG("WIFI: module rebooted unexpectedly");
    SetState(WIFI_STATE_INIT);
    return;
  }
  if ((ev & ESP_EVT_WIFI_DISCONNECT) != 0U)
  {
    LOG("WIFI: AP disconnected");
    s_ip[0] = '\0';
    GoWait(WIFI_STATE_AP_CONNECT, WIFI_AP_RETRY_MS);
    return;
  }
  if ((ev & ESP_EVT_TCP_CLOSED) != 0U)
  {
    LOG("WIFI: server connection closed");
    GoWait(WIFI_STATE_SERVER_CONNECT, WIFI_SERVER_RETRY_MS);
    return;
  }

  /* URC 를 놓쳤을 경우를 대비한 주기적 확인 */
  if ((HAL_GetTick() - s_last_status_check) >= WIFI_STATUS_CHECK_MS)
  {
    LinkStatus_t ls;

    s_last_status_check = HAL_GetTick();
    ls = QueryLinkStatus();

    switch (ls)
    {
      case LINK_NO_RESPONSE:
        LOG("WIFI: module not responding");
        GoReset();
        break;
      case LINK_NO_AP:
        LOG("WIFI: status check: AP lost");
        s_ip[0] = '\0';
        GoWait(WIFI_STATE_AP_CONNECT, WIFI_AP_RETRY_MS);
        break;
      case LINK_AP_ONLY:
        LOG("WIFI: status check: server lost");
        GoWait(WIFI_STATE_SERVER_CONNECT, WIFI_SERVER_RETRY_MS);
        break;
      default:
        break;      /* LINK_TCP: 정상 */
    }
  }
}

static void State_Wait(void)
{
  uint32_t ev = ESP_PeekEvents();

  /* 대기 중에 들어온 이벤트로 목적지를 수정 */
  if ((ev & ESP_EVT_READY) != 0U)
  {
    (void)ESP_TakeEvents();
    LOG("WIFI: module rebooted while waiting");
    SetState(WIFI_STATE_INIT);
    return;
  }
  if (((ev & ESP_EVT_WIFI_DISCONNECT) != 0U) && (s_next_state == WIFI_STATE_SERVER_CONNECT))
  {
    (void)ESP_TakeEvents();
    LOG("WIFI: AP lost while waiting -> reconnect AP");
    s_next_state = WIFI_STATE_AP_CONNECT;
  }

  if ((int32_t)(HAL_GetTick() - s_wait_until) >= 0)
  {
    SetState(s_next_state);
  }
}

/* ---- 공개 API ---------------------------------------------------------------- */

void WIFI_GetDefaultConfig(WIFI_Config_t *cfg)
{
  memset(cfg, 0, sizeof(*cfg));
  strncpy(cfg->ssid,       WIFI_SSID,           sizeof(cfg->ssid) - 1U);
  strncpy(cfg->password,   WIFI_PASSWORD,       sizeof(cfg->password) - 1U);
  strncpy(cfg->server_ip,  SERVER_IP,           sizeof(cfg->server_ip) - 1U);
  cfg->server_port     = (uint16_t)SERVER_PORT;
  cfg->tcp_keepalive_s = (uint16_t)SERVER_TCP_KEEPALIVE_S;
  cfg->use_dhcp        = (WIFI_USE_DHCP != 0U);
  strncpy(cfg->static_ip,  WIFI_STATIC_IP,      sizeof(cfg->static_ip) - 1U);
  strncpy(cfg->gateway,    WIFI_STATIC_GATEWAY, sizeof(cfg->gateway) - 1U);
  strncpy(cfg->netmask,    WIFI_STATIC_NETMASK, sizeof(cfg->netmask) - 1U);
}

void WIFI_Init(const WIFI_Config_t *cfg)
{
  if (cfg != NULL)
  {
    s_cfg = *cfg;
  }
  else
  {
    WIFI_GetDefaultConfig(&s_cfg);
  }

  s_state             = WIFI_STATE_RESET;
  s_next_state        = WIFI_STATE_RESET;
  s_ap_fail           = 0U;
  s_srv_fail          = 0U;
  s_reset_count       = 0U;
  s_last_status_check = 0U;
  s_mac[0]            = '\0';
  s_ip[0]             = '\0';
  s_fw[0]             = '\0';

  ESP_Init();

  LOG("WIFI: ssid='%s' server=%s:%u dhcp=%s",
      s_cfg.ssid, s_cfg.server_ip, (unsigned)s_cfg.server_port,
      s_cfg.use_dhcp ? "on" : "off");
}

void WIFI_Reconfigure(const WIFI_Config_t *cfg)
{
  s_cfg = *cfg;
  (void)ESP_Cmd("AT+CIPCLOSE", TMO_SHORT_MS);
  (void)ESP_Cmd("AT+CWQAP",    TMO_SHORT_MS);
  (void)ESP_TakeEvents();
  s_ap_fail  = 0U;
  s_srv_fail = 0U;
  SetState(WIFI_STATE_AP_CONNECT);
}

void WIFI_Process(void)
{
  ESP_Poll();

  switch (s_state)
  {
    case WIFI_STATE_RESET:          State_Reset();         break;
    case WIFI_STATE_INIT:           State_Init();          break;
    case WIFI_STATE_AP_CONNECT:     State_ApConnect();     break;
    case WIFI_STATE_SERVER_CONNECT: State_ServerConnect(); break;
    case WIFI_STATE_ONLINE:         State_Online();        break;
    case WIFI_STATE_WAIT:           State_Wait();          break;
    default:                        SetState(WIFI_STATE_RESET); break;
  }
}

WIFI_State_t WIFI_GetState(void)
{
  return s_state;
}

const char *WIFI_GetStateName(void)
{
  return s_state_name[s_state];
}

bool WIFI_IsOnline(void)
{
  return s_state == WIFI_STATE_ONLINE;
}

const char *WIFI_GetMac(void)
{
  return s_mac;
}

const char *WIFI_GetIp(void)
{
  return s_ip;
}

const char *WIFI_GetFwVersion(void)
{
  return s_fw;
}

bool WIFI_Send(const uint8_t *data, uint16_t len)
{
  ESP_Result_t r;

  if (s_state != WIFI_STATE_ONLINE)
  {
    return false;
  }

  r = ESP_SendData(data, len);
  if (r == ESP_OK)
  {
    return true;
  }

  if (r == ESP_TIMEOUT)
  {
    LOG("WIFI: send timeout -> module reset");
    GoReset();
  }
  else
  {
    /* SEND FAIL / ERROR : 링크가 죽었을 가능성이 크다. 이벤트를 보고 결정 */
    uint32_t ev = ESP_TakeEvents();
    if ((ev & ESP_EVT_WIFI_DISCONNECT) != 0U)
    {
      LOG("WIFI: send failed, AP lost");
      GoWait(WIFI_STATE_AP_CONNECT, WIFI_AP_RETRY_MS);
    }
    else
    {
      LOG("WIFI: send failed, reconnecting server");
      GoWait(WIFI_STATE_SERVER_CONNECT, WIFI_SERVER_RETRY_MS);
    }
  }
  return false;
}

uint16_t WIFI_Receive(uint8_t *buf, uint16_t max_len)
{
  return ESP_DataRead(buf, max_len);
}
