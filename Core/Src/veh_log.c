/**
  ******************************************************************************
  * @file    veh_log.c
  * @brief   초당 로그 줄 수 제한
  ******************************************************************************
  */

#include "main.h"
#include "veh_log.h"
#include "vehicle_net_conf.h"

static uint32_t window_tick;
static uint32_t lines_in_window;
static uint32_t dropped;

uint8_t veh_log_allow(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - window_tick) >= 1000U)
  {
    window_tick     = now;
    lines_in_window = 0U;
  }
  if (lines_in_window < APP_LOG_LINES_PER_SEC)
  {
    lines_in_window++;
    return 1U;
  }
  dropped++;
  return 0U;
}

uint32_t veh_log_take_dropped(void)
{
  uint32_t d = dropped;
  dropped = 0U;
  return d;
}

/* ---------------------------------------------------------------------------
 *  µs 시계 : DWT->CYCCNT (400 MHz 에서 약 10.7 초마다 한 바퀴) 를 누적
 * ------------------------------------------------------------------------ */
static uint32_t last_cyc;
static uint32_t acc_us;
static uint32_t rem_cyc;

void veh_time_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->LAR = 0xC5ACCE55U;            /* Cortex-M7 : DWT 잠금 해제 */
  DWT->CYCCNT = 0U;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
  last_cyc = 0U;
  acc_us   = 0U;
  rem_cyc  = 0U;
}

uint32_t veh_time_us(void)
{
  uint32_t mhz = SystemCoreClock / 1000000U;
  uint32_t cyc = DWT->CYCCNT;
  uint32_t total;

  if (mhz == 0U)
  {
    return HAL_GetTick() * 1000U;
  }
  total    = (cyc - last_cyc) + rem_cyc;
  last_cyc = cyc;
  acc_us  += total / mhz;
  rem_cyc  = total % mhz;
  return acc_us;
}
