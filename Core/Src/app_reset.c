/**
  ******************************************************************************
  * @file    app_reset.c
  * @brief   RTC WakeUp Timer 로 일정 시간마다 소프트웨어 리셋하고,
  *          리셋 사실과 횟수를 USART3(RS485) 와 USB CDC 로 PC 에 보고한다.
  *
  *  [ VBAT 가 VDD 와 함께 켜지고 꺼지는 보드에서의 동작 ]
  *
  *   이 보드는 VBAT 핀을 MCU 전원과 공용으로 쓴다. 그래서
  *     · 소프트웨어 리셋(NVIC_SystemReset)   -> 백업 도메인 전원 유지
  *                                             RTC 계속 동작, 백업 레지스터 보존
  *     · 전원 OFF -> ON                      -> 백업 도메인도 같이 꺼짐
  *                                             RTC 달력 리셋, 백업 레지스터 = 0
  *
  *   즉 "리셋 횟수"는 두 가지 의미가 생긴다.
  *     (a) 이번에 전원을 넣은 뒤의 리셋 횟수  -> 백업 레지스터로 충분
  *     (b) 장비 설치 후 지금까지의 총 리셋 횟수 -> 백업 레지스터로는 불가능,
  *                                              내부 Flash 에 저장해야 한다
  *   이 파일은 두 값을 모두 관리하고 둘 다 RS485 로 보고한다.
  *
  *   콜드/웜 부트 판별은 백업 레지스터 BKP_REG_MAGIC 의 매직값으로 한다.
  *   전원이 끊기면 이 레지스터가 0 이 되므로, 매직값이 없으면 "전원을 새로
  *   넣은 것"이다.
  *
  *   그리고 "내가 건 RTC 리셋"인지 "디버거/다른 이유의 소프트 리셋"인지
  *   구분하기 위해, 리셋 직전에 BKP_REG_PENDING 에 표식을 남긴다.
  ******************************************************************************
  */
#include "app_reset.h"
#include "comm.h"
#include "flash_counter.h"
#include <stdio.h>
#include <string.h>

extern RTC_HandleTypeDef hrtc;

/* ------------------------------------------------------------------------- */
static AppReset_Ctx_t   s_ctx;
static volatile uint8_t s_wakeup_flag = 0U;   /* RTC IRQ -> main 루프 신호 */
static uint32_t         s_remain_sec  = 0U;   /* 아직 장전하지 않은 잔여 초 */
static uint32_t         s_base_tick   = 0U;   /* 주기 계산 기준 시각        */
static uint32_t         s_tick_led    = 0U;
static uint32_t         s_tick_beat   = 0U;

/* ------------------------------------------------------------------------- */
static inline uint32_t BKP_Read(uint32_t reg)
{
  return HAL_RTCEx_BKUPRead(&hrtc, reg);
}

static inline void BKP_Write(uint32_t reg, uint32_t val)
{
  HAL_RTCEx_BKUPWrite(&hrtc, reg, val);
}

/* 부팅 후 경과 시간[초] */
static uint32_t AppReset_ElapsedSec(void)
{
  return (HAL_GetTick() - s_base_tick) / 1000U;
}

/* 다음 리셋까지 남은 시간[초] */
static uint32_t AppReset_RemainSec(void)
{
  uint32_t e = AppReset_ElapsedSec();
  return (e >= s_ctx.period_sec) ? 0U : (s_ctx.period_sec - e);
}

static void AppReset_CalcPeriod(void)
{
  if (s_ctx.value == 0U)
  {
    s_ctx.value = 1U;
  }
  s_ctx.period_sec = (s_ctx.unit == RESET_UNIT_HOUR)
                   ? (s_ctx.value * 3600U)
                   : (s_ctx.value * 60U);
}

static const char *AppReset_UnitStr(void)
{
  return (s_ctx.unit == RESET_UNIT_HOUR) ? "hour" : "min";
}

/* 초 -> "hh:mm:ss" */
static void AppReset_FormatHMS(uint32_t sec, char *buf, size_t size)
{
  (void)snprintf(buf, size, "%02lu:%02lu:%02lu",
                 (unsigned long)(sec / 3600U),
                 (unsigned long)((sec % 3600U) / 60U),
                 (unsigned long)(sec % 60U));
}

/* ===========================================================================
 *  1. 리셋 원인 캡처 (HAL_Init 직후 딱 한 번)
 * ========================================================================= */
void AppReset_CaptureCause(void)
{
  s_ctx.csr = RCC->CSR;
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

/* ===========================================================================
 *  2. 카운터 정리 (MX_RTC_Init 이후)
 * ========================================================================= */
void AppReset_Init(void)
{
  FlashCounter_t fc = {0};

  /* 백업 도메인(RTC/TAMP 레지스터) 쓰기 허용 */
  HAL_PWR_EnableBkUpAccess();

#if (USE_FLASH_COUNTER == 1U)
  /* 전원 off 에도 남는 누적값은 Flash 에서 읽어온다.
     리셋 직전(AppReset_DoReset)에 이미 +1 해서 기록했으므로
     여기서 또 더하면 안 된다. */
  (void)FlashCounter_Read(&fc);
  s_ctx.total_reset = fc.total_reset;
  s_ctx.power_cycle = fc.power_cycle;
#else
  (void)fc;
  s_ctx.total_reset = 0U;
  s_ctx.power_cycle = 0U;
#endif

  if (BKP_Read(BKP_REG_MAGIC) != BKP_MAGIC_VALUE)
  {
    /* ---- COLD BOOT : 전원을 새로 넣었다 (VBAT 도 같이 꺼졌었다) ---------- */
    s_ctx.cold_boot   = true;
    s_ctx.by_rtc      = false;
    s_ctx.boot_count  = 1U;
    s_ctx.reset_count = 0U;
    s_ctx.run_sec     = 0U;
    s_ctx.unit        = RESET_PERIOD_UNIT;    /* 컴파일 타임 기본값으로 복귀 */
    s_ctx.value       = RESET_PERIOD_VALUE;

    BKP_Write(BKP_REG_MAGIC,   BKP_MAGIC_VALUE);
    BKP_Write(BKP_REG_PENDING, 0U);

#if (USE_FLASH_COUNTER == 1U)
    /* 전원 인가 횟수는 전원이 꺼져도 남아야 하므로 Flash 에 누적 */
    s_ctx.power_cycle++;
    fc.total_reset = s_ctx.total_reset;
    fc.power_cycle = s_ctx.power_cycle;
    (void)FlashCounter_Write(&fc);
#endif
  }
  else
  {
    /* ---- WARM BOOT : 전원은 계속 들어와 있었고 리셋만 걸렸다 ------------- */
    s_ctx.cold_boot   = false;
    s_ctx.boot_count  = BKP_Read(BKP_REG_BOOT_COUNT) + 1U;
    s_ctx.reset_count = BKP_Read(BKP_REG_RESET_COUNT);
    s_ctx.run_sec     = BKP_Read(BKP_REG_RUN_SEC);
    s_ctx.unit        = BKP_Read(BKP_REG_UNIT);
    s_ctx.value       = BKP_Read(BKP_REG_VALUE);

    if (s_ctx.unit > RESET_UNIT_HOUR)
    {
      s_ctx.unit = RESET_PERIOD_UNIT;
    }
    if ((s_ctx.value == 0U) || (s_ctx.value > 1000U))
    {
      s_ctx.value = RESET_PERIOD_VALUE;
    }

    /* 우리가 건 리셋인지 표식으로 확인 (디버거 리셋 등과 구분) */
    if (BKP_Read(BKP_REG_PENDING) == BKP_PENDING_VALUE)
    {
      s_ctx.by_rtc = true;
      s_ctx.reset_count++;
      BKP_Write(BKP_REG_PENDING, 0U);
    }
    else
    {
      s_ctx.by_rtc = false;
    }
  }

  AppReset_CalcPeriod();

#if (USE_FLASH_COUNTER == 0U)
  /* Flash 를 안 쓰면 "누적 횟수"를 보관할 곳이 없다.
     전원 인가 후 횟수와 같은 값으로 맞춰 둔다(전원을 끄면 0 으로 돌아간다). */
  s_ctx.total_reset = s_ctx.reset_count;
#endif

  BKP_Write(BKP_REG_BOOT_COUNT,  s_ctx.boot_count);
  BKP_Write(BKP_REG_RESET_COUNT, s_ctx.reset_count);
  BKP_Write(BKP_REG_RUN_SEC,     s_ctx.run_sec);
  BKP_Write(BKP_REG_UNIT,        s_ctx.unit);
  BKP_Write(BKP_REG_VALUE,       s_ctx.value);

  s_base_tick = HAL_GetTick();
  s_tick_led  = s_base_tick;
  s_tick_beat = s_base_tick;
}

/* ===========================================================================
 *  3. 부팅 배너 (PC 로 보내는 리셋 보고)
 * ========================================================================= */
void AppReset_PrintBanner(void)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  char run_str[16];
  char period_str[16];

  AppReset_FormatHMS(s_ctx.run_sec,    run_str,    sizeof(run_str));
  AppReset_FormatHMS(s_ctx.period_sec, period_str, sizeof(period_str));

  /* GetTime 이 shadow register 를 잠그고 GetDate 가 푼다. 반드시 이 순서로 둘 다
     호출해야 다음 읽기에서 시각이 갱신된다. */
  (void)HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  (void)HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  COMM_Puts("\r\n");
  COMM_Puts("========================================================\r\n");
  COMM_Puts(" STM32L562RCT6  RTC WakeUp -> Software Reset  (RS485)\r\n");
  COMM_Puts("========================================================\r\n");

  /* --- 이번 부팅이 어떤 리셋이었는지 --- */
  COMM_Puts(" Reset cause  : ");
  if ((s_ctx.csr & RCC_CSR_LPWRRSTF) != 0U) { COMM_Puts("LOW-POWER "); }
  if ((s_ctx.csr & RCC_CSR_WWDGRSTF) != 0U) { COMM_Puts("WWDG "); }
  if ((s_ctx.csr & RCC_CSR_IWDGRSTF) != 0U) { COMM_Puts("IWDG "); }
  if ((s_ctx.csr & RCC_CSR_SFTRSTF)  != 0U) { COMM_Puts("SOFTWARE "); }
  if ((s_ctx.csr & RCC_CSR_BORRSTF)  != 0U) { COMM_Puts("BOR/POR "); }
  if ((s_ctx.csr & RCC_CSR_PINRSTF)  != 0U) { COMM_Puts("NRST-PIN "); }
  if ((s_ctx.csr & RCC_CSR_OBLRSTF)  != 0U) { COMM_Puts("OPTION-BYTE "); }
  COMM_Printf("(CSR=0x%08lX)\r\n", (unsigned long)s_ctx.csr);

  if (s_ctx.cold_boot)
  {
    COMM_Puts(" Boot type    : COLD  (power ON - VBAT/backup domain cleared)\r\n");
  }
  else if (s_ctx.by_rtc)
  {
    COMM_Puts(" Boot type    : WARM  *** RESET BY RTC WAKEUP TIMER ***\r\n");
  }
  else
  {
    COMM_Puts(" Boot type    : WARM  (software reset, not by RTC)\r\n");
  }

  /* --- 횟수 --- */
  COMM_Printf(" RTC resets   : %lu   (since power ON, backup reg)\r\n",
               (unsigned long)s_ctx.reset_count);
  COMM_Printf(" Boot count   : %lu   (since power ON, backup reg)\r\n",
               (unsigned long)s_ctx.boot_count);
#if (USE_FLASH_COUNTER == 1U)
  COMM_Printf(" TOTAL resets : %lu   (survives power OFF, flash @0x%08lX)\r\n",
               (unsigned long)s_ctx.total_reset,
               (unsigned long)FlashCounter_GetPageAddr());
  COMM_Printf(" Power cycles : %lu   (survives power OFF)\r\n",
               (unsigned long)s_ctx.power_cycle);
#endif

  /* --- 설정 --- */
  COMM_Printf(" RTC clock    : %s\r\n", RTC_CLOCK_NAME);
  COMM_Printf(" Reset period : %lu %s  = %lu s (%s)\r\n",
               (unsigned long)s_ctx.value, AppReset_UnitStr(),
               (unsigned long)s_ctx.period_sec, period_str);
  COMM_Printf(" Run time     : %s  (accumulated since power ON)\r\n", run_str);
  /* RTC 달력은 소프트 리셋으로 지워지지 않는다. 부팅할 때마다 이 값이 주기만큼
     늘어나는지로 실제 주기를 검증할 수 있다(전원을 끄면 2000-01-01 로 복귀). */
  COMM_Printf(" RTC time     : 20%02u-%02u-%02u %02u:%02u:%02u\r\n",
               (unsigned)sDate.Year, (unsigned)sDate.Month, (unsigned)sDate.Date,
               (unsigned)sTime.Hours, (unsigned)sTime.Minutes, (unsigned)sTime.Seconds);
  COMM_Printf(" Next reset in: %lu s\r\n", (unsigned long)s_ctx.period_sec);
  COMM_Puts("--------------------------------------------------------\r\n");
#if (USE_COMM_CMD == 1U)
  COMM_Puts(" CMD: s=status  r=reset now  m=minute  h=hour  +/-=value\r\n");
  COMM_Puts("      t=test(10s)  c=clear counters\r\n");
  COMM_Puts("--------------------------------------------------------\r\n");
#endif
}

/* ===========================================================================
 *  4. WakeUp Timer 장전
 *
 *   WUT 카운터는 16bit 라 ck_spre(1Hz) 기준 한 번에 최대 65535초.
 *   주기가 그보다 길면 여러 번에 나눠서 장전하고, 마지막 조각이 끝났을 때
 *   리셋한다. 이렇게 하면 24시간·48시간 같은 장주기도 그대로 쓸 수 있다.
 * ========================================================================= */
static void AppReset_ArmChunk(void)
{
  uint32_t chunk = (s_remain_sec > WUT_MAX_CHUNK_SEC) ? WUT_MAX_CHUNK_SEC
                                                      : s_remain_sec;
  if (chunk == 0U)
  {
    chunk = 1U;
  }

  (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

  /* ck_spre(1Hz) 모드에서 주기 = (WUT + 1) 초 */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc,
                                  chunk - 1U,
                                  RTC_WAKEUPCLOCK_CK_SPRE_16BITS,
                                  0U) != HAL_OK)
  {
    Error_Handler();
  }

  s_remain_sec -= chunk;
}

void AppReset_StartTimer(void)
{
  s_remain_sec = s_ctx.period_sec;
  s_base_tick  = HAL_GetTick();
  s_tick_beat  = s_base_tick;
  AppReset_ArmChunk();
}

/* ===========================================================================
 *  5. 소프트웨어 리셋 실행
 * ========================================================================= */
void AppReset_DoReset(const char *reason)
{
  FlashCounter_t fc;

  (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

  /* "이 리셋은 내가 의도한 것" 표식 + 누적 동작 시간 갱신 */
  BKP_Write(BKP_REG_PENDING, BKP_PENDING_VALUE);
  BKP_Write(BKP_REG_RUN_SEC, s_ctx.run_sec + AppReset_ElapsedSec());

#if (USE_FLASH_COUNTER == 1U)
  /* 전원이 꺼져도 남는 누적 횟수를 먼저 확정해 둔다 */
  fc.total_reset = s_ctx.total_reset + 1U;
  fc.power_cycle = s_ctx.power_cycle;
  (void)FlashCounter_Write(&fc);
#else
  (void)fc;
#endif

  COMM_Printf("\r\n*** SOFTWARE RESET (%s) : RTC reset #%lu, total #%lu ***\r\n",
               (reason != NULL) ? reason : "-",
               (unsigned long)(s_ctx.reset_count + 1U),
               (unsigned long)(s_ctx.total_reset + 1U));

  /* 마지막 바이트가 나갈 때까지 기다리고 USB 는 정상 분리한 뒤 리셋 */
  COMM_PrepareReset();

  HAL_NVIC_SystemReset();
  /* 여기로는 돌아오지 않는다 */
}

/* ===========================================================================
 *  6. 하트비트 / RS485 명령
 * ========================================================================= */
#if (USE_HEARTBEAT_LOG == 1U)
static void AppReset_PrintHeartbeat(void)
{
  char up[16];
  char left[16];

  AppReset_FormatHMS(AppReset_ElapsedSec(), up,   sizeof(up));
  AppReset_FormatHMS(AppReset_RemainSec(),  left, sizeof(left));

  COMM_Printf("[ALIVE] up %s | next reset in %s | resets %lu (total %lu)\r\n",
               up, left,
               (unsigned long)s_ctx.reset_count,
               (unsigned long)s_ctx.total_reset);
}
#endif

#if (USE_COMM_CMD == 1U)
static void AppReset_ApplyPeriodChange(void)
{
  AppReset_CalcPeriod();
  BKP_Write(BKP_REG_UNIT,  s_ctx.unit);
  BKP_Write(BKP_REG_VALUE, s_ctx.value);

  AppReset_StartTimer();     /* 새 주기로 다시 카운트 시작 */

  COMM_Printf("[CFG ] period = %lu %s (%lu s) - timer restarted\r\n",
               (unsigned long)s_ctx.value, AppReset_UnitStr(),
               (unsigned long)s_ctx.period_sec);
}

static void AppReset_HandleCommand(uint8_t ch)
{
  switch (ch)
  {
    case 's':
    case 'S':
    case '?':
      AppReset_PrintBanner();
      break;

    case 'r':
    case 'R':
      AppReset_DoReset("manual");
      break;

    case 'm':
    case 'M':
      s_ctx.unit = RESET_UNIT_MINUTE;
      AppReset_ApplyPeriodChange();
      break;

    case 'h':
    case 'H':
      s_ctx.unit = RESET_UNIT_HOUR;
      AppReset_ApplyPeriodChange();
      break;

    case '+':
      if (s_ctx.value < 1000U) { s_ctx.value++; }
      AppReset_ApplyPeriodChange();
      break;

    case '-':
      if (s_ctx.value > 1U) { s_ctx.value--; }
      AppReset_ApplyPeriodChange();
      break;

    case 't':
    case 'T':
      /* 동작 확인용 : 10초 주기 (분/시간 단위와 무관한 임시 값) */
      s_ctx.period_sec = 10U;
      AppReset_StartTimer();
      COMM_Puts("[CFG ] TEST mode - reset in 10 s\r\n");
      break;

    case 'c':
    case 'C':
      s_ctx.reset_count = 0U;
      s_ctx.boot_count  = 1U;
      s_ctx.run_sec     = 0U;
      s_ctx.total_reset = 0U;
      s_ctx.power_cycle = 0U;
      BKP_Write(BKP_REG_RESET_COUNT, 0U);
      BKP_Write(BKP_REG_BOOT_COUNT,  1U);
      BKP_Write(BKP_REG_RUN_SEC,     0U);
#if (USE_FLASH_COUNTER == 1U)
      (void)FlashCounter_Erase();
#endif
      COMM_Puts("[CFG ] all counters cleared\r\n");
      break;

    default:
      break;   /* 개행 등은 무시 */
  }
}
#endif /* USE_COMM_CMD */

/* ===========================================================================
 *  7. main 루프에서 계속 호출
 * ========================================================================= */
void AppReset_Task(void)
{
  /* ---- RTC WakeUp 만료 처리 ---------------------------------------------- */
  if (s_wakeup_flag != 0U)
  {
    s_wakeup_flag = 0U;

    if (s_remain_sec > 0U)
    {
      AppReset_ArmChunk();      /* 장주기 : 다음 조각을 이어서 장전 */
    }
    else
    {
      AppReset_DoReset("RTC wakeup");   /* 돌아오지 않는다 */
    }
  }

  /* ---- RS485 / USB CDC 명령 ---------------------------------------------- */
#if (USE_COMM_CMD == 1U)
  {
    uint8_t ch;
    while (COMM_GetChar(&ch))
    {
      AppReset_HandleCommand(ch);
    }
  }
#endif

  /* ---- 하트비트 로그 ----------------------------------------------------- */
#if (USE_HEARTBEAT_LOG == 1U)
  if ((HAL_GetTick() - s_tick_beat) >= (HEARTBEAT_PERIOD_SEC * 1000U))
  {
    s_tick_beat += (HEARTBEAT_PERIOD_SEC * 1000U);
    AppReset_PrintHeartbeat();
  }
#endif

  /* ---- 상태 LED ---------------------------------------------------------- */
#if (USE_STATUS_LED == 1U)
  if ((HAL_GetTick() - s_tick_led) >= 500U)
  {
    s_tick_led = HAL_GetTick();
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
  }
#endif
}

const AppReset_Ctx_t *AppReset_GetCtx(void)
{
  return &s_ctx;
}

/* ===========================================================================
 *  8. RTC WakeUp 인터럽트 콜백
 *
 *   ISR 안에서 바로 리셋하면 RS485 로그가 잘릴 수 있으므로 플래그만 세우고
 *   실제 리셋은 main 루프(AppReset_Task)에서 한다.
 * ========================================================================= */
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc_handle)
{
  UNUSED(hrtc_handle);
  s_wakeup_flag = 1U;
}
