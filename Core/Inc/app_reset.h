/**
  ******************************************************************************
  * @file    app_reset.h
  * @brief   RTC WakeUp Timer 기반 주기적 소프트웨어 리셋 + RS485 보고
  ******************************************************************************
  */
#ifndef __APP_RESET_H
#define __APP_RESET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
  bool     cold_boot;     /* true = 전원을 새로 넣었다(백업 도메인 소실)    */
  bool     by_rtc;        /* true = 직전 리셋이 우리가 건 RTC 리셋이었다    */
  uint32_t csr;           /* 부팅 시점의 RCC->CSR 스냅샷                    */

  uint32_t boot_count;    /* 전원 인가 후 부팅 횟수    (백업 레지스터)      */
  uint32_t reset_count;   /* 전원 인가 후 RTC 리셋 횟수(백업 레지스터)      */
  uint32_t run_sec;       /* 전원 인가 후 누적 동작 시간[s] (이전 세션 합)  */

  uint32_t power_cycle;   /* 전원 인가 누적 횟수   (Flash, 전원 off 에도 유지) */
  uint32_t total_reset;   /* RTC 리셋 누적 횟수    (Flash, 전원 off 에도 유지) */

  uint32_t unit;          /* RESET_UNIT_MINUTE / RESET_UNIT_HOUR            */
  uint32_t value;         /* 단위 배수                                       */
  uint32_t period_sec;    /* 실제 주기[초]                                   */
} AppReset_Ctx_t;

/* HAL_Init() 직후, 다른 어떤 것보다 먼저 호출할 것 (리셋 플래그는 1회성) */
void AppReset_CaptureCause(void);

/* MX_RTC_Init() 이후 호출 : 백업/Flash 카운터 정리 */
void AppReset_Init(void);

/* 부팅 배너를 RS485 로 전송 */
void AppReset_PrintBanner(void);

/* WakeUp Timer 기동 */
void AppReset_StartTimer(void);

/* main 루프에서 계속 호출 : 리셋 처리 / 하트비트 / LED / RS485 명령 */
void AppReset_Task(void);

/* 즉시 소프트웨어 리셋 (돌아오지 않음) */
void AppReset_DoReset(const char *reason);

const AppReset_Ctx_t *AppReset_GetCtx(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_RESET_H */
