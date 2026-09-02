/**
  ******************************************************************************
  * @file    cli.c
  ******************************************************************************
  */
#include "cli.h"
#include "cfg_store.h"
#include "w25q40.h"
#include "uart_link.h"
#include "usb_bridge.h"
#include "data_router.h"
#include "fpga_link.h"
#include "wifi_task.h"
#include "esp_at.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"

/* ---------------------------------------------------------------------------
 * 소도구
 * -------------------------------------------------------------------------*/
static char to_upper(char c)
{
  return ((c >= 'a') && (c <= 'z')) ? (char)(c - 'a' + 'A') : c;
}

/* 대소문자 무시 비교 */
static bool ci_eq(const char *a, const char *b)
{
  while ((*a != '\0') && (*b != '\0'))
  {
    if (to_upper(*a) != to_upper(*b)) { return false; }
    a++; b++;
  }
  return ((*a == '\0') && (*b == '\0'));
}

static char *skip_ws(char *p)
{
  while ((*p == ' ') || (*p == '\t')) { p++; }
  return p;
}

/* p 에서 다음 토큰을 잘라 반환하고, *rest 에 나머지 시작 위치를 넣는다 */
static char *next_token(char *p, char **rest)
{
  char *start;

  p = skip_ws(p);
  start = p;

  while ((*p != '\0') && (*p != ' ') && (*p != '\t')) { p++; }

  if (*p != '\0')
  {
    *p = '\0';
    *rest = skip_ws(p + 1);
  }
  else
  {
    *rest = p;
  }
  return start;
}

static void out_str(cli_ctx_t *ctx, const char *s)
{
  if (ctx->out != NULL)
  {
    ctx->out(s, (uint16_t)strlen(s));
  }
}

static void out_fmt(cli_ctx_t *ctx, const char *fmt, ...)
{
  char    buf[SD_LINE_MAX];
  va_list ap;
  int     n;

  va_start(ap, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (n > 0)
  {
    if ((uint16_t)n >= sizeof(buf)) { n = (int)sizeof(buf) - 1; }
    if (ctx->out != NULL) { ctx->out(buf, (uint16_t)n); }
  }
}

static void out_ok(cli_ctx_t *ctx)   { out_str(ctx, "OK\r\n"); }
static void out_err(cli_ctx_t *ctx, const char *why)
{
  out_fmt(ctx, "ERR,%s\r\n", why);
}

static void copy_str(char *dst, const char *src, uint16_t dst_sz)
{
  uint16_t n = 0u;

  while ((src[n] != '\0') && (n < (dst_sz - 1u)))
  {
    dst[n] = src[n];
    n++;
  }
  dst[n] = '\0';
}

/* ---------------------------------------------------------------------------
 * SET
 * -------------------------------------------------------------------------*/
static void cmd_set(cli_ctx_t *ctx, char *args)
{
  char *key;
  char *val;

  key = next_token(args, &val);

  if ((key[0] == '\0'))
  {
    out_err(ctx, "NOKEY");
    return;
  }

  if (!cfgstore_lock(1000u))
  {
    out_err(ctx, "BUSY");
    return;
  }

  bool ok = true;

  if (ci_eq(key, "SSID"))
  {
    copy_str(g_cfg.ssid, val, sizeof(g_cfg.ssid));
  }
  else if (ci_eq(key, "PASS"))
  {
    copy_str(g_cfg.pass, val, sizeof(g_cfg.pass));
  }
  else if (ci_eq(key, "SRVIP"))
  {
    if (cfgstore_is_valid_ip(val)) { copy_str(g_cfg.srv_ip, val, sizeof(g_cfg.srv_ip)); }
    else                           { ok = false; }
  }
  else if (ci_eq(key, "SRVPORT"))
  {
    long v = strtol(val, NULL, 10);
    if ((v > 0) && (v <= 65535)) { g_cfg.srv_port = (uint16_t)v; }
    else                         { ok = false; }
  }
  else if (ci_eq(key, "DHCP"))
  {
    long v = strtol(val, NULL, 10);
    if ((v == 0) || (v == 1)) { g_cfg.dhcp = (uint8_t)v; }
    else                      { ok = false; }
  }
  else if (ci_eq(key, "IP"))
  {
    if (cfgstore_is_valid_ip(val)) { copy_str(g_cfg.sta_ip, val, sizeof(g_cfg.sta_ip)); }
    else                           { ok = false; }
  }
  else if (ci_eq(key, "GW"))
  {
    if (cfgstore_is_valid_ip(val)) { copy_str(g_cfg.gw, val, sizeof(g_cfg.gw)); }
    else                           { ok = false; }
  }
  else if (ci_eq(key, "MASK"))
  {
    if (cfgstore_is_valid_ip(val)) { copy_str(g_cfg.mask, val, sizeof(g_cfg.mask)); }
    else                           { ok = false; }
  }
  else if (ci_eq(key, "SAMPLEMS"))
  {
    long v = strtol(val, NULL, 10);
    if ((v >= 100) && (v <= 60000)) { g_cfg.sample_ms = (uint32_t)v; }
    else                            { ok = false; }
  }
  else if (ci_eq(key, "OUT_UART")) { g_cfg.out_uart   = (strtol(val, NULL, 10) != 0) ? 1u : 0u; }
  else if (ci_eq(key, "OUT_USB"))  { g_cfg.out_usb    = (strtol(val, NULL, 10) != 0) ? 1u : 0u; }
  else if (ci_eq(key, "OUT_WIFI")) { g_cfg.out_wifi   = (strtol(val, NULL, 10) != 0) ? 1u : 0u; }
  else if (ci_eq(key, "AUTOSTART")){ g_cfg.auto_start = (strtol(val, NULL, 10) != 0) ? 1u : 0u; }
  else
  {
    cfgstore_unlock();
    out_err(ctx, "BADKEY");
    return;
  }

  cfgstore_unlock();

  if (!ok)
  {
    out_err(ctx, "BADVAL");
    return;
  }

  /* 실제 반영(플래시 기록 + WiFi 재접속)은 SAVE 명령에서 수행한다 */
  osEventFlagsSet(g_evtSys, SD_EVT_CFG_DIRTY);
  out_ok(ctx);
}

/* ---------------------------------------------------------------------------
 * GET
 * -------------------------------------------------------------------------*/
static void get_one(cli_ctx_t *ctx, const char *key, bool mask_pass)
{
  if      (ci_eq(key, "SSID"))     { out_fmt(ctx, "SSID=%s\r\n", g_cfg.ssid); }
  else if (ci_eq(key, "PASS"))
  {
#if SD_ALLOW_PASS_READ
    out_fmt(ctx, "PASS=%s\r\n", mask_pass ? "********" : g_cfg.pass);
#else
    (void)mask_pass;
    out_str(ctx, "PASS=********\r\n");
#endif
  }
  else if (ci_eq(key, "SRVIP"))    { out_fmt(ctx, "SRVIP=%s\r\n", g_cfg.srv_ip); }
  else if (ci_eq(key, "SRVPORT"))  { out_fmt(ctx, "SRVPORT=%u\r\n", (unsigned)g_cfg.srv_port); }
  else if (ci_eq(key, "DHCP"))     { out_fmt(ctx, "DHCP=%u\r\n", (unsigned)g_cfg.dhcp); }
  else if (ci_eq(key, "IP"))       { out_fmt(ctx, "IP=%s\r\n", g_cfg.sta_ip); }
  else if (ci_eq(key, "GW"))       { out_fmt(ctx, "GW=%s\r\n", g_cfg.gw); }
  else if (ci_eq(key, "MASK"))     { out_fmt(ctx, "MASK=%s\r\n", g_cfg.mask); }
  else if (ci_eq(key, "SAMPLEMS")) { out_fmt(ctx, "SAMPLEMS=%lu\r\n", (unsigned long)g_cfg.sample_ms); }
  else if (ci_eq(key, "OUT_UART")) { out_fmt(ctx, "OUT_UART=%u\r\n", (unsigned)g_cfg.out_uart); }
  else if (ci_eq(key, "OUT_USB"))  { out_fmt(ctx, "OUT_USB=%u\r\n", (unsigned)g_cfg.out_usb); }
  else if (ci_eq(key, "OUT_WIFI")) { out_fmt(ctx, "OUT_WIFI=%u\r\n", (unsigned)g_cfg.out_wifi); }
  else if (ci_eq(key, "AUTOSTART")){ out_fmt(ctx, "AUTOSTART=%u\r\n", (unsigned)g_cfg.auto_start); }
  else                             { out_err(ctx, "BADKEY"); }
}

static const char *const k_all_keys[] = {
  "SSID", "PASS", "SRVIP", "SRVPORT", "DHCP", "IP", "GW", "MASK",
  "SAMPLEMS", "OUT_UART", "OUT_USB", "OUT_WIFI", "AUTOSTART"
};

static void cmd_get(cli_ctx_t *ctx, char *args)
{
  char *key;
  char *rest;

  key = next_token(args, &rest);

  if (key[0] == '\0')
  {
    out_err(ctx, "NOKEY");
    return;
  }

  if (!cfgstore_lock(1000u))
  {
    out_err(ctx, "BUSY");
    return;
  }

  if (ci_eq(key, "ALL"))
  {
    for (uint32_t i = 0u; i < (sizeof(k_all_keys) / sizeof(k_all_keys[0])); i++)
    {
      get_one(ctx, k_all_keys[i], true);
    }
    cfgstore_unlock();
    out_ok(ctx);
    return;
  }

  get_one(ctx, key, false);
  cfgstore_unlock();
  out_ok(ctx);
}

/* ---------------------------------------------------------------------------
 * STATUS
 * -------------------------------------------------------------------------*/
static void cmd_status(cli_ctx_t *ctx)
{
  uint32_t fl = osEventFlagsGet(g_evtSys);

  out_fmt(ctx, "UPTIME=%lu\r\n", (unsigned long)(osKernelGetTickCount() / 1000u));
  out_fmt(ctx, "WIFI=%s\r\n",  ((fl & SD_EVT_WIFI_UP)  != 0u) ? "UP" : "DOWN");
  out_fmt(ctx, "TCP=%s\r\n",   ((fl & SD_EVT_TCP_UP)   != 0u) ? "UP" : "DOWN");
  out_fmt(ctx, "WIFISM=%s\r\n", wifi_state_str());
  out_fmt(ctx, "FPGA=%s\r\n",  ((fl & SD_EVT_FPGA_RUN) != 0u) ? "RUN" : "IDLE");
  out_fmt(ctx, "DIRTY=%u\r\n", ((fl & SD_EVT_CFG_DIRTY) != 0u) ? 1u : 0u);

  out_fmt(ctx, "TRIG=%lu\r\n",        (unsigned long)g_stat.trig_cnt);
  out_fmt(ctx, "RX_FRAME=%lu\r\n",    (unsigned long)g_stat.rx_frame);
  out_fmt(ctx, "RX_CRCERR=%lu\r\n",   (unsigned long)g_stat.rx_crcerr);
  out_fmt(ctx, "RX_TIMEOUT=%lu\r\n",  (unsigned long)g_stat.rx_timeout);
  out_fmt(ctx, "TX_UART=%lu\r\n",     (unsigned long)g_stat.tx_uart);
  out_fmt(ctx, "TX_USB=%lu\r\n",      (unsigned long)g_stat.tx_usb);
  out_fmt(ctx, "TX_WIFI=%lu\r\n",     (unsigned long)g_stat.tx_wifi);
  out_fmt(ctx, "DROP_SAMPLE=%lu\r\n", (unsigned long)g_stat.drop_sample);
  out_fmt(ctx, "DROP_WIFI=%lu\r\n",   (unsigned long)g_stat.drop_wifi);
  out_fmt(ctx, "DROP_USB=%lu\r\n",    (unsigned long)g_stat.drop_usb);
  out_fmt(ctx, "WIFI_RECONN=%lu\r\n", (unsigned long)g_stat.wifi_reconn);

  out_fmt(ctx, "STACK_FPGA=%lu\r\n",   (unsigned long)osThreadGetStackSpace(g_thFpga));
  out_fmt(ctx, "STACK_ROUTER=%lu\r\n", (unsigned long)osThreadGetStackSpace(g_thRouter));
  out_fmt(ctx, "STACK_WIFI=%lu\r\n",   (unsigned long)osThreadGetStackSpace(g_thWifi));
  out_fmt(ctx, "STACK_CLIU=%lu\r\n",   (unsigned long)osThreadGetStackSpace(g_thCliUart));
  out_fmt(ctx, "STACK_CLIB=%lu\r\n",   (unsigned long)osThreadGetStackSpace(g_thCliUsb));
  out_fmt(ctx, "HEAP_FREE=%lu\r\n",    (unsigned long)xPortGetFreeHeapSize());

  out_ok(ctx);
}

static void cmd_help(cli_ctx_t *ctx)
{
  out_str(ctx, "SET <KEY> <VAL> | GET <KEY>|ALL\r\n");
  out_str(ctx, "KEY: SSID PASS SRVIP SRVPORT DHCP IP GW MASK SAMPLEMS\r\n");
  out_str(ctx, "     OUT_UART OUT_USB OUT_WIFI AUTOSTART\r\n");
  out_str(ctx, "SAVE LOAD DEFAULT STATUS START STOP WIFI FLASHID VER REBOOT\r\n");
  out_str(ctx, "AT <raw at command>\r\n");
  out_ok(ctx);
}

/* ---------------------------------------------------------------------------
 * 라인 처리
 * -------------------------------------------------------------------------*/
void cli_exec_line(cli_ctx_t *ctx, char *line)
{
  char *cmd;
  char *args;

  line = skip_ws(line);
  if (*line == '\0') { return; }

#if (SD_RS485_ADDR != 0)
  /* "@nn:" 주소 접두어 검사 */
  if (*line == '@')
  {
    long addr = strtol(line + 1, NULL, 10);
    char *colon = strchr(line, ':');

    if ((colon == NULL) || (addr != SD_RS485_ADDR)) { return; }
    line = skip_ws(colon + 1);
  }
#endif

  cmd = next_token(line, &args);

  if      (ci_eq(cmd, "SET"))    { cmd_set(ctx, args); }
  else if (ci_eq(cmd, "WRITE"))  { cmd_set(ctx, args); }   /* 별칭 */
  else if (ci_eq(cmd, "GET"))    { cmd_get(ctx, args); }
  else if (ci_eq(cmd, "READ"))   { cmd_get(ctx, args); }   /* 별칭 */
  else if (ci_eq(cmd, "SAVE"))
  {
    if (cfgstore_save() == SD_OK)
    {
      osEventFlagsClear(g_evtSys, SD_EVT_CFG_DIRTY);
      wifi_request_reconnect();       /* 새 설정으로 재접속 */
      out_ok(ctx);
    }
    else
    {
      out_err(ctx, "FLASH");
    }
  }
  else if (ci_eq(cmd, "LOAD"))
  {
    if (cfgstore_load() == SD_OK) { out_ok(ctx); }
    else                          { out_err(ctx, "FLASH"); }
  }
  else if (ci_eq(cmd, "DEFAULT"))
  {
    if (cfgstore_lock(1000u))
    {
      cfgstore_defaults(&g_cfg);
      cfgstore_unlock();
      osEventFlagsSet(g_evtSys, SD_EVT_CFG_DIRTY);
      out_ok(ctx);
    }
    else
    {
      out_err(ctx, "BUSY");
    }
  }
  else if (ci_eq(cmd, "STATUS"))  { cmd_status(ctx); }
  else if (ci_eq(cmd, "START"))
  {
    if (fpga_send_start() == SD_OK) { out_ok(ctx); } else { out_err(ctx, "UART2"); }
  }
  else if (ci_eq(cmd, "STOP"))
  {
    if (fpga_send_stop() == SD_OK) { out_ok(ctx); } else { out_err(ctx, "UART2"); }
  }
  else if (ci_eq(cmd, "WIFI"))
  {
    wifi_request_reconnect();
    out_ok(ctx);
  }
  else if (ci_eq(cmd, "AT"))
  {
    char resp[SD_LINE_MAX];
    char at[SD_CLI_LINE_MAX];

    if (args[0] == '\0')
    {
      copy_str(at, "AT", sizeof(at));
    }
    else
    {
      (void)snprintf(at, sizeof(at), "AT%s%s", (args[0] == '+') ? "" : " ", args);
    }

    resp[0] = '\0';
    sd_res_t r = esp_cmd_resp(at, resp, sizeof(resp), 5000u);
    if (resp[0] != '\0') { out_fmt(ctx, "%s\r\n", resp); }
    if (r == SD_OK) { out_ok(ctx); } else { out_err(ctx, "AT"); }
  }
  else if (ci_eq(cmd, "FLASHID"))
  {
    uint32_t id = 0u;
    sd_res_t r;

    if (osMutexAcquire(g_mtxFlash, 1000u) == osOK)
    {
      r = w25q_read_id(&id);
      osMutexRelease(g_mtxFlash);
    }
    else
    {
      r = SD_ERR_BUSY;
    }

    if (r == SD_OK) { out_fmt(ctx, "ID=%06lX\r\n", (unsigned long)id); out_ok(ctx); }
    else            { out_err(ctx, "SPI"); }
  }
  else if (ci_eq(cmd, "VER"))
  {
    out_fmt(ctx, "VER=%s %s\r\n", SD_FW_NAME, SD_FW_VERSION);
    out_ok(ctx);
  }
  else if (ci_eq(cmd, "REBOOT"))
  {
    out_ok(ctx);
    osDelay(100u);
    NVIC_SystemReset();
  }
  else if (ci_eq(cmd, "HELP") || ci_eq(cmd, "?"))
  {
    cmd_help(ctx);
  }
  else
  {
    out_err(ctx, "BADCMD");
  }
}

void cli_init(cli_ctx_t *ctx, cli_out_fn out)
{
  memset(ctx, 0, sizeof(*ctx));
  ctx->out = out;
}

void cli_feed(cli_ctx_t *ctx, uint8_t c)
{
  if ((c == '\r') || (c == '\n'))
  {
    if (ctx->idx > 0u)
    {
      ctx->line[ctx->idx] = '\0';
      cli_exec_line(ctx, ctx->line);
      ctx->idx = 0u;
    }
    return;
  }

  if (ctx->idx < (SD_CLI_LINE_MAX - 1u))
  {
    ctx->line[ctx->idx++] = (char)c;
  }
  else
  {
    ctx->idx = 0u;                  /* 라인 과다 : 폐기 */
  }
}

/* ---------------------------------------------------------------------------
 * 태스크
 * -------------------------------------------------------------------------*/
static void out_to_uart(const char *s, uint16_t len)
{
  (void)uartlink_write(&g_lnkPc, (const uint8_t *)s, len, 500u);
}

static void out_to_usb(const char *s, uint16_t len)
{
  usbbridge_write(s, len);
}

void cli_uart_task(void *arg)
{
  static cli_ctx_t ctx;
  uint8_t c;

  (void)arg;
  cli_init(&ctx, out_to_uart);

  for (;;)
  {
    if (uartlink_getc(&g_lnkPc, &c, 1000u) == SD_OK)
    {
      cli_feed(&ctx, c);
    }
  }
}

void cli_usb_task(void *arg)
{
  static cli_ctx_t ctx;
  uint8_t c;

  (void)arg;
  cli_init(&ctx, out_to_usb);

  for (;;)
  {
    if (osMessageQueueGet(g_qUsbRx, &c, NULL, osWaitForever) == osOK)
    {
      cli_feed(&ctx, c);
    }
  }
}
