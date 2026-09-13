/**
  ******************************************************************************
  * @file    flash_counter.h
  * @brief   전원을 꺼도 남는 누적 카운터 (내부 Flash 마지막 페이지 사용)
  *
  *  이 보드는 VBAT 가 MCU 전원과 함께 on/off 되므로 전원을 끄면
  *  TAMP 백업 레지스터가 지워진다. "전원 사이클을 넘어서 남아야 하는 값"은
  *  여기(내부 Flash)에 저장한다.
  ******************************************************************************
  */
#ifndef __FLASH_COUNTER_H
#define __FLASH_COUNTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
  uint32_t total_reset;    /* 지금까지의 RTC 소프트웨어 리셋 누적 횟수 */
  uint32_t power_cycle;    /* 전원을 넣은 횟수 (콜드 부트 횟수)        */
} FlashCounter_t;

#if (USE_FLASH_COUNTER == 1U)

/* 마지막으로 저장된 값을 읽는다. 한 번도 저장된 적 없으면 0/0 을 돌려주고 false */
bool FlashCounter_Read(FlashCounter_t *out);

/* 값을 기록한다. 페이지가 꽉 차면 자동으로 지우고 처음부터 다시 쓴다. */
bool FlashCounter_Write(const FlashCounter_t *in);

/* 저장 페이지를 통째로 지운다 (카운터 초기화) */
bool FlashCounter_Erase(void);

/* 저장에 쓰는 페이지의 시작 주소 (로그 출력용) */
uint32_t FlashCounter_GetPageAddr(void);

#else

#define FlashCounter_Read(o)      (false)
#define FlashCounter_Write(i)     (true)
#define FlashCounter_Erase()      (true)
#define FlashCounter_GetPageAddr() (0U)

#endif /* USE_FLASH_COUNTER */

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_COUNTER_H */
