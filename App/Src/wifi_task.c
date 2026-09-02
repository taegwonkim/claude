/**
  ******************************************************************************
  * @file    wifi_task.c
  * @brief   ESP32-C WROOM 연결 상태머신 + 서버 전송
  *
  *  RESET → INIT → NETCFG → JOIN → TCP → ONLINE
  *    실패 시 BACKOFF 로 이동해 1→2→4→8→16→30초 지수 백오프 후 재시도.
  *    연속 SD_WIFI_FAIL_BEFORE_HWRESET 회 실패하면 ESP32 를 하드웨어 리셋.
  ******************************************************************************
  */
#include "wifi_task.h"
#include "esp_at.h"
#include "uart_link.h"
#include "cfg_store.h"
#include <stdio.h>

volatile wifi_state_t g_wifi_state = WIFI_ST_RESET;

static uint32_t s_backoff_ms  = SD_WIFI_BACKOFF_MIN_MS;
static uint32_t s_fail_cnt    = 0u;
static char     s_cmd[160];

/* 상태머신이 사용하는 설정 스냅샷 (설정 뮤텍스를 오래 잡지 않기 위해) */
static sd_cfg_t s_snap;

const char *wifi_state_str(void)
{
  switch (g_wifi_state)
  {
    case WIFI_ST_RESET:   return "RESET";
    case WIFI_ST_INIT:    return "INIT";
    case WIFI_ST_NETCFG:  return "NETCFG";
    case WIFI_ST_JOIN:    return "JOIN";
    case WIFI_ST_TCP:     return "TCP";
    case WIFI_ST_ONLINE:  return "ONLINE";
    case WIFI_ST_BACKOFF: return "BACKOFF";
    default:              return "?";
  }
}

void wifi_request_reconnect(void)
{
  osEventFlagsSet(g_evtSys, SD_EVT_WIFI_RECONF);
}

/* ---------------------------------------------------------------------------
 * 내부
 * -------------------------------------------------------------------------*/
static void snap_cfg(void)
{
  if (cfgstore_lock(1000u))
  {
    memcpy(&s_snap, &g_cfg, sizeof(s_snap));
    cfgstore_unlock();
  }
}

static void go_backoff(void)
{
  g_wifi_state = WIFI_ST_BACKOFF;
  s_fail_cnt++;
}

static void on_success(void)
{
  s_backoff_ms = SD_WIFI_BACKOFF_MIN_MS;
  s_fail_cnt   = 0u;
}

static void drain_tx_queue(void)
{
  sd_line_t line;

  /* 큐에 쌓인 라인을 비운다 (오프라인 구간에서 누적된 것 포함) */
  while (osMessageQueueGet(g_qWifiTx, &line, NULL, 0u) == osOK) { }
}

/* ---------------------------------------------------------------------------
 * 상태별 처리
 * -------------------------------------------------------------------------*/
static void st_reset(void)
{
  HAL_GPIO_WritePin(LED_WIFI_GPIO_Port, LED_WIFI_Pin, GPIO_PIN_RESET);

  snap_cfg();
  osEventFlagsClear(g_evtSys, SD_EVT_WIFI_RECONF);

  (void)esp_hw_reset(3000u);        /* "ready" 를 못 봐도 계속 진행 */
  g_wifi_state = WIFI_ST_INIT;
}

static void st_init(void)
{
  /* 부팅 직후 모듈이 busy 일 수 있으므로 AT 를 몇 번 시도 */
  for (uint8_t i = 0u; i < 5u; i++)
  {
    if (esp_cmd("AT", "OK", 1000u) == SD_OK)
    {
      if (esp_cmd("ATE0", "OK", 1000u) != SD_OK)        { go_backoff(); return; }
      if (esp_cmd("AT+CWMODE=1", "OK", 1000u) != SD_OK) { go_backoff(); return; }
      if (esp_cmd("AT+CIPMUX=0", "OK", 1000u) != SD_OK) { go_backoff(); return; }
      g_wifi_state = WIFI_ST_NETCFG;
      return;
    }
    osDelay(300u);
  }
  go_backoff();
}

static void st_netcfg(void)
{
  if (s_snap.dhcp != 0u)
  {
    if (esp_cmd("AT+CWDHCP=1,1", "OK", 2000u) != SD_OK) { go_backoff(); return; }
  }
  else
  {
    if (esp_cmd("AT+CWDHCP=1,0", "OK", 2000u) != SD_OK) { go_backoff(); return; }

    (void)snprintf(s_cmd, sizeof(s_cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"",
                   s_snap.sta_ip, s_snap.gw, s_snap.mask);
    if (esp_cmd(s_cmd, "OK", 2000u) != SD_OK) { go_backoff(); return; }
  }
  g_wifi_state = WIFI_ST_JOIN;
}

static void st_join(void)
{
  if (s_snap.ssid[0] == '\0')
  {
    /* SSID 미설정 : 백오프로 대기하며 설정을 기다린다 */
    go_backoff();
    return;
  }

  (void)snprintf(s_cmd, sizeof(s_cmd), "AT+CWJAP=\"%s\",\"%s\"",
                 s_snap.ssid, s_snap.pass);

  if (esp_cmd(s_cmd, "OK", 20000u) != SD_OK)
  {
    go_backoff();
    return;
  }

  g_esp.wifi_up = true;
  osEventFlagsSet(g_evtSys, SD_EVT_WIFI_UP);
  HAL_GPIO_WritePin(LED_WIFI_GPIO_Port, LED_WIFI_Pin, GPIO_PIN_SET);
  on_success();
  g_wifi_state = WIFI_ST_TCP;
}

static void st_tcp(void)
{
  if (!cfgstore_is_valid_ip(s_snap.srv_ip) || (s_snap.srv_port == 0u))
  {
    go_backoff();
    return;
  }

  (void)snprintf(s_cmd, sizeof(s_cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                 s_snap.srv_ip, (unsigned)s_snap.srv_port);

  if (esp_cmd(s_cmd, "OK", 10000u) != SD_OK)
  {
    /* 이미 연결되어 있으면 ALREADY CONNECTED 가 온다 — 성공으로 처리 */
    if (!g_esp.tcp_up)
    {
      go_backoff();
      return;
    }
  }

  g_esp.tcp_up = true;
  osEventFlagsSet(g_evtSys, SD_EVT_TCP_UP);
  drain_tx_queue();                 /* 오래된 데이터는 버리고 현재부터 전송 */
  on_success();
  g_wifi_state = WIFI_ST_ONLINE;
}

static void st_online(void)
{
  sd_line_t line;

  /* 설정 변경 요청 확인 */
  uint32_t fl = osEventFlagsGet(g_evtSys);
  if ((fl & SD_EVT_WIFI_RECONF) != 0u)
  {
    g_stat.wifi_reconn++;
    g_wifi_state = WIFI_ST_RESET;
    return;
  }

  if (osMessageQueueGet(g_qWifiTx, &line, NULL, 500u) != osOK)
  {
    /* 데이터가 없는 동안 URC(연결 끊김 등) 를 확인한다 */
    esp_poll_urc(10u);

    if (!g_esp.wifi_up)     { g_wifi_state = WIFI_ST_JOIN; HAL_GPIO_WritePin(LED_WIFI_GPIO_Port, LED_WIFI_Pin, GPIO_PIN_RESET); }
    else if (!g_esp.tcp_up) { g_wifi_state = WIFI_ST_TCP;  }
    return;
  }

  sd_res_t r = esp_tcp_send((const uint8_t *)line.data, line.len, 3000u);

  if (r == SD_OK)
  {
    g_stat.tx_wifi++;
  }
  else
  {
    /* 전송 실패 : 링크 상태를 다시 확인하고 필요하면 재연결 */
    g_esp.tcp_up = false;
    osEventFlagsClear(g_evtSys, SD_EVT_TCP_UP);
    g_wifi_state = g_esp.wifi_up ? WIFI_ST_TCP : WIFI_ST_JOIN;
  }
}

static void st_backoff(void)
{
  uint32_t waited = 0u;

  HAL_GPIO_WritePin(LED_WIFI_GPIO_Port, LED_WIFI_Pin, GPIO_PIN_RESET);

  /* 대기 중에도 설정 변경 요청에는 즉시 반응한다 */
  while (waited < s_backoff_ms)
  {
    if ((osEventFlagsGet(g_evtSys) & SD_EVT_WIFI_RECONF) != 0u)
    {
      break;
    }
    osDelay(100u);
    waited += 100u;
  }

  s_backoff_ms <<= 1;
  if (s_backoff_ms > SD_WIFI_BACKOFF_MAX_MS)
  {
    s_backoff_ms = SD_WIFI_BACKOFF_MAX_MS;
  }

  g_stat.wifi_reconn++;

  if (s_fail_cnt >= SD_WIFI_FAIL_BEFORE_HWRESET)
  {
    s_fail_cnt   = 0u;
    g_wifi_state = WIFI_ST_RESET;      /* 하드웨어 리셋 포함 */
  }
  else
  {
    snap_cfg();
    g_wifi_state = g_esp.wifi_up ? WIFI_ST_TCP : WIFI_ST_INIT;
  }
}

/* ---------------------------------------------------------------------------
 * 태스크
 * -------------------------------------------------------------------------*/
void wifi_task(void *arg)
{
  (void)arg;

  osDelay(500u);                       /* 다른 초기화가 끝나길 기다림 */

  for (;;)
  {
    switch (g_wifi_state)
    {
      case WIFI_ST_RESET:   st_reset();   break;
      case WIFI_ST_INIT:    st_init();    break;
      case WIFI_ST_NETCFG:  st_netcfg();  break;
      case WIFI_ST_JOIN:    st_join();    break;
      case WIFI_ST_TCP:     st_tcp();     break;
      case WIFI_ST_ONLINE:  st_online();  break;
      case WIFI_ST_BACKOFF: st_backoff(); break;
      default:              g_wifi_state = WIFI_ST_RESET; break;
    }
    osThreadYield();
  }
}
