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

typedef struct
{
  uint16_t service_id;
  uint16_t instance_id;
  uint8_t  major;
  uint32_t minor;
  uint32_t ttl;
  uint8_t  has_ep;
  uint8_t  ep_ip[4];
  uint8_t  ep_proto;
  uint16_t ep_port;
  uint32_t from_ip;      /* 0xaabbccdd */
  uint32_t offers;
} app_someip_service_t;

void APP_SOMEIP_Init(void);
/* index 번째 발견된 서비스 (0 = 있음, -1 = 없음) */
int  APP_SOMEIP_GetService(uint32_t index, app_someip_service_t *out);
void APP_SOMEIP_Process(void);
void APP_SOMEIP_PrintTable(void);
uint32_t APP_SOMEIP_ServiceCount(void);
const app_someip_counters_t *APP_SOMEIP_Counters(void);

#endif /* APP_SOMEIP_H */
