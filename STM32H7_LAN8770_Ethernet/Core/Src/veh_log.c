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
