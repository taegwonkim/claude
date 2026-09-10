/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32L562 - RTC Alarm A 를 이용한 소프트웨어 리셋
  *                    (기본 설정 : 매일 03:00:00)
  *
  * 동작 개요
  *  1) RTC 를 LSI(또는 LSE) 로 구동하고 달력을 실제 시각으로 맞춘다.
  *  2) Alarm A 에 AlarmMask = RTC_ALARMMASK_DATEWEEKDAY 를 주면 "날짜"가
  *     비교에서 제외되어, 지정한 시:분:초마다(= 하루에 한 번) 알람이 발생한다.
  *     즉 마스크만으로 24시간 주기가 만들어지고 재장전이 필요 없다.
  *  3) 인터럽트 콜백에서 플래그만 세우고, main 루프에서 HAL_NVIC_SystemReset() 호출.
  *  4) 소프트 리셋으로는 RTC/백업도메인이 지워지지 않으므로 리셋 횟수를
  *     백업 레지스터에 누적한다. 단 이 보드는 VBAT 가 MCU 전원(VDD)에 물려
  *     있어, 전원을 내렸다 올리면 백업 도메인도 함께 초기화된다(콜드 부트).
  *  5) RCC 리셋 플래그로 "소프트웨어 리셋"이었는지 부팅 시 확인/출력한다.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;
#if (USE_DEBUG_UART == 1U)
UART_HandleTypeDef huart_dbg;
#endif

/* USER CODE BEGIN PV */
/* RTC Wakeup 인터럽트에서 세워지는 리셋 요청 플래그 */
static volatile uint8_t g_reset_request = 0U;

/* 부팅 직후 캡처한 RCC 리셋 원인 플래그 */
static uint32_t g_reset_flags = 0U;
static uint32_t g_reset_count = 0U;

/* 백업 도메인이 초기화된 채로 부팅했는가 = 전원이 새로 인가된 콜드 부트.
   이 보드는 VBAT 가 MCU 전원(VDD)에 연결되어 있어 전원 off/on 시 항상 1 이 된다. */
static uint8_t g_cold_boot = 1U;

/* 실제로 RTC 에 물린 저속 클럭 (LSE 기동 실패 시 LSI 로 폴백) */
static uint8_t g_rtc_clk_is_lse = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
#if (USE_DEBUG_UART == 1U)
static void MX_USART1_UART_Init(void);
#endif

/* USER CODE BEGIN PFP */
static void CaptureResetCause(void);
static void PrintBanner(void);
#if (USE_DEBUG_UART == 1U) && (USE_HEARTBEAT_LOG == 1U)
static void PrintHeartbeat(void);
#endif
static void SetInitialDateTime(void);
static void RTC_SetResetAlarm(void);
#if (USE_DEBUG_UART == 1U) && (USE_UART_TIME_SYNC == 1U)
static uint8_t SyncTimeFromUart(void);
#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#if (USE_DEBUG_UART == 1U)
/* printf() 를 디버그 UART 로 연결 (CubeIDE 의 syscalls.c 가 _write -> __io_putchar 호출) */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart_dbg, (uint8_t *)&ch, 1U, HAL_MAX_DELAY);
  return ch;
}
#endif

#if (RTC_INIT_FROM_BUILD_TIME == 1U) || (USE_UART_TIME_SYNC == 1U)
/**
  * @brief  Sakamoto 알고리즘으로 요일을 계산한다.
  * @param  year : 4자리 연도, month : 1~12, day : 1~31
  * @retval RTC_WEEKDAY_MONDAY(1) ~ RTC_WEEKDAY_SUNDAY(7)
  */
static uint8_t CalcWeekDay(uint16_t year, uint8_t month, uint8_t day)
{
  static const uint8_t t[12] = {0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U};
  uint32_t y = year;
  uint32_t w;

  if (month < 3U)
  {
    y--;
  }
  w = (y + (y / 4U) - (y / 100U) + (y / 400U) + t[month - 1U] + day) % 7U;

  /* Sakamoto : 0=일요일 ... 6=토요일 / RTC : 1=월요일 ... 7=일요일 */
  return (w == 0U) ? (uint8_t)RTC_WEEKDAY_SUNDAY : (uint8_t)w;
}
#endif /* RTC_INIT_FROM_BUILD_TIME || USE_UART_TIME_SYNC */

/**
  * @brief  콜드 부트 시 RTC 달력의 초기 시각을 설정한다.
  * @note   RTC_INIT_FROM_BUILD_TIME 이 1 이면 컴파일 시각(__DATE__/__TIME__)을
  *         사용한다. Alarm 방식은 "벽시계 시각" 기준이므로 실제 시각과 맞춰야
  *         의미가 있는데, 빌드 시각을 넣어두면 별도 동기화 없이도 대략 맞는다.
  *         (플래싱까지 걸린 시간만큼 오차가 생기므로, 정확도가 필요하면
  *          GPS/NTP/호스트 통신 등으로 받은 시각을 넣을 것)
  */
static void SetInitialDateTime(void)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

#if (RTC_INIT_FROM_BUILD_TIME == 1U)
  /* __DATE__ = "Mmm dd yyyy" (dd 는 공백 패딩 가능), __TIME__ = "hh:mm:ss" */
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *bd = __DATE__;
  const char *bt = __TIME__;
  uint8_t  month = 1U;
  uint8_t  day;
  uint16_t year;
  uint8_t  i;

  for (i = 0U; i < 12U; i++)
  {
    if ((months[i * 3U] == bd[0]) &&
        (months[(i * 3U) + 1U] == bd[1]) &&
        (months[(i * 3U) + 2U] == bd[2]))
    {
      month = (uint8_t)(i + 1U);
      break;
    }
  }
  day  = (uint8_t)(((bd[4] == ' ') ? 0U : (uint8_t)(bd[4] - '0') * 10U)
                   + (uint8_t)(bd[5] - '0'));
  year = (uint16_t)(((bd[7] - '0') * 1000) + ((bd[8] - '0') * 100)
                    + ((bd[9] - '0') * 10) + (bd[10] - '0'));

  sTime.Hours   = (uint8_t)(((bt[0] - '0') * 10) + (bt[1] - '0'));
  sTime.Minutes = (uint8_t)(((bt[3] - '0') * 10) + (bt[4] - '0'));
  sTime.Seconds = (uint8_t)(((bt[6] - '0') * 10) + (bt[7] - '0'));

  sDate.Month   = month;
  sDate.Date    = day;
  sDate.Year    = (uint8_t)(year % 100U);          /* RTC 는 2자리 연도 */
  sDate.WeekDay = CalcWeekDay(year, month, day);
#else
  /* 2000-01-01 (토요일) 00:00:00 */
  sTime.Hours   = 0U;
  sTime.Minutes = 0U;
  sTime.Seconds = 0U;
  sDate.Month   = RTC_MONTH_JANUARY;
  sDate.Date    = 1U;
  sDate.Year    = 0U;
  sDate.WeekDay = RTC_WEEKDAY_SATURDAY;
#endif

  sTime.SubSeconds     = 0U;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
}

#if (USE_DEBUG_UART == 1U) && (USE_UART_TIME_SYNC == 1U)
/**
  * @brief  콜드 부트 시 UART 로 현재 시각을 받아 RTC 달력에 설정한다.
  * @note   입력 형식 : "YYYY-MM-DD HH:MM:SS" + 개행 (예: 2026-09-10 14:30:00)
  *         TIME_SYNC_TIMEOUT_MS 안에 유효한 입력이 없으면 아무것도 바꾸지 않는다.
  * @retval 1 = 시각을 설정함, 0 = 설정하지 않음
  */
static uint8_t SyncTimeFromUart(void)
{
  char     buf[32];
  uint8_t  idx = 0U;
  uint8_t  ch;
  uint32_t start = HAL_GetTick();
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  uint16_t year;
  uint8_t  month;
  uint8_t  day;
  uint8_t  i;

  printf("[TIME] Send \"YYYY-MM-DD HH:MM:SS\" within %lu ms to set the RTC...\r\n",
         (unsigned long)TIME_SYNC_TIMEOUT_MS);

  /* 개행이 올 때까지, 또는 타임아웃까지 한 글자씩 받는다 */
  while ((HAL_GetTick() - start) < TIME_SYNC_TIMEOUT_MS)
  {
    if (HAL_UART_Receive(&huart_dbg, &ch, 1U, 50U) != HAL_OK)
    {
      continue;
    }
    if ((ch == (uint8_t)'\r') || (ch == (uint8_t)'\n'))
    {
      if (idx > 0U)
      {
        break;
      }
      continue;
    }
    if (idx < (uint8_t)(sizeof(buf) - 1U))
    {
      buf[idx] = (char)ch;
      idx++;
    }
  }
  buf[idx] = '\0';

  if (idx < 19U)
  {
    printf("[TIME] no input - keeping the default calendar\r\n");
    return 0U;
  }

  /* 자리별 숫자 검증 : "YYYY-MM-DD HH:MM:SS" */
  for (i = 0U; i < 19U; i++)
  {
    uint8_t is_digit_pos = ((i != 4U) && (i != 7U) && (i != 10U)
                            && (i != 13U) && (i != 16U)) ? 1U : 0U;
    if (is_digit_pos != 0U)
    {
      if ((buf[i] < '0') || (buf[i] > '9'))
      {
        printf("[TIME] bad format - keeping the default calendar\r\n");
        return 0U;
      }
    }
  }

  year  = (uint16_t)(((buf[0] - '0') * 1000) + ((buf[1] - '0') * 100)
                     + ((buf[2] - '0') * 10) + (buf[3] - '0'));
  month = (uint8_t)(((buf[5] - '0') * 10) + (buf[6] - '0'));
  day   = (uint8_t)(((buf[8] - '0') * 10) + (buf[9] - '0'));
  sTime.Hours   = (uint8_t)(((buf[11] - '0') * 10) + (buf[12] - '0'));
  sTime.Minutes = (uint8_t)(((buf[14] - '0') * 10) + (buf[15] - '0'));
  sTime.Seconds = (uint8_t)(((buf[17] - '0') * 10) + (buf[18] - '0'));

  if ((year < 2000U) || (year > 2099U) || (month < 1U) || (month > 12U)
      || (day < 1U) || (day > 31U) || (sTime.Hours > 23U)
      || (sTime.Minutes > 59U) || (sTime.Seconds > 59U))
  {
    printf("[TIME] out of range - keeping the default calendar\r\n");
    return 0U;
  }

  sTime.SubSeconds     = 0U;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sDate.Month   = month;
  sDate.Date    = day;
  sDate.Year    = (uint8_t)(year % 100U);      /* RTC 는 2자리 연도 */
  sDate.WeekDay = CalcWeekDay(year, month, day);

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }

  printf("[TIME] RTC set to %04u-%02u-%02u %02u:%02u:%02u\r\n",
         (unsigned int)year, (unsigned int)month, (unsigned int)day,
         (unsigned int)sTime.Hours, (unsigned int)sTime.Minutes,
         (unsigned int)sTime.Seconds);
  return 1U;
}
#endif /* USE_UART_TIME_SYNC */

/**
  * @brief  RCC 리셋 플래그를 읽어 저장하고 클리어한다.
  *         반드시 부팅 직후 한 번만 호출할 것(클리어되면 다음 부팅까지 알 수 없음).
  */
static void CaptureResetCause(void)
{
  g_reset_flags = RCC->CSR;
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

static void PrintBanner(void)
{
#if (USE_DEBUG_UART == 1U)
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* GetTime 을 먼저 호출해야 shadow register 가 갱신되고 GetDate 가 유효하다 */
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  printf("\r\n==========================================\r\n");
  printf(" STM32L562 RTC Alarm A Reset (daily)\r\n");
  printf("==========================================\r\n");
  printf(" Reset cause : ");
  if (g_reset_flags & RCC_CSR_SFTRSTF)  { printf("SOFTWARE "); }
  if (g_reset_flags & RCC_CSR_PINRSTF)  { printf("NRST-PIN "); }
  if (g_reset_flags & RCC_CSR_BORRSTF)  { printf("BOR "); }
  if (g_reset_flags & RCC_CSR_IWDGRSTF) { printf("IWDG "); }
  if (g_reset_flags & RCC_CSR_WWDGRSTF) { printf("WWDG "); }
  if (g_reset_flags & RCC_CSR_LPWRRSTF) { printf("LOW-POWER "); }
  printf("(CSR=0x%08lX)\r\n", (unsigned long)g_reset_flags);
  printf(" Boot type   : %s\r\n",
         (g_cold_boot != 0U) ? "COLD  (power-on, backup domain cleared)"
                             : "WARM  (reset only, backup domain kept)");
  printf(" RTC clock   : %s\r\n",
         (g_rtc_clk_is_lse != 0U) ? "LSE 32.768kHz" : "LSI ~32kHz (+/-5%)");
  printf(" Soft resets since power-on : %lu\r\n", (unsigned long)g_reset_count);
  printf(" RTC time    : 20%02d-%02d-%02d %02d:%02d:%02d\r\n",
         sDate.Year, sDate.Month, sDate.Date,
         sTime.Hours, sTime.Minutes, sTime.Seconds);
#if (ALARM_MODE == ALARM_MODE_DAILY_FIXED)
  printf(" Trigger     : RTC Alarm A (daily fixed)\r\n");
  printf(" Next reset at %02u:%02u:%02u every day\r\n",
         (unsigned int)ALARM_RESET_HOUR,
         (unsigned int)ALARM_RESET_MINUTE,
         (unsigned int)ALARM_RESET_SECOND);
#else
  printf(" Trigger     : RTC Alarm A (relative)\r\n");
  printf(" Next reset in %lu s (%luh %02lum)\r\n",
         (unsigned long)RESET_PERIOD_SEC,
         (unsigned long)(RESET_PERIOD_SEC / 3600U),
         (unsigned long)((RESET_PERIOD_SEC % 3600U) / 60U));
#endif
  printf("------------------------------------------\r\n");
#endif
}

#if (USE_DEBUG_UART == 1U) && (USE_HEARTBEAT_LOG == 1U)
/**
  * @brief  살아있음 로그. HEARTBEAT_PERIOD_SEC 마다 호출된다.
  * @note   저전력 모드에 진입하지 않고 계속 동작 중임을 확인하는 용도.
  */
static void PrintHeartbeat(void)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  uint32_t up_sec = HAL_GetTick() / 1000U;      /* 부팅 후 경과 [초] */
  uint32_t remain;

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

#if (ALARM_MODE == ALARM_MODE_DAILY_FIXED)
  {
    /* 현재 시각에서 다음 알람 시각까지 남은 시간 */
    uint32_t now_sec = ((uint32_t)sTime.Hours * 3600U)
                       + ((uint32_t)sTime.Minutes * 60U)
                       + (uint32_t)sTime.Seconds;
    uint32_t target_sec = ((uint32_t)ALARM_RESET_HOUR * 3600U)
                          + ((uint32_t)ALARM_RESET_MINUTE * 60U)
                          + (uint32_t)ALARM_RESET_SECOND;
    remain = (target_sec + 86400U - now_sec) % 86400U;
    if (remain == 0U)
    {
      remain = 86400U;
    }
  }
#else
  /* RELATIVE 모드의 알람은 부팅 시점에 걸리므로 uptime 으로 환산한다 */
  remain = (up_sec >= RESET_PERIOD_SEC) ? 0U : (RESET_PERIOD_SEC - up_sec);
#endif

  printf("[ALIVE] uptime %02lu:%02lu:%02lu | RTC 20%02d-%02d-%02d %02d:%02d:%02d"
         " | reset in %lu s (%luh %02lum)\r\n",
         (unsigned long)(up_sec / 3600U),
         (unsigned long)((up_sec % 3600U) / 60U),
         (unsigned long)(up_sec % 60U),
         sDate.Year, sDate.Month, sDate.Date,
         sTime.Hours, sTime.Minutes, sTime.Seconds,
         (unsigned long)remain,
         (unsigned long)(remain / 3600U),
         (unsigned long)((remain % 3600U) / 60U));
}
#endif /* USE_HEARTBEAT_LOG */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  uint32_t tick_led = 0U;
#if (USE_DEBUG_UART == 1U) && (USE_HEARTBEAT_LOG == 1U)
  uint32_t tick_beat = 0U;
#endif
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  CaptureResetCause();
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
#if (USE_DEBUG_UART == 1U)
  MX_USART1_UART_Init();
#endif
  MX_RTC_Init();

  /* USER CODE BEGIN 2 */

  /* 백업 도메인(RTC/TAMP 백업 레지스터) 쓰기 허용 */
  HAL_PWR_EnableBkUpAccess();

  /* 백업 도메인 보존 여부로 콜드/웜 부트를 구분한다.
     - 매직 값이 없음  = 백업 도메인이 지워짐 = 전원 off/on (콜드 부트)
     - 매직 값이 있음  = 소프트 리셋 또는 NRST (웜 부트, RTC 계속 동작 중)
     VBAT 가 VDD 와 공유되므로 전원을 내렸다 올리면 항상 콜드 부트가 된다.
     따라서 리셋 횟수는 "이번 전원 인가 이후"의 누적값이다. */
  if (HAL_RTCEx_BKUPRead(&hrtc, BKP_REG_MAGIC) != BKP_MAGIC_VALUE)
  {
    g_cold_boot = 1U;
    HAL_RTCEx_BKUPWrite(&hrtc, BKP_REG_MAGIC, BKP_MAGIC_VALUE);
    g_reset_count = 0U;
  }
  else
  {
    g_cold_boot = 0U;
    g_reset_count = HAL_RTCEx_BKUPRead(&hrtc, BKP_REG_RESET_COUNT);
    if (g_reset_flags & RCC_CSR_SFTRSTF)
    {
      g_reset_count++;
    }
  }
  HAL_RTCEx_BKUPWrite(&hrtc, BKP_REG_RESET_COUNT, g_reset_count);

#if (USE_DEBUG_UART == 1U) && (USE_UART_TIME_SYNC == 1U)
  /* 전원을 내리면 RTC 달력이 지워지므로, 콜드 부트 때만 실제 시각을 받는다.
     (소프트 리셋 후에는 달력이 살아있으므로 건너뛴다) */
  if (g_cold_boot != 0U)
  {
    if (SyncTimeFromUart() != 0U)
    {
      /* 달력이 바뀌었으니 알람을 다시 건다 */
      RTC_SetResetAlarm();
    }
  }
#endif

  PrintBanner();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 이 루프는 저전력 모드에 진입하지 않는다.
       (HAL_PWR_EnterSLEEPMode / STOPMode / STANDBYMode, __WFI 를 쓰지 않음)
       MCU 는 리셋 시점까지 풀스피드로 계속 동작하며, RTC 는 백업 도메인에서
       독립적으로 카운트하다가 시간이 되면 인터럽트를 발생시킨다. */

    if (g_reset_request != 0U)
    {
      g_reset_request = 0U;

#if (USE_DEBUG_UART == 1U)
      printf("\r\n[RTC] Alarm A fired -> Software reset now!\r\n");
      /* UART 송신 완료 대기 (마지막 문자가 잘리지 않도록) */
      while (__HAL_UART_GET_FLAG(&huart_dbg, UART_FLAG_TC) == RESET) { }
#endif

      /* 재시작 후 다시 설정하므로 여기서는 정리만 한다 */
      HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);

      /* ===== 소프트웨어 리셋 ===== */
      HAL_NVIC_SystemReset();
      /* 여기로는 절대 돌아오지 않는다 */
    }

#if (USE_DEBUG_UART == 1U) && (USE_HEARTBEAT_LOG == 1U)
    /* 살아있음 로그 : HEARTBEAT_PERIOD_SEC 마다 uptime / 남은 시간 출력 */
    if ((HAL_GetTick() - tick_beat) >= (HEARTBEAT_PERIOD_SEC * 1000U))
    {
      tick_beat += (HEARTBEAT_PERIOD_SEC * 1000U);
      PrintHeartbeat();
    }
#endif

#if (USE_STATUS_LED == 1U)
    /* 살아있음 표시 : LED 500ms 토글 */
    if ((HAL_GetTick() - tick_led) >= 500U)
    {
      tick_led = HAL_GetTick();
      HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
    }
#else
    (void)tick_led;
#endif

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  *        SYSCLK = MSI 4MHz (기본값, 저전력/단순 구성)
  *        RTC    = LSI 32kHz (또는 LSE 32.768kHz)
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /* 백업 도메인 쓰기 허용 (LSE/RTC 설정에 필요) */
  HAL_PWR_EnableBkUpAccess();

  /** Configure the main internal regulator output voltage */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators : MSI (시스템 클럭) */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;   /* 4 MHz */
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** RTC 용 저속 클럭
    * 이 보드는 VBAT 가 MCU 전원(VDD)에 연결되어 있어 백업 도메인이 상시
    * 전원을 받지 못한다. 즉 전원을 넣을 때마다 저속 발진기를 새로 기동해야
    * 한다. LSE 는 기동에 수백 ms ~ 수 초가 걸리고 크리스탈이 없으면 실패하는데,
    * 그때 Error_Handler() 로 멈춰버리면 전원을 넣을 때마다 보드가 죽는다.
    * 따라서 LSE 기동 실패 시 LSI 로 폴백해서 계속 동작시킨다.
    */
#if (RTC_CLOCK_LSE == 1U)
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) == HAL_OK)
  {
    g_rtc_clk_is_lse = 1U;
  }
#endif
  if (g_rtc_clk_is_lse == 0U)
  {
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
#ifdef RCC_LSI_DIV1
    RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV1;   /* LSI = 32 kHz (분주 없음) */
#endif
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
      Error_Handler();
    }
  }

  /** Initializes the CPU, AHB and APB buses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the peripherals clocks (RTC / USART1) */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = (g_rtc_clk_is_lse != 0U)
                                    ? RCC_RTCCLKSOURCE_LSE : RCC_RTCCLKSOURCE_LSI;
#if (USE_DEBUG_UART == 1U)
  PeriphClkInit.PeriphClockSelection |= RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
#endif
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RTC Initialization Function
  *        - 달력(Calendar) 초기화 (콜드부트 시에만)
  *        - Wakeup Timer 를 1Hz(ck_spre) 기준 600초로 설정
  * @retval None
  */
static void MX_RTC_Init(void)
{
  /** Initialize RTC Only */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  /* 프리스케일러는 실제로 기동된 클럭에 맞춰야 한다 (LSE 폴백 대응) */
  if (g_rtc_clk_is_lse != 0U)
  {
    /* LSE 32768 Hz : (127+1) * (255+1) = 32768 -> ck_spre = 1 Hz */
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv  = 255;
  }
  else
  {
    /* LSI 32000 Hz : (127+1) * (249+1) = 32000 -> ck_spre = 1 Hz */
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv  = 249;
  }
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
#ifdef RTC_BINARY_NONE
  hrtc.Init.BinMode = RTC_BINARY_NONE;   /* HAL 버전에 따라 없을 수 있음 */
#endif
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  /* 소프트웨어 리셋 후에는 RTC 가 계속 살아있으므로 달력을 다시 쓰지 않는다.
     INITS 비트(달력이 한 번이라도 설정되었는지)로 판별한다. */
  if ((hrtc.Instance->ICSR & RTC_ICSR_INITS) != RTC_ICSR_INITS)
  {
    /* USER CODE END Check_RTC_BKUP */

    /** Initialize RTC and set the Time and Date */
    SetInitialDateTime();

    /* USER CODE BEGIN Check_RTC_Calendar */
  }
  /* USER CODE END Check_RTC_Calendar */

  /* USER CODE BEGIN RTC_Init 2 */
  /* Alarm A 설정 : 벽시계 시각 기준으로 알람을 건다.
     DAILY_FIXED 모드는 날짜를 마스킹하므로 매일 같은 시각에 자동 반복된다. */
  RTC_SetResetAlarm();
  /* USER CODE END RTC_Init 2 */
}

#if (USE_DEBUG_UART == 1U)
/**
  * @brief USART1 Initialization Function (디버그 로그용, 115200-8-N-1)
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart_dbg.Instance = DBG_UART_INSTANCE;
  huart_dbg.Init.BaudRate = 115200;
  huart_dbg.Init.WordLength = UART_WORDLENGTH_8B;
  huart_dbg.Init.StopBits = UART_STOPBITS_1;
  huart_dbg.Init.Parity = UART_PARITY_NONE;
  huart_dbg.Init.Mode = UART_MODE_TX_RX;
  huart_dbg.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart_dbg.Init.OverSampling = UART_OVERSAMPLING_16;
  huart_dbg.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart_dbg.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart_dbg.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart_dbg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart_dbg, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart_dbg, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart_dbg) != HAL_OK)
  {
    Error_Handler();
  }
}
#endif /* USE_DEBUG_UART */

/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init(void)
{
#if (USE_STATUS_LED == 1U)
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
#else
  __HAL_RCC_GPIOA_CLK_ENABLE();
#endif
}

/* USER CODE BEGIN 4 */

/**
  * @brief  리셋용 Alarm A 를 설정한다.
  * @note   AlarmMask 에 RTC_ALARMMASK_DATEWEEKDAY 를 주면 "날짜"를 비교에서
  *         제외하므로, 지정한 시:분:초가 될 때마다(= 하루에 한 번) 알람이
  *         발생한다. 즉 마스크만으로 24시간 주기가 만들어진다.
  */
static void RTC_SetResetAlarm(void)
{
  RTC_AlarmTypeDef sAlarm = {0};

#if (ALARM_MODE == ALARM_MODE_DAILY_FIXED)
  /* 매일 고정된 시각에 리셋 (기본 03:00:00) */
  sAlarm.AlarmTime.Hours   = ALARM_RESET_HOUR;
  sAlarm.AlarmTime.Minutes = ALARM_RESET_MINUTE;
  sAlarm.AlarmTime.Seconds = ALARM_RESET_SECOND;
#else
  /* 현재 시각 + RESET_PERIOD_SEC 에 리셋 (콜백에서 재장전) */
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  uint32_t now_sec;
  uint32_t target_sec;

  /* GetTime -> GetDate 순서로 호출해야 shadow register 가 정상 해제된다 */
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  now_sec = ((uint32_t)sTime.Hours * 3600U)
            + ((uint32_t)sTime.Minutes * 60U)
            + (uint32_t)sTime.Seconds;
  /* 날짜를 마스킹하므로 하루(86400초) 안에서 순환시킨다.
     RESET_PERIOD_SEC 가 정확히 86400 이면 target == now 가 되는데,
     알람 비교는 매 초 갱신 시점에 일어나므로 이번 초에는 매치되지 않고
     정확히 24시간 뒤에 발생한다. */
  target_sec = (now_sec + RESET_PERIOD_SEC) % 86400U;

  sAlarm.AlarmTime.Hours   = (uint8_t)(target_sec / 3600U);
  sAlarm.AlarmTime.Minutes = (uint8_t)((target_sec % 3600U) / 60U);
  sAlarm.AlarmTime.Seconds = (uint8_t)(target_sec % 60U);
#endif

  sAlarm.AlarmTime.SubSeconds     = 0U;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;

  sAlarm.AlarmMask          = RTC_ALARMMASK_DATEWEEKDAY;   /* 날짜 무시 = 매일 */
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;  /* 서브초 무시 */
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay    = 1U;                         /* 마스킹되어 무의미 */
  sAlarm.Alarm               = RTC_ALARM_A;

  /* 재장전 시 기존 알람을 먼저 해제해야 안전하다 */
  if (HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  RTC Alarm A 인터럽트 콜백
  * @note   DAILY_FIXED 모드는 알람이 매일 자동 반복되므로 재장전이 필요 없다.
  *         RELATIVE 모드는 다음 알람을 여기서 다시 걸어야 하지만, 어차피
  *         곧바로 리셋되므로 재부팅 후 MX_RTC_Init() 에서 다시 설정된다.
  */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc_handle)
{
  UNUSED(hrtc_handle);
  g_reset_request = 1U;
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
