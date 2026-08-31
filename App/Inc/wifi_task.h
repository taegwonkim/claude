#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include "app_types.h"

typedef enum
{
  WIFI_ST_RESET = 0,
  WIFI_ST_INIT,
  WIFI_ST_NETCFG,
  WIFI_ST_JOIN,
  WIFI_ST_TCP,
  WIFI_ST_ONLINE,
  WIFI_ST_BACKOFF
} wifi_state_t;

extern volatile wifi_state_t g_wifi_state;

const char *wifi_state_str(void);
void        wifi_task(void *arg);

/* 설정 변경 등으로 상태머신을 처음부터 다시 돌린다 */
void        wifi_request_reconnect(void);

#endif /* WIFI_TASK_H */
