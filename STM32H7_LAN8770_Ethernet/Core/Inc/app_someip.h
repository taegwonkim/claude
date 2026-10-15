/**
  ******************************************************************************
  * @file    app_someip.h
  * @brief   SOME/IP 수신 + SOME/IP-SD 서비스 발견 (lwIP UDP)
  ******************************************************************************
  */

#ifndef APP_SOMEIP_H
#define APP_SOMEIP_H

#include <stdint.h>

typedef struct
{
  uint32_t rx_datagrams;
  uint32_t rx_messages;
  uint32_t rx_sd_messages;
  uint32_t rx_errors;
  uint32_t tx_find;
} app_someip_counters_t;

void APP_SOMEIP_Init(void);
void APP_SOMEIP_Process(void);
void APP_SOMEIP_PrintTable(void);
uint32_t APP_SOMEIP_ServiceCount(void);
const app_someip_counters_t *APP_SOMEIP_Counters(void);

#endif /* APP_SOMEIP_H */
