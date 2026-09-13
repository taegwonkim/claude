/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32L562RCT6
  *                   RTC WakeUp Timer 로 일정 시간(분/시간 단위)마다 소프트웨어
  *                   리셋하고, 리셋 사실과 횟수를 USART3(RS485) 와
  *                   USB CDC(가상 COM 포트) 양쪽으로 PC 에 보고.
  *
  *  동작 요약
  *   1) HAL_Init() 직후 RCC->CSR 로 직전 리셋 원인을 캡처한다(1회성 플래그).
  *   2) RTC 를 LSI(또는 LSE) 로 구동하고 WakeUp Timer 를 ck_spre(1Hz)로 건다.
  *      주기가 65535초를 넘으면 소프트웨어로 나눠서 재장전한다.
  *   3) 시간이 되면 인터럽트 -> main 루프에서 RS485 로 메시지를 보내고
  *      마지막 바이트 송신 완료를 기다린 뒤 HAL_NVIC_SystemReset().
  *   4) 리셋 횟수는 두 군데에 기록한다.
  *        - TAMP 백업 레지스터 : 전원이 유지되는 동안의 횟수 (빠름)
  *        - 내부 Flash        : 전원을 껐다 켜도 남는 누적 횟수
  *      이 보드는 VBAT 가 VDD 와 함께 꺼지므로 백업 레지스터만으로는
  *      전원 사이클을 넘어선 횟수를 셀 수 없기 때문이다.
  *
  *  저전력 모드에 들어가지 않는다 (__WFI / STOP / STANDBY 미사용).
  *  MCU 는 리셋 시점까지 계속 돌고, RTC 만 백업 도메인에서 시간을 센다.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_reset.h"
#include "comm.h"
#include "rs485.h"
#include "usb_cdc.h"
#include "flash_counter.h"
#if (USE_USB_CDC == 1U)
#include "usb_device.h"     /* CubeMX 가 USB_DEVICE/App 에 생성 */
#endif
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
/* USB 를 쓰면 SYSCLK 을 48MHz 로 올린다.
   USB FS 는 인터럽트를 제때 처리해야 열거가 되는데 MSI 4MHz 로는 빠듯하다.
   USB 자체 클럭은 SYSCLK 과 무관하게 HSI48(+CRS) 로 따로 공급한다.

   FLASH wait state 는 데이터시트 최소값(48MHz/Range1 기준 2WS)보다 한 단계
   여유를 줬다. 필요한 값보다 크게 잡는 것은 동작상 안전하며(약간 느려질 뿐)
   데이터시트 표가 개정돼도 문제가 생기지 않는다. */
#if (USE_USB_CDC == 1U)
  #define APP_MSI_RANGE       RCC_MSIRANGE_11   /* 48 MHz */
  #define APP_FLASH_LATENCY   FLASH_LATENCY_3
#else
  #define APP_MSI_RANGE       RCC_MSIRANGE_6    /* 4 MHz  */
  #define APP_FLASH_LATENCY   FLASH_LATENCY_0
#endif
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ICACHE_Init(void);
static void MX_RTC_Init(void);
#if (USE_RS485 == 1U)
static void MX_USART3_UART_Init(void);
#endif
#if (USE_USB_CDC == 1U)
static void MX_USB_Clock_Init(void);
#endif

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* 리셋 원인 플래그는 읽고 지우면 사라지므로 가장 먼저 캡처한다 */
  AppReset_CaptureCause();
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
#if (USE_RS485 == 1U)
  MX_USART3_UART_Init();
#endif
  MX_RTC_Init();
#if (USE_USB_CDC == 1U)
  MX_USB_DEVICE_Init();     /* CubeMX 가 USB_DEVICE/App/usb_device.c 에 생성 */
#endif

  /* USER CODE BEGIN 2 */
  COMM_Init();              /* USB 열거 대기 (RS485 만 쓰면 즉시 리턴)      */
  AppReset_Init();          /* 백업/Flash 카운터 정리 (콜드·웜 부트 판별) */
  AppReset_PrintBanner();   /* PC 로 리셋 보고 전송                        */
  AppReset_StartTimer();    /* WakeUp Timer 기동                           */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 저전력 모드에 진입하지 않는다. 시간이 되면 AppReset_Task() 안에서
       RS485 로 메시지를 보내고 소프트웨어 리셋한다. */
    AppReset_Task();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  *
  *   SYSCLK = MSI 48 MHz  (USB 사용 시) / MSI 4 MHz (USB 미사용 시)
  *   USART3 = HSI16       (SYSCLK 과 무관하게 정확한 보레이트 확보)
  *                         16MHz/115200 = 138.9 -> 오차 -0.08%
  *   USB    = HSI48 + CRS (USB SOF 로 동기 -> 크리스탈 없이 규격 만족)
  *   RTC    = LSI 32kHz 또는 LSE 32.768kHz (main.h 의 RTC_CLOCK_LSE)
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /* 백업 도메인 쓰기 허용 (LSE / RTC 클럭 선택에 필요) */
  HAL_PWR_EnableBkUpAccess();

#if (RTC_CLOCK_LSE == 1U)
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
#endif

  /** Configure the main internal regulator output voltage */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_HSI;
#if (RTC_CLOCK_LSE == 1U)
  RCC_OscInitStruct.OscillatorType |= RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
#else
  RCC_OscInitStruct.OscillatorType |= RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
#ifdef RCC_LSI_DIV1
  RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV1;      /* LSI = 32 kHz (분주 없음) */
#endif
#endif
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = APP_MSI_RANGE;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;                 /* USART3 용 16MHz */
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
#if (USE_USB_CDC == 1U)
  RCC_OscInitStruct.OscillatorType |= RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;             /* USB 용 48MHz */
#endif
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
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, APP_FLASH_LATENCY) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the peripherals clocks (RTC / USART3) */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
#if (RTC_CLOCK_LSE == 1U)
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
#else
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
#endif
#if (USE_RS485 == 1U)
  PeriphClkInit.PeriphClockSelection |= RCC_PERIPHCLK_USART3;
  PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_HSI;
#endif
#if (USE_USB_CDC == 1U)
  PeriphClkInit.PeriphClockSelection |= RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
#endif
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

#if (USE_USB_CDC == 1U)
  MX_USB_Clock_Init();
#endif
}

#if (USE_USB_CDC == 1U)
/**
  * @brief  USB 전원 도메인 + HSI48 자동 보정(CRS) 설정
  *
  *   - VDDUSB 를 켜지 않으면 USB 트랜시버가 동작하지 않는다. L5/L4 에서
  *     USB 가 "아무 반응 없음"인 경우 대부분 이것을 빠뜨린 것이다.
  *   - HSI48 은 RC 발진기라 그대로는 USB 규격(±0.25%)을 만족하지 못한다.
  *     CRS 가 호스트의 SOF(1ms)를 기준으로 HSI48 을 계속 보정해 주므로
  *     32MHz 크리스탈 없이도 USB 가 동작한다.
  * @retval None
  */
static void MX_USB_Clock_Init(void)
{
  RCC_CRSInitTypeDef CRSInitStruct = {0};

  HAL_PWREx_EnableVddUSB();

  __HAL_RCC_CRS_CLK_ENABLE();

  CRSInitStruct.Prescaler             = RCC_CRS_SYNC_DIV1;
  CRSInitStruct.Source                = RCC_CRS_SYNC_SOURCE_USB;
  CRSInitStruct.Polarity              = RCC_CRS_SYNC_POLARITY_RISING;
  CRSInitStruct.ReloadValue           = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000U, 1000U);
  CRSInitStruct.ErrorLimitValue       = 34;
  CRSInitStruct.HSI48CalibrationValue = 32;
  HAL_RCCEx_CRSConfig(&CRSInitStruct);
}
#endif /* USE_USB_CDC */

/**
  * @brief RTC Initialization Function
  * @note  WakeUp Timer 는 AppReset_StartTimer() 에서 장전한다.
  *        여기서는 ck_spre 가 정확히 1Hz 가 되도록 프리스케일러만 맞춘다.
  * @retval None
  */
static void MX_RTC_Init(void)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  hrtc.Instance = RTC;
  hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv   = RTC_ASYNC_PREDIV;   /* main.h 에서 클럭별로 계산 */
  hrtc.Init.SynchPrediv    = RTC_SYNC_PREDIV;
  hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap    = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp   = RTC_OUTPUT_PULLUP_NONE;
#ifdef RTC_BINARY_NONE
  hrtc.Init.BinMode        = RTC_BINARY_NONE;    /* HAL 버전에 따라 없을 수 있음 */
#endif
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  /* 소프트웨어 리셋 후에는 RTC 가 계속 살아 있으므로 달력을 다시 쓰지 않는다.
     ICSR.INITS = "달력이 한 번이라도 설정되었는가" 비트.
     이 보드는 VBAT 가 VDD 와 함께 꺼지므로, 전원을 껐다 켜면 INITS 도 0 이
     되어 아래 달력 초기화가 다시 수행된다(= 전원 인가 시점이 0시 0분 0초). */
  if ((hrtc.Instance->ICSR & RTC_ICSR_INITS) != RTC_ICSR_INITS)
  {
    /* USER CODE END Check_RTC_BKUP */

    /** Initialize RTC and set the Time and Date
      * WakeUp Timer 는 달력과 무관하게 동작하므로 기준값만 넣는다. */
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
    sDate.Year = 0x0;                   /* 2000-01-01 */
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler();
    }

    /* USER CODE BEGIN Check_RTC_Calendar */
  }
  /* USER CODE END Check_RTC_Calendar */

  /* USER CODE BEGIN RTC_Init 2 */
  /* WakeUp Timer 장전은 AppReset_StartTimer() 에서 한다
     (주기를 실행 중에도 바꿀 수 있어야 하므로). */
  /* USER CODE END RTC_Init 2 */
}

#if (USE_RS485 == 1U)
/**
  * @brief USART3 Initialization Function (RS485 Driver Enable 모드)
  * @note  실제 설정은 rs485.c 의 RS485_Init() 안에 있다.
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{
  /* USER CODE BEGIN USART3_Init 2 */
  RS485_Init();
  /* USER CODE END USART3_Init 2 */
}
#endif

/**
  * @brief ICACHE Initialization Function
  * @retval None
  */
static void MX_ICACHE_Init(void)
{
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

#if (USE_STATUS_LED == 1U)
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
#endif

#if (USE_RS485 == 1U) && (RS485_USE_HW_DE == 0U)
  /* 소프트웨어 DE 제어 모드일 때만 DE 핀을 일반 출력으로 잡는다.
     (하드웨어 DE 모드에서는 HAL_UART_MspInit 에서 AF7 로 설정된다) */
  HAL_GPIO_WritePin(RS485_DE_GPIO_PORT, RS485_DE_PIN, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = RS485_DE_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(RS485_DE_GPIO_PORT, &GPIO_InitStruct);
#endif

  (void)GPIO_InitStruct;
}

/* USER CODE BEGIN 4 */
/* HAL_RTCEx_WakeUpTimerEventCallback() 은 app_reset.c 에 있다. */
/* HAL_UART_RxCpltCallback() / HAL_UART_ErrorCallback() 은 rs485.c 에 있다.  */
/* USB OTG FS 인터럽트 핸들러는 usb_cdc.c 에 있다.                          */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @note   LSE 를 선택했는데 크리스탈이 없으면 여기서 멈춘다.
  *         그 경우 main.h 의 RTC_CLOCK_LSE 를 0 으로 바꿀 것.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
#if (USE_STATUS_LED == 1U)
    /* 에러 표시 : LED 빠르게 깜빡임 (HAL_Delay 는 IRQ 정지 상태라 못 씀) */
    volatile uint32_t i;
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
    for (i = 0U; i < 100000U; i++) { __NOP(); }
#endif
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
  UNUSED(file);
  UNUSED(line);
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
