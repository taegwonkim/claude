/**
  ******************************************************************************
  * @file    data_router.h
  * @brief   qSample → ASCII 라인 포맷 → RS485 / USB CDC / WiFi 분배
  ******************************************************************************
  */
#ifndef DATA_ROUTER_H
#define DATA_ROUTER_H

#include "app_types.h"

void router_task(void *arg);

/* 샘플을 출력 라인으로 포맷. 반환: 문자열 길이 */
uint16_t router_format(const sd_sample_t *s, char *dst, uint16_t dst_sz);

/* 임의 문자열을 각 경로로 보낸다 (CLI 응답/알림 공용) */
void router_send_usb(const char *s, uint16_t len);
void router_send_uart(const char *s, uint16_t len);

#endif /* DATA_ROUTER_H */
