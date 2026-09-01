/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32L562 - RTC Wakeup Timer 를 이용한 주기적 소프트웨어 리셋
  *                    (기본 설정 : 24시간 = 86400초)
  *                    "부팅 시점"으로부터 RESET_PERIOD_SEC 마다 리셋한다.
  *                    벽시계 시각 기준으로 리셋하려면 AlarmA 프로젝트를 볼 것.
  *
  * 동작 개요
  *  1) RTC 를 LSI(또는 LSE) 로 구동하고 Wakeup Timer 를 ck_spre(1Hz) 로 설정한다.
  *  2) 24시간은 16bit 카운터(최대 65536초)를 넘으므로 CK_SPRE_17BITS 모드
  *     (2^16 가산)를 사용한다. 카운터 20863 + 1 + 65536 = 86400초.
  *  3) 인터럽트 콜백에서 플래그만 세우고, main 루프에서 HAL_NVIC_SystemReset() 호출.
  *  4) 리셋 후에도 RTC/백업도메인은 유지되므로 리셋 횟수를 백업 레지스터에 누적한다.
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
  printf(" STM32L562 RTC WakeUp Timer Reset (24h)\r\n");
  printf("==========================================\r\n");
  printf(" Reset cause : ");
  if (g_reset_flags & RCC_CSR_SFTRSTF)  { printf("SOFTWARE "); }
  if (g_reset_flags & RCC_CSR_PINRSTF)  { printf("NRST-PIN "); }
  if (g_reset_flags & RCC_CSR_BORRSTF)  { printf("BOR "); }
  if (g_reset_flags & RCC_CSR_IWDGRSTF) { printf("IWDG "); }
  if (g_reset_flags & RCC_CSR_WWDGRSTF) { printf("WWDG "); }
  if (g_reset_flags & RCC_CSR_LPWRRSTF) { printf("LOW-POWER "); }
  printf("(CSR=0x%08lX)\r\n", (unsigned long)g_reset_flags);
  printf(" Soft reset count : %lu\r\n", (unsigned long)g_reset_count);
  printf(" RTC time    : 20%02d-%02d-%02d %02d:%02d:%02d\r\n",
         sDate.Year, sDate.Month, sDate.Date,
         sTime.Hours, sTime.Minutes, sTime.Seconds);
  printf(" Trigger     : RTC WakeUp Timer\r\n");
  printf(" Next reset in %lu s (%luh %02lum)\r\n",
         (unsigned long)RESET_PERIOD_SEC,
         (unsigned long)(RESET_PERIOD_SEC / 3600U),
         (unsigned long)((RESET_PERIOD_SEC % 3600U) / 60U));
  printf("------------------------------------------\r\n");
#endif
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  uint32_t tick_led = 0U;
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

  /* 리셋 횟수 누적 : 콜드부트면 0 으로 시작, 소프트 리셋이면 +1 */
  if (HAL_RTCEx_BKUPRead(&hrtc, BKP_REG_MAGIC) != BKP_MAGIC_VALUE)
  {
    HAL_RTCEx_BKUPWrite(&hrtc, BKP_REG_MAGIC, BKP_MAGIC_VALUE);
    g_reset_count = 0U;
  }
  else
  {
    g_reset_count = HAL_RTCEx_BKUPRead(&hrtc, BKP_REG_RESET_COUNT);
    if (g_reset_flags & RCC_CSR_SFTRSTF)
    {
      g_reset_count++;
    }
  }
  HAL_RTCEx_BKUPWrite(&hrtc, BKP_REG_RESET_COUNT, g_reset_count);

  PrintBanner();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (g_reset_request != 0U)
    {
      g_reset_request = 0U;

#if (USE_DEBUG_UART == 1U)
      printf("\r\n[RTC] %lu s elapsed -> Software reset now!\r\n",
             (unsigned long)RESET_PERIOD_SEC);
      /* UART 송신 완료 대기 (마지막 문자가 잘리지 않도록) */
      while (__HAL_UART_GET_FLAG(&huart_dbg, UART_FLAG_TC) == RESET) { }
#endif

      /* 재시작 후 다시 설정하므로 여기서는 정리만 한다 */
      HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

      /* ===== 소프트웨어 리셋 ===== */
      HAL_NVIC_SystemReset();
      /* 여기로는 절대 돌아오지 않는다 */
    }

#if (USE_STATUS_LED == 1U)
    /* 살아있음 표시 : 500ms 토글 */
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

  /** Initializes the RCC Oscillators */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
#if (RTC_CLOCK_LSE == 1U)
  RCC_OscInitStruct.OscillatorType |= RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
#else
  RCC_OscInitStruct.OscillatorType |= RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
#ifdef RCC_LSI_DIV1
  RCC_OscInitStruct.LSIDiv   = RCC_LSI_DIV1;   /* LSI = 32 kHz (분주 없음) */
#endif
#endif
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;   /* 4 MHz */
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
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
#if (RTC_CLOCK_LSE == 1U)
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
#else
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
#endif
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
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /** Initialize RTC Only */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
#if (RTC_CLOCK_LSE == 1U)
  /* LSE 32768 Hz : (127+1) * (255+1) = 32768 -> ck_spre = 1 Hz */
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv  = 255;
#else
  /* LSI 32000 Hz : (127+1) * (249+1) = 32000 -> ck_spre = 1 Hz */
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv  = 249;
#endif
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

    /** Initialize RTC and set the Time and Date
      * Wakeup Timer 는 달력 시각과 무관하게 동작하므로 기준값만 넣는다.
      * (로그의 RTC time 이 리셋 간격을 눈으로 확인하는 용도가 된다) */
    sTime.Hours = 0x0;
    sTime.Minutes = 0x0;
    sTime.Seconds = 0x0;
    sTime.SubSeconds = 0x0;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler();
    }
    sDate.WeekDay = RTC_WEEKDAY_SATURDAY;
    sDate.Month = RTC_MONTH_JANUARY;
    sDate.Date = 0x1;
    sDate.Year = 0x0;                  /* 2000-01-01 */
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler();
    }

    /* USER CODE BEGIN Check_RTC_Calendar */
  }
  /* USER CODE END Check_RTC_Calendar */

  /* USER CODE BEGIN RTC_Init 2 */
  /** Enable the WakeUp
    * 주기 파라미터(WUT_CLOCK_SEL / WUT_COUNTER)는 main.h 에서
    * RESET_PERIOD_SEC 값으로부터 자동 계산된다.
    *   - 65536초 이하 : CK_SPRE_16BITS, 주기 = (WUT + 1) 초
    *   - 그 이상      : CK_SPRE_17BITS, 주기 = (WUT + 1 + 65536) 초
    * 24시간(86400초) -> CK_SPRE_17BITS, WUT = 20863
    *   (20863 + 1 + 65536 = 86400)
    */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, WUT_COUNTER,
                                  WUT_CLOCK_SEL, 0U) != HAL_OK)
  {
    Error_Handler();
  }
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
  * @brief  RTC Wakeup Timer 인터럽트 콜백 (RESET_PERIOD_SEC 마다 호출, 기본 24시간)
  * @note   ISR 컨텍스트이므로 여기서 바로 리셋하지 않고 플래그만 세운다.
  *         (즉시 리셋을 원하면 여기서 HAL_NVIC_SystemReset() 을 호출해도 된다.)
  */
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc_handle)
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
