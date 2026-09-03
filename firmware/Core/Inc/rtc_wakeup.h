/**
 * rtc_wakeup.h
 *
 * RTC Wakeup Timer를 이용한 주기적 자가 리셋. 설정된 주기(초)마다 RTC Wakeup Timer 인터럽트가
 * 발생하면 그 즉시 NVIC_SystemReset()으로 MCU를 리셋하고, 이 동작을 반복한다(docs/프로토콜_명세.md §6).
 * 리셋 횟수는 RTC 백업 레지스터(RTC_BKP_DR0, RTC 도메인이라 시스템 리셋에는 영향받지 않음)에
 * 누적 저장하고, 부팅 시 그 값을 증가시킨 뒤 PC(USART3+USB)로 RESET_COUNT,<count> 프레임을
 * 1회 브로드캐스트한다.
 *
 * CubeMX가 RTC(Calendar, ck_spre=1Hz) + Wakeup Timer 인터럽트(NVIC RTC_WKUP_IRQn Enable)를
 * 설정해 hrtc를 생성해두어야 한다(firmware/docs/CubeMX_설정가이드.md 참고).
 */
#ifndef RTC_WAKEUP_H
#define RTC_WAKEUP_H

#include <stdint.h>
#include <stdbool.h>
#include "rtc.h" /* CubeMX 생성: hrtc */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 플래시에서 주기 설정을 로드(없으면 기본값)하고, 부팅 리셋 카운터를 증가시켜
 *        PC로 RESET_COUNT,<count>를 1회 브로드캐스트한 뒤 Wakeup Timer를 그 주기로 무장한다.
 *        PcComm_Init() 이후(USART3/USB가 준비된 뒤) 호출할 것.
 */
void RtcWakeup_Init(void);

/** 현재 적용 중인 Wakeup Timer 주기(초)를 반환한다 (RESET_R_ALL 응답용). */
uint32_t RtcWakeup_GetPeriodSec(void);

/**
 * @brief 주기(초)를 플래시에 저장하고 즉시 Wakeup Timer를 그 값으로 다시 무장한다
 *        (RESET_W_ALL 처리용). APP_RESET_MIN_PERIOD_SEC..APP_RESET_MAX_PERIOD_SEC 범위 밖이거나
 *        플래시 쓰기에 실패하면 false를 반환하고 기존 설정을 그대로 유지한다.
 */
bool RtcWakeup_SetPeriodSec(uint32_t seconds);

/** HAL_RTCEx_WakeUpTimerEventCallback()에서 호출 (app_it_callbacks.c). 반환하지 않는다(리셋). */
void RtcWakeup_OnWakeupTimerEvent(void);

#ifdef __cplusplus
}
#endif

#endif /* RTC_WAKEUP_H */
