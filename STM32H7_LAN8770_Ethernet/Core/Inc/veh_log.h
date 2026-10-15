/**
  ******************************************************************************
  * @file    veh_log.h
  * @brief   차량 이더넷 모듈 공용 로그 (초당 줄 수 제한)
  ******************************************************************************
  */

#ifndef VEH_LOG_H
#define VEH_LOG_H

#include <stdint.h>
#include <stdio.h>
#include "lan8770_conf.h"

/* 이번 초에 아직 출력 여유가 있으면 1. 넘치면 버린 줄 수를 세고 0 */
uint8_t veh_log_allow(void);

/* 버린 줄 수를 돌려주고 0 으로 초기화 */
uint32_t veh_log_take_dropped(void);

/* µs 시계 (DWT 사이클 카운터). 메인 루프에서 10 초 안에 한 번 이상 호출돼야 함 */
void     veh_time_init(void);
uint32_t veh_time_us(void);

#if APP_USE_UART_LOG
/* 반드시 출력 (상태 요약 등) */
#define VLOG(...)        printf(__VA_ARGS__)
/* 트래픽에 따라 많이 나올 수 있는 로그 (제한 적용) */
#define VLOG_RL(...)     do { if (veh_log_allow()) { printf(__VA_ARGS__); } } while (0)
#else
#define VLOG(...)        ((void)0)
#define VLOG_RL(...)     ((void)0)
#endif

#endif /* VEH_LOG_H */
