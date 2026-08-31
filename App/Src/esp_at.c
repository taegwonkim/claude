/**
  ******************************************************************************
  * @file    esp_at.c
  ******************************************************************************
  */
#include "esp_at.h"
#include "uart_link.h"
#include "main.h"
#include <stdio.h>

esp_state_t g_esp;

static char s_line[SD_AT_LINE_MAX];

/* ---------------------------------------------------------------------------
 * 내부
 * -------------------------------------------------------------------------*/
static bool line_has(const char *line, const char *needle)
{
  return (strstr(line, needle) != NULL);
}

/* URC(비동기 알림)를 상태에 반영. URC 였으면 true */
static bool handle_urc(const char *line)
{
  if (line_has(line, "WIFI GOT IP") || line_has(line, "WIFI CONNECTED"))
  {
    g_esp.wifi_up = true;
    osEventFlagsSet(g_evtSys, SD_EVT_WIFI_UP);
    return true;
  }
  if (line_has(line, "WIFI DISCONNECT"))
  {
    g_esp.wifi_up = false;
    g_esp.tcp_up  = false;
    osEventFlagsClear(g_evtSys, SD_EVT_WIFI_UP | SD_EVT_TCP_UP);
    return true;
  }
  if (line_has(line, "CLOSED"))
  {
    g_esp.tcp_up = false;
    osEventFlagsClear(g_evtSys, SD_EVT_TCP_UP);
    return true;
  }
  if (line_has(line, "CONNECT") && !line_has(line, "CONNECTED"))
  {
    g_esp.tcp_up = true;
    osEventFlagsSet(g_evtSys, SD_EVT_TCP_UP);
    return true;
  }
  if (line_has(line, "ready"))
  {
    g_esp.ready   = true;
    g_esp.wifi_up = false;
    g_esp.tcp_up  = false;
    osEventFlagsClear(g_evtSys, SD_EVT_WIFI_UP | SD_EVT_TCP_UP);
    return true;
  }
  return false;
}

static inline bool at_lock(uint32_t tmo)
{
  return (osMutexAcquire(g_mtxAt, tmo) == osOK);
}

static inline void at_unlock(void)
{
  (void)osMutexRelease(g_mtxAt);
}

/* ---------------------------------------------------------------------------
 * 공개 API
 * -------------------------------------------------------------------------*/
void esp_poll_urc(uint32_t timeout_ms)
{
  /* AT 세션이 진행 중이면 건드리지 않는다 */
  if (!at_lock(0u)) { return; }
  (void)esp_read_line(s_line, sizeof(s_line), timeout_ms);
  at_unlock();
}

int32_t esp_read_line(char *dst, uint16_t dst_sz, uint32_t timeout_ms)
{
  int32_t n = uartlink_readline(&g_lnkEsp, dst, dst_sz, timeout_ms);

  if (n >= 0)
  {
    (void)handle_urc(dst);
  }
  return n;
}

sd_res_t esp_hw_reset(uint32_t timeout_ms)
{
  uint32_t deadline;

  if (!at_lock(timeout_ms + 1000u)) { return SD_ERR_BUSY; }

  g_esp.ready   = false;
  g_esp.wifi_up = false;
  g_esp.tcp_up  = false;
  osEventFlagsClear(g_evtSys, SD_EVT_WIFI_UP | SD_EVT_TCP_UP);

  HAL_GPIO_WritePin(ESP_EN_GPIO_Port, ESP_EN_Pin, GPIO_PIN_RESET);
  osDelay(30u);
  HAL_GPIO_WritePin(ESP_EN_GPIO_Port, ESP_EN_Pin, GPIO_PIN_SET);

  uartlink_flush_rx(&g_lnkEsp);

  deadline = osKernelGetTickCount() + timeout_ms;
  while (osKernelGetTickCount() < deadline)
  {
    if (esp_read_line(s_line, sizeof(s_line), 200u) >= 0)
    {
      if (g_esp.ready)
      {
        at_unlock();
        return SD_OK;
      }
    }
  }

  at_unlock();

  /* "ready" 를 못 봤더라도 부팅 로그가 깨진 경우가 있으므로 계속 진행한다 */
  return SD_ERR_TMO;
}

sd_res_t esp_cmd_resp(const char *cmd, char *resp, uint16_t resp_sz, uint32_t timeout_ms)
{
  uint32_t deadline;

  sd_res_t res = SD_ERR_TMO;

  if (cmd == NULL) { return SD_ERR_PARAM; }
  if (!at_lock(timeout_ms + 1000u)) { return SD_ERR_BUSY; }

  uartlink_flush_rx(&g_lnkEsp);

  if ((uartlink_puts(&g_lnkEsp, cmd, 500u) != SD_OK) ||
      (uartlink_puts(&g_lnkEsp, "\r\n", 500u) != SD_OK))
  {
    at_unlock();
    return SD_ERR_HW;
  }

  deadline = osKernelGetTickCount() + timeout_ms;

  for (;;)
  {
    uint32_t now = osKernelGetTickCount();
    if (now >= deadline) { res = SD_ERR_TMO; break; }

    if (esp_read_line(s_line, sizeof(s_line), deadline - now) < 0)
    {
      res = SD_ERR_TMO;
      break;
    }
    if (s_line[0] == '\0') { continue; }

    if ((resp != NULL) && (resp_sz > 1u))
    {
      (void)strncpy(resp, s_line, resp_sz - 1u);
      resp[resp_sz - 1u] = '\0';
    }

    if (line_has(s_line, "OK"))                                { res = SD_OK;  break; }
    if (line_has(s_line, "ERROR") || line_has(s_line, "FAIL")) { res = SD_ERR; break; }
  }

  at_unlock();
  return res;
}

sd_res_t esp_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
  uint32_t deadline;

  sd_res_t res = SD_ERR_TMO;

  if (expect == NULL)
  {
    return esp_cmd_resp(cmd, NULL, 0u, timeout_ms);
  }
  if (!at_lock(timeout_ms + 1000u)) { return SD_ERR_BUSY; }

  uartlink_flush_rx(&g_lnkEsp);

  if ((uartlink_puts(&g_lnkEsp, cmd, 500u) != SD_OK) ||
      (uartlink_puts(&g_lnkEsp, "\r\n", 500u) != SD_OK))
  {
    at_unlock();
    return SD_ERR_HW;
  }

  deadline = osKernelGetTickCount() + timeout_ms;

  for (;;)
  {
    uint32_t now = osKernelGetTickCount();
    if (now >= deadline) { res = SD_ERR_TMO; break; }

    if (esp_read_line(s_line, sizeof(s_line), deadline - now) < 0)
    {
      res = SD_ERR_TMO;
      break;
    }
    if (s_line[0] == '\0') { continue; }

    if (line_has(s_line, expect))  { res = SD_OK;  break; }
    if (line_has(s_line, "ERROR")) { res = SD_ERR; break; }
    if (line_has(s_line, "FAIL"))  { res = SD_ERR; break; }
  }

  at_unlock();
  return res;
}

sd_res_t esp_tcp_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
  char     cmd[32];
  uint32_t deadline;
  uint8_t  c;
  bool     prompt = false;

  sd_res_t res = SD_ERR_TMO;

  if ((data == NULL) || (len == 0u)) { return SD_ERR_PARAM; }
  if (!at_lock(timeout_ms + 3000u))  { return SD_ERR_BUSY; }

  (void)snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", (unsigned)len);

  uartlink_flush_rx(&g_lnkEsp);
  if (uartlink_puts(&g_lnkEsp, cmd, 500u) != SD_OK)
  {
    at_unlock();
    return SD_ERR_HW;
  }

  /* '>' 프롬프트 대기 : 라인 단위가 아니라 문자 단위로 확인해야 한다 */
  deadline = osKernelGetTickCount() + 2000u;
  while (osKernelGetTickCount() < deadline)
  {
    if (uartlink_getc(&g_lnkEsp, &c, 100u) != SD_OK) { continue; }
    if (c == '>')
    {
      prompt = true;
      break;
    }
    if (c == 'E')            /* "ERROR" 시작 — 링크 미연결 등 */
    {
      (void)esp_read_line(s_line, sizeof(s_line), 100u);
      at_unlock();
      return SD_ERR;
    }
  }
  if (!prompt)
  {
    at_unlock();
    return SD_ERR_TMO;
  }

  if (uartlink_write(&g_lnkEsp, data, len, 1000u) != SD_OK)
  {
    at_unlock();
    return SD_ERR_HW;
  }

  deadline = osKernelGetTickCount() + timeout_ms;
  for (;;)
  {
    uint32_t now = osKernelGetTickCount();
    if (now >= deadline) { res = SD_ERR_TMO; break; }

    if (esp_read_line(s_line, sizeof(s_line), deadline - now) < 0)
    {
      res = SD_ERR_TMO;
      break;
    }
    if (line_has(s_line, "SEND OK"))   { res = SD_OK;  break; }
    if (line_has(s_line, "SEND FAIL")) { res = SD_ERR; break; }
    if (line_has(s_line, "ERROR"))     { res = SD_ERR; break; }
  }

  at_unlock();
  return res;
}
