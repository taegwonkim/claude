#include "rtc_wakeup.h"
#include "reset_config.h"
#include "pc_comm.h"
#include "app_config.h"
#include <stdio.h>

#define RESET_COUNT_BKP_REG RTC_BKP_DR0

static uint32_t s_periodSec;

/* ck_spre(1Hz, CubeMX RTC Calendar 설정에서 서브초 프리스케일러를 1Hz로 맞춰둔다고 가정) 기준
 * 16bit 모드에서는 (WUTR+1)초가 실제 주기이므로 seconds-1을 넣는다. */
static void ArmWakeupTimer(uint32_t seconds)
{
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, seconds - 1U, RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0U);
}

void RtcWakeup_Init(void)
{
    ResetConfig_t cfg;
    uint32_t count;
    char buf[32];

    if (!ResetConfig_Load(&cfg)) {
        ResetConfig_SetDefaults(&cfg);
    }
    s_periodSec = cfg.period_sec;

    /* RTC 백업 레지스터는 RTC 도메인(백업 도메인)에 있어 NVIC_SystemReset()의 영향을 받지
     * 않으므로, 전원이 유지되는 한 리셋을 거듭해도 누적된다. */
    count = HAL_RTCEx_BKUPRead(&hrtc, RESET_COUNT_BKP_REG) + 1U;
    HAL_RTCEx_BKUPWrite(&hrtc, RESET_COUNT_BKP_REG, count);

    snprintf(buf, sizeof(buf), "RESET_COUNT,%lu", (unsigned long)count);
    PcComm_BroadcastFrame(buf);

    ArmWakeupTimer(s_periodSec);
}

uint32_t RtcWakeup_GetPeriodSec(void)
{
    return s_periodSec;
}

bool RtcWakeup_SetPeriodSec(uint32_t seconds)
{
    ResetConfig_t cfg;

    if (seconds < APP_RESET_MIN_PERIOD_SEC || seconds > APP_RESET_MAX_PERIOD_SEC) {
        return false;
    }

    ResetConfig_SetDefaults(&cfg); /* magic/version 채움 */
    cfg.period_sec = seconds;
    if (!ResetConfig_Save(&cfg)) {
        return false;
    }

    s_periodSec = seconds;
    ArmWakeupTimer(seconds);
    return true;
}

void RtcWakeup_OnWakeupTimerEvent(void)
{
    NVIC_SystemReset(); /* 반환하지 않음 */
}
