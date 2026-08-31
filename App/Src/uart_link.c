/**
  ******************************************************************************
  * @file    uart_link.c
  * @brief   UART DMA 링버퍼 링크 구현
  ******************************************************************************
  */
#include "uart_link.h"

/* CubeMX 생성 핸들 */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

/* DMA 순환 버퍼 : 반드시 정적 (태스크 스택 금지) */
static uint8_t s_u1_rx[SD_U1_RX_BUF_SZ];
static uint8_t s_u2_rx[SD_U2_RX_BUF_SZ];
static uint8_t s_u3_rx[SD_U3_RX_BUF_SZ];

uart_link_t g_lnkEsp;
uart_link_t g_lnkFpga;
uart_link_t g_lnkPc;

/* ---------------------------------------------------------------------------
 * 내부 헬퍼
 * -------------------------------------------------------------------------*/
static inline uint16_t link_head(const uart_link_t *lnk)
{
  /* DMA 잔여 카운트로 현재 쓰기 위치 계산 */
  uint16_t remain = (uint16_t)__HAL_DMA_GET_COUNTER(lnk->huart->hdmarx);
  return (uint16_t)(lnk->rx_size - remain);
}

static sd_res_t link_init_one(uart_link_t *lnk, UART_HandleTypeDef *huart,
                              uint8_t *buf, uint16_t size, const char *name)
{
  osSemaphoreAttr_t sa;
  osMutexAttr_t     ma;

  memset(lnk, 0, sizeof(*lnk));
  lnk->huart   = huart;
  lnk->rx_buf  = buf;
  lnk->rx_size = size;
  lnk->rd      = 0u;

  memset(&sa, 0, sizeof(sa));
  sa.name  = name;
  lnk->rx_sig = osSemaphoreNew(8u, 0u, &sa);          /* counting  */
  lnk->tx_done = osSemaphoreNew(1u, 1u, &sa);         /* binary, 초기 available */

  memset(&ma, 0, sizeof(ma));
  ma.name  = name;
  ma.attr_bits = osMutexPrioInherit;
  lnk->tx_mtx = osMutexNew(&ma);

  if ((lnk->rx_sig == NULL) || (lnk->tx_done == NULL) || (lnk->tx_mtx == NULL))
  {
    return SD_ERR;
  }

  /* Idle 라인 검출 + 순환 DMA 수신 시작 */
  if (HAL_UARTEx_ReceiveToIdle_DMA(huart, buf, size) != HAL_OK)
  {
    return SD_ERR_HW;
  }

  /* Half-transfer 인터럽트까지 쓰면 이벤트가 잦아지므로 그대로 둔다.
   * (유실 방지 목적이므로 오히려 활성 상태가 유리하다) */
  return SD_OK;
}

/* ---------------------------------------------------------------------------
 * 공개 API
 * -------------------------------------------------------------------------*/
sd_res_t uartlink_init_all(void)
{
  sd_res_t r;

  r = link_init_one(&g_lnkEsp,  &huart1, s_u1_rx, SD_U1_RX_BUF_SZ, "lnkEsp");
  if (r != SD_OK) { return r; }

  r = link_init_one(&g_lnkFpga, &huart2, s_u2_rx, SD_U2_RX_BUF_SZ, "lnkFpga");
  if (r != SD_OK) { return r; }

  r = link_init_one(&g_lnkPc,   &huart3, s_u3_rx, SD_U3_RX_BUF_SZ, "lnkPc");
  return r;
}

uint16_t uartlink_avail(uart_link_t *lnk)
{
  uint16_t head = link_head(lnk);
  uint16_t rd   = lnk->rd;

  return (head >= rd) ? (uint16_t)(head - rd)
                      : (uint16_t)(lnk->rx_size - rd + head);
}

void uartlink_flush_rx(uart_link_t *lnk)
{
  lnk->rd = link_head(lnk);
  /* 세마포어 토큰도 비운다 */
  while (osSemaphoreAcquire(lnk->rx_sig, 0u) == osOK) { }
}

sd_res_t uartlink_getc(uart_link_t *lnk, uint8_t *b, uint32_t timeout_ms)
{
  uint32_t start = osKernelGetTickCount();

  for (;;)
  {
    if (uartlink_avail(lnk) > 0u)
    {
      *b = lnk->rx_buf[lnk->rd];
      lnk->rd = (uint16_t)((lnk->rd + 1u) % lnk->rx_size);
      return SD_OK;
    }

    uint32_t elapsed = osKernelGetTickCount() - start;
    if (elapsed >= timeout_ms)
    {
      return SD_ERR_TMO;
    }

    /* RX 이벤트를 기다리되, 이벤트 없이도 최대 5ms 마다 재확인한다.
     * (DMA 는 이벤트 없이도 데이터를 채우므로 폴링 백업이 필요) */
    uint32_t wait = timeout_ms - elapsed;
    if (wait > 5u) { wait = 5u; }
    (void)osSemaphoreAcquire(lnk->rx_sig, wait);
  }
}

sd_res_t uartlink_read(uart_link_t *lnk, uint8_t *dst, uint16_t len, uint32_t timeout_ms)
{
  uint32_t start = osKernelGetTickCount();

  for (uint16_t i = 0; i < len; i++)
  {
    uint32_t elapsed = osKernelGetTickCount() - start;
    if (elapsed >= timeout_ms)
    {
      return SD_ERR_TMO;
    }
    if (uartlink_getc(lnk, &dst[i], timeout_ms - elapsed) != SD_OK)
    {
      return SD_ERR_TMO;
    }
  }
  return SD_OK;
}

int32_t uartlink_readline(uart_link_t *lnk, char *dst, uint16_t dst_sz, uint32_t timeout_ms)
{
  uint32_t start = osKernelGetTickCount();
  uint16_t idx   = 0u;
  uint8_t  c;

  if ((dst == NULL) || (dst_sz < 2u)) { return SD_ERR_PARAM; }

  for (;;)
  {
    uint32_t elapsed = osKernelGetTickCount() - start;
    if (elapsed >= timeout_ms)
    {
      dst[idx] = '\0';
      return (int32_t)SD_ERR_TMO;
    }

    if (uartlink_getc(lnk, &c, timeout_ms - elapsed) != SD_OK)
    {
      dst[idx] = '\0';
      return (int32_t)SD_ERR_TMO;
    }

    if (c == '\n')
    {
      dst[idx] = '\0';
      return (int32_t)idx;
    }
    if (c == '\r')
    {
      continue;                     /* CR 무시 */
    }
    if (idx < (dst_sz - 1u))
    {
      dst[idx++] = (char)c;
    }
    else
    {
      /* 라인이 너무 길다 : 잘라서 반환 (다음 '\n' 까지는 다음 호출에서 소비) */
      dst[idx] = '\0';
      return (int32_t)idx;
    }
  }
}

sd_res_t uartlink_write(uart_link_t *lnk, const uint8_t *src, uint16_t len, uint32_t timeout_ms)
{
  sd_res_t res = SD_OK;

  if ((src == NULL) || (len == 0u)) { return SD_ERR_PARAM; }

  if (osMutexAcquire(lnk->tx_mtx, timeout_ms) != osOK)
  {
    return SD_ERR_BUSY;
  }

  /* 이전 전송의 잔여 토큰 확보 (첫 전송은 초기값 1 로 즉시 통과) */
  if (osSemaphoreAcquire(lnk->tx_done, timeout_ms) != osOK)
  {
    osMutexRelease(lnk->tx_mtx);
    return SD_ERR_TMO;
  }

#if SD_RS485_SW_DE
  if (lnk == &g_lnkPc)
  {
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_SET);
  }
#endif

  if (HAL_UART_Transmit_DMA(lnk->huart, (uint8_t *)src, len) != HAL_OK)
  {
    osSemaphoreRelease(lnk->tx_done);
    osMutexRelease(lnk->tx_mtx);
    return SD_ERR_HW;
  }

  /* 완료 대기 후 토큰을 되돌려 놓는다 */
  if (osSemaphoreAcquire(lnk->tx_done, timeout_ms) != osOK)
  {
    HAL_UART_AbortTransmit(lnk->huart);
    res = SD_ERR_TMO;
  }
  osSemaphoreRelease(lnk->tx_done);

#if SD_RS485_SW_DE
  if (lnk == &g_lnkPc)
  {
    /* 마지막 바이트가 시프트 레지스터를 빠져나갈 때까지 대기 */
    uint32_t guard = osKernelGetTickCount() + 10u;
    while ((__HAL_UART_GET_FLAG(lnk->huart, UART_FLAG_TC) == RESET) &&
           (osKernelGetTickCount() < guard))
    {
      osThreadYield();
    }
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);
  }
#endif

  osMutexRelease(lnk->tx_mtx);
  return res;
}

sd_res_t uartlink_puts(uart_link_t *lnk, const char *s, uint32_t timeout_ms)
{
  return uartlink_write(lnk, (const uint8_t *)s, (uint16_t)strlen(s), timeout_ms);
}

/* ---------------------------------------------------------------------------
 * HAL 콜백 라우팅 (hooks.c 에서 호출)
 * -------------------------------------------------------------------------*/
static uart_link_t *link_of(const UART_HandleTypeDef *huart)
{
  if (huart == g_lnkEsp.huart)  { return &g_lnkEsp;  }
  if (huart == g_lnkFpga.huart) { return &g_lnkFpga; }
  if (huart == g_lnkPc.huart)   { return &g_lnkPc;   }
  return NULL;
}

void uartlink_on_rx_event(UART_HandleTypeDef *huart, uint16_t size)
{
  uart_link_t *lnk = link_of(huart);
  (void)size;

  if ((lnk != NULL) && (lnk->rx_sig != NULL))
  {
    (void)osSemaphoreRelease(lnk->rx_sig);   /* 가득 차면 osErrorResource, 무시 */
  }
}

void uartlink_on_tx_done(UART_HandleTypeDef *huart)
{
  uart_link_t *lnk = link_of(huart);

  if ((lnk != NULL) && (lnk->tx_done != NULL))
  {
    (void)osSemaphoreRelease(lnk->tx_done);
  }
}

void uartlink_on_error(UART_HandleTypeDef *huart)
{
  uart_link_t *lnk = link_of(huart);

  if (lnk == NULL) { return; }

  lnk->ovf_cnt++;

  /* ORE/FE/NE 등으로 DMA 수신이 멈추면 재시작한다 */
  if (huart->RxState == HAL_UART_STATE_READY)
  {
    lnk->rd = 0u;
    (void)HAL_UARTEx_ReceiveToIdle_DMA(huart, lnk->rx_buf, lnk->rx_size);
  }
}
