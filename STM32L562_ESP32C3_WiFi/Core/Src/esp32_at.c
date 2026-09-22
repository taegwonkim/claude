/**
  ******************************************************************************
  * @file    esp32_at.c
  * @brief   ESP32-C3 AT 커맨드 드라이버 (USART1, 폴링)
  *
  *  수신 처리 흐름 (ESP_Poll)
  *
  *    UART RDR ──┬─ +IPD 데이터 수신 중이면 → 데이터 링버퍼
  *               └─ 아니면 한 줄 버퍼에 누적
  *                     ├─ '\n' 이 오면 줄 완성 → ProcessLine()
  *                     │     ├─ 명령 실행 중이면 응답 버퍼에 추가
  *                     │     ├─ OK / ERROR / FAIL → 명령 결과 확정
  *                     │     └─ ready / WIFI xxx / CONNECT / CLOSED → 이벤트 비트
  *                     ├─ 줄이 ">" 하나면 → CIPSEND 프롬프트
  *                     └─ 줄이 "+IPD,<len>:" 이면 → len 바이트 데이터 모드 진입
  ******************************************************************************
  */
#include "esp32_at.h"
#include "debug_log.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

UART_HandleTypeDef huart_esp;

/* ---- 명령 결과 --------------------------------------------------------------- */
typedef enum
{
  RES_NONE = 0,
  RES_OK,
  RES_ERROR,
  RES_BUSY,
} CmdResult_t;

/* ---- 내부 상태 --------------------------------------------------------------- */
static char        s_line[ESP_LINE_BUF_SIZE];      /* 지금 받고 있는 한 줄          */
static uint16_t    s_line_len;

static char        s_resp[ESP_RESP_BUF_SIZE];      /* 직전 명령 응답 (여러 줄)       */
static uint16_t    s_resp_len;

static bool        s_cmd_active;                   /* 명령 응답 대기 중              */
static CmdResult_t s_cmd_result;
static bool        s_prompt;                       /* '>' 프롬프트 수신              */

static const char *s_wait_token;                   /* ESP_WaitFor 용                */
static bool        s_wait_hit;

static uint32_t    s_events;

static uint8_t     s_data_buf[ESP_DATA_BUF_SIZE];  /* +IPD 데이터 링버퍼             */
static uint16_t    s_data_head;
static uint16_t    s_data_tail;
static uint16_t    s_ipd_remaining;                /* 아직 받아야 할 +IPD 바이트 수   */

static uint32_t    s_overrun_count;

/* ---- 저수준 UART ------------------------------------------------------------- */

/**
  * @brief  RX 레지스터에 바이트가 있으면 꺼낸다 (논블로킹).
  * @note   에러 플래그(오버런/프레이밍/노이즈)는 여기서 지운다. 지우지 않으면
  *         ORE 가 걸린 뒤로 수신이 멈춘 것처럼 보일 수 있다.
  */
static bool UART_ReadByte(uint8_t *ch)
{
  if (__HAL_UART_GET_FLAG(&huart_esp, UART_FLAG_ORE) != RESET)
  {
    s_overrun_count++;
    __HAL_UART_CLEAR_FLAG(&huart_esp, UART_CLEAR_OREF);
  }
  if (__HAL_UART_GET_FLAG(&huart_esp, UART_FLAG_FE) != RESET)
  {
    __HAL_UART_CLEAR_FLAG(&huart_esp, UART_CLEAR_FEF);
  }
  if (__HAL_UART_GET_FLAG(&huart_esp, UART_FLAG_NE) != RESET)
  {
    __HAL_UART_CLEAR_FLAG(&huart_esp, UART_CLEAR_NEF);
  }

  /* STM32L5 에서 UART_FLAG_RXNE 는 RXNE/RXFNE 공용 비트 (FIFO 모드 포함) */
  if (__HAL_UART_GET_FLAG(&huart_esp, UART_FLAG_RXNE) != RESET)
  {
    *ch = (uint8_t)(huart_esp.Instance->RDR & 0xFFU);
    return true;
  }
  return false;
}

static void UART_Write(const uint8_t *data, uint16_t len)
{
  if ((data == NULL) || (len == 0U))
  {
    return;
  }
  (void)HAL_UART_Transmit(&huart_esp, (uint8_t *)data, len, ESP_TX_TIMEOUT_MS);
}

/* ---- 데이터 링버퍼 ----------------------------------------------------------- */

static void Data_Push(uint8_t b)
{
  uint16_t next = (uint16_t)((s_data_head + 1U) % ESP_DATA_BUF_SIZE);
  if (next == s_data_tail)
  {
    /* 가득 참: 가장 오래된 바이트를 버린다 */
    s_data_tail = (uint16_t)((s_data_tail + 1U) % ESP_DATA_BUF_SIZE);
  }
  s_data_buf[s_data_head] = b;
  s_data_head = next;
}

uint16_t ESP_DataAvailable(void)
{
  return (uint16_t)((s_data_head + ESP_DATA_BUF_SIZE - s_data_tail) % ESP_DATA_BUF_SIZE);
}

uint16_t ESP_DataRead(uint8_t *buf, uint16_t max_len)
{
  uint16_t n = 0U;
  while ((n < max_len) && (s_data_tail != s_data_head))
  {
    buf[n++] = s_data_buf[s_data_tail];
    s_data_tail = (uint16_t)((s_data_tail + 1U) % ESP_DATA_BUF_SIZE);
  }
  return n;
}

/* ---- 줄 처리 ---------------------------------------------------------------- */

static bool StartsWith(const char *s, const char *prefix)
{
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void Resp_Append(const char *line)
{
  size_t len = strlen(line);

  if ((s_resp_len + len + 2U) >= ESP_RESP_BUF_SIZE)
  {
    return;             /* 응답 버퍼 넘침: 뒷부분은 버린다 */
  }
  memcpy(&s_resp[s_resp_len], line, len);
  s_resp_len = (uint16_t)(s_resp_len + len);
  s_resp[s_resp_len++] = '\n';
  s_resp[s_resp_len]   = '\0';
}

static void ProcessLine(const char *line)
{
#if (ESP_AT_DEBUG == 1U)
  DBG_Printf("  <- %s\r\n", line);
#endif

  if (s_cmd_active)
  {
    Resp_Append(line);
  }

  if ((s_wait_token != NULL) && StartsWith(line, s_wait_token))
  {
    s_wait_hit = true;
  }

  /* ---- 명령 최종 응답 ---- */
  if ((strcmp(line, "OK") == 0) || (strcmp(line, "SEND OK") == 0))
  {
    s_cmd_result = RES_OK;
  }
  else if ((strcmp(line, "ERROR") == 0) || (strcmp(line, "FAIL") == 0) ||
           (strcmp(line, "SEND FAIL") == 0))
  {
    s_cmd_result = RES_ERROR;
  }
  else if (StartsWith(line, "busy"))
  {
    /* "busy p..." : 직전 명령 처리 중이라 방금 보낸 명령은 버려졌다 */
    s_cmd_result = RES_BUSY;
  }
  /* ---- URC ---- */
  else if (strcmp(line, "ready") == 0)
  {
    s_events |= ESP_EVT_READY;
  }
  else if (strcmp(line, "WIFI CONNECTED") == 0)
  {
    s_events |= ESP_EVT_WIFI_CONNECTED;
  }
  else if (strcmp(line, "WIFI GOT IP") == 0)
  {
    s_events |= ESP_EVT_WIFI_GOT_IP;
  }
  else if (strcmp(line, "WIFI DISCONNECT") == 0)
  {
    s_events |= ESP_EVT_WIFI_DISCONNECT;
  }
  else if ((strcmp(line, "CONNECT") == 0) || (strcmp(line, "0,CONNECT") == 0))
  {
    s_events |= ESP_EVT_TCP_CONNECT;
  }
  else if ((strcmp(line, "CLOSED") == 0) || (strcmp(line, "0,CLOSED") == 0))
  {
    s_events |= ESP_EVT_TCP_CLOSED;
  }
  else
  {
    /* 그 외 정보성 줄 (+CWJAP:, +CIPSTAMAC:, STATUS: ...) 은 응답 버퍼에만 남긴다 */
  }
}

/* ---- 공개 함수 --------------------------------------------------------------- */

/**
  * @brief  USART1 을 115200-8-N-1 로 초기화하고 내부 버퍼를 비운다.
  * @note   GPIO/클럭은 HAL_UART_MspInit()(stm32l5xx_hal_msp.c) 에서 처리.
  */
void ESP_Init(void)
{
  huart_esp.Instance                    = ESP_UART_INSTANCE;
  huart_esp.Init.BaudRate               = ESP_UART_BAUDRATE;
  huart_esp.Init.WordLength             = UART_WORDLENGTH_8B;
  huart_esp.Init.StopBits               = UART_STOPBITS_1;
  huart_esp.Init.Parity                 = UART_PARITY_NONE;
  huart_esp.Init.Mode                   = UART_MODE_TX_RX;
  huart_esp.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
  huart_esp.Init.OverSampling           = UART_OVERSAMPLING_16;
  huart_esp.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
  huart_esp.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
  huart_esp.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  if (HAL_UART_Init(&huart_esp) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart_esp, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart_esp, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  /* 폴링 방식이므로 8바이트 RX FIFO 를 켜서 오버런 여유를 확보한다 */
  if (HAL_UARTEx_EnableFifoMode(&huart_esp) != HAL_OK)
  {
    Error_Handler();
  }

  s_line_len      = 0U;
  s_resp_len      = 0U;
  s_resp[0]       = '\0';
  s_cmd_active    = false;
  s_cmd_result    = RES_NONE;
  s_prompt        = false;
  s_wait_token    = NULL;
  s_wait_hit      = false;
  s_events        = 0U;
  s_data_head     = 0U;
  s_data_tail     = 0U;
  s_ipd_remaining = 0U;
  s_overrun_count = 0U;

  /* EN 은 평소 H (모듈 동작) */
  HAL_GPIO_WritePin(ESP_EN_GPIO_PORT, ESP_EN_PIN, GPIO_PIN_SET);
}

/**
  * @brief  EN 핀을 L→H 로 흔들어 ESP32-C3 를 하드웨어 리셋하고 "ready" 를 기다린다.
  */
void ESP_HardReset(void)
{
  uint8_t dummy;

  LOG("ESP32: hardware reset (EN low %ums)", (unsigned)ESP_EN_RESET_PULSE_MS);

  HAL_GPIO_WritePin(ESP_EN_GPIO_PORT, ESP_EN_PIN, GPIO_PIN_RESET);
  HAL_Delay(ESP_EN_RESET_PULSE_MS);

  /* 리셋 중 들어온 쓰레기 바이트와 상태를 모두 버린다 */
  while (UART_ReadByte(&dummy))
  {
  }
  s_line_len      = 0U;
  s_cmd_active    = false;
  s_cmd_result    = RES_NONE;
  s_prompt        = false;
  s_events        = 0U;
  s_ipd_remaining = 0U;
  s_data_head     = 0U;
  s_data_tail     = 0U;

  HAL_GPIO_WritePin(ESP_EN_GPIO_PORT, ESP_EN_PIN, GPIO_PIN_SET);

  if (ESP_WaitFor("ready", ESP_READY_TIMEOUT_MS))
  {
    LOG("ESP32: ready");
  }
  else
  {
    LOG("ESP32: no 'ready' within %ums (check wiring / baudrate)",
        (unsigned)ESP_READY_TIMEOUT_MS);
  }
  (void)ESP_TakeEvents();     /* READY 이벤트는 여기서 소비 */
}

/**
  * @brief  UART 수신 처리. 메인 루프와 모든 대기 루프에서 호출된다.
  */
void ESP_Poll(void)
{
  uint8_t ch;

  while (UART_ReadByte(&ch))
  {
    /* ---- +IPD 데이터 수신 모드 ---- */
    if (s_ipd_remaining > 0U)
    {
      Data_Push(ch);
      s_ipd_remaining--;
      if (s_ipd_remaining == 0U)
      {
        s_events |= ESP_EVT_DATA_RECEIVED;
      }
      continue;
    }

    /* ---- 줄 끝 ---- */
    if (ch == '\n')
    {
      if ((s_line_len > 0U) && (s_line[s_line_len - 1U] == '\r'))
      {
        s_line_len--;
      }
      s_line[s_line_len] = '\0';
      if (s_line_len > 0U)
      {
        ProcessLine(s_line);
      }
      s_line_len = 0U;
      continue;
    }

    if (s_line_len < (ESP_LINE_BUF_SIZE - 1U))
    {
      s_line[s_line_len++] = (char)ch;
    }
    else
    {
      s_line_len = 0U;    /* 비정상적으로 긴 줄: 버린다 */
      continue;
    }
    s_line[s_line_len] = '\0';

    /* ---- 줄 끝 없이 오는 특수 토큰 ---- */

    /* AT+CIPSEND 프롬프트: "\r\n>" 뒤에 개행이 오지 않는다 */
    if ((s_line_len == 1U) && (s_line[0] == '>'))
    {
      s_prompt   = true;
      s_line_len = 0U;
      continue;
    }

    /* "+IPD,<len>:" 뒤에 곧바로 <len> 바이트의 데이터가 붙는다.
       (CIPMUX=1 이면 "+IPD,<id>,<len>:" 이라 마지막 숫자를 쓴다) */
    if ((ch == ':') && StartsWith(s_line, "+IPD,"))
    {
      const char *p = strrchr(s_line, ',');
      long len = (p != NULL) ? strtol(p + 1, NULL, 10) : 0L;

#if (ESP_AT_DEBUG == 1U)
      DBG_Printf("  <- %s (%ld bytes)\r\n", s_line, len);
#endif
      if (len > 0L)
      {
        s_ipd_remaining = (uint16_t)len;
      }
      s_line_len = 0U;
      continue;
    }
  }
}

void ESP_DelayPoll(uint32_t ms)
{
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < ms)
  {
    ESP_Poll();
  }
}

/**
  * @brief  명령을 보내고 OK / ERROR 까지 기다린다.
  * @note   "busy p..." 가 오면 잠시 기다렸다가 최대 3회 다시 보낸다.
  */
ESP_Result_t ESP_Cmd(const char *cmd, uint32_t timeout_ms)
{
  uint8_t attempt;

  for (attempt = 0U; attempt < 3U; attempt++)
  {
    uint32_t start;

    ESP_Poll();                     /* 먼저 밀린 수신을 처리 */

    s_resp_len   = 0U;
    s_resp[0]    = '\0';
    s_cmd_result = RES_NONE;
    s_prompt     = false;
    s_cmd_active = true;

#if (ESP_AT_DEBUG == 1U)
    DBG_Printf("  -> %s\r\n", cmd);
#endif
    UART_Write((const uint8_t *)cmd, (uint16_t)strlen(cmd));
    UART_Write((const uint8_t *)"\r\n", 2U);

    start = HAL_GetTick();
    while (s_cmd_result == RES_NONE)
    {
      ESP_Poll();
      if ((HAL_GetTick() - start) >= timeout_ms)
      {
        s_cmd_active = false;
        LOG("ESP32: timeout waiting for response to '%s'", cmd);
        return ESP_TIMEOUT;
      }
    }
    s_cmd_active = false;

    if (s_cmd_result == RES_OK)
    {
      return ESP_OK;
    }
    if (s_cmd_result == RES_ERROR)
    {
      return ESP_ERROR;
    }
    /* RES_BUSY: 조금 기다렸다 재전송 */
    ESP_DelayPoll(200U);
  }
  return ESP_ERROR;
}

ESP_Result_t ESP_CmdFmt(uint32_t timeout_ms, const char *fmt, ...)
{
  static char buf[ESP_LINE_BUF_SIZE];
  va_list ap;

  va_start(ap, fmt);
  (void)vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  return ESP_Cmd(buf, timeout_ms);
}

bool ESP_WaitFor(const char *token, uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();

  s_wait_token = token;
  s_wait_hit   = false;

  while (!s_wait_hit)
  {
    ESP_Poll();
    if ((HAL_GetTick() - start) >= timeout_ms)
    {
      s_wait_token = NULL;
      return false;
    }
  }
  s_wait_token = NULL;
  return true;
}

const char *ESP_Response(void)
{
  return s_resp;
}

bool ESP_ResponseContains(const char *key)
{
  return strstr(s_resp, key) != NULL;
}

bool ESP_GetQuoted(const char *key, char *out, size_t out_len)
{
  const char *p = strstr(s_resp, key);
  const char *q;
  size_t n;

  if ((p == NULL) || (out == NULL) || (out_len == 0U))
  {
    return false;
  }
  p = strchr(p + strlen(key), '"');
  if (p == NULL)
  {
    return false;
  }
  p++;
  q = strchr(p, '"');
  if (q == NULL)
  {
    return false;
  }
  n = (size_t)(q - p);
  if (n >= out_len)
  {
    n = out_len - 1U;
  }
  memcpy(out, p, n);
  out[n] = '\0';
  return true;
}

bool ESP_GetInt(const char *key, int32_t *out)
{
  const char *p = strstr(s_resp, key);

  if ((p == NULL) || (out == NULL))
  {
    return false;
  }
  *out = (int32_t)strtol(p + strlen(key), NULL, 10);
  return true;
}

uint32_t ESP_TakeEvents(void)
{
  uint32_t ev = s_events;
  s_events = 0U;
  return ev;
}

uint32_t ESP_PeekEvents(void)
{
  return s_events;
}

/**
  * @brief  AT+CIPSEND=<len> 으로 서버에 데이터를 보낸다.
  *
  *   -> AT+CIPSEND=<len>
  *   <- OK
  *   <- >                (프롬프트, 개행 없음)
  *   -> <len 바이트 데이터>
  *   <- Recv <len> bytes
  *   <- SEND OK  /  SEND FAIL
  */
ESP_Result_t ESP_SendData(const uint8_t *data, uint16_t len)
{
  char     cmd[32];
  uint32_t start;

  if ((data == NULL) || (len == 0U))
  {
    return ESP_ERROR;
  }

  ESP_Poll();

  (void)snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)len);

  s_resp_len   = 0U;
  s_resp[0]    = '\0';
  s_cmd_result = RES_NONE;
  s_prompt     = false;
  s_cmd_active = true;

#if (ESP_AT_DEBUG == 1U)
  DBG_Printf("  -> %s\r\n", cmd);
#endif
  UART_Write((const uint8_t *)cmd, (uint16_t)strlen(cmd));
  UART_Write((const uint8_t *)"\r\n", 2U);

  /* 1) '>' 프롬프트 대기 (그 전에 "OK" 가 먼저 온다) */
  start = HAL_GetTick();
  while (!s_prompt)
  {
    ESP_Poll();
    if ((s_cmd_result == RES_ERROR) || (s_cmd_result == RES_BUSY))
    {
      s_cmd_active = false;
      LOG("ESP32: CIPSEND rejected");
      return ESP_ERROR;
    }
    if ((HAL_GetTick() - start) >= 3000U)
    {
      s_cmd_active = false;
      LOG("ESP32: no '>' prompt");
      return ESP_TIMEOUT;
    }
  }

  /* 2) 데이터 전송 후 SEND OK / SEND FAIL 대기 */
  s_cmd_result = RES_NONE;
#if (ESP_AT_DEBUG == 1U)
  DBG_Printf("  -> <%u bytes>\r\n", (unsigned)len);
#endif
  UART_Write(data, len);

  start = HAL_GetTick();
  while (s_cmd_result == RES_NONE)
  {
    ESP_Poll();
    if ((HAL_GetTick() - start) >= 10000U)
    {
      s_cmd_active = false;
      LOG("ESP32: no SEND OK");
      return ESP_TIMEOUT;
    }
  }
  s_cmd_active = false;

  return (s_cmd_result == RES_OK) ? ESP_OK : ESP_ERROR;
}

uint32_t ESP_OverrunCount(void)
{
  return s_overrun_count;
}
