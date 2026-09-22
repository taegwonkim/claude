/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32L562CET6 + ESP32-C3-WROOM (ESP-AT) Wi-Fi 클라이언트
  *
  *  - USART1(PA9/PA10) 로 ESP32-C3 에 AT 명령을 보내고 응답을 폴링으로 받는다.
  *  - 부팅 시 ESP32 를 하드웨어 리셋 → 초기화 → MAC 주소 읽기 → AP 접속
  *    (DHCP 또는 고정 IP) → TCP 서버 접속 순서로 진행한다.
  *  - AP 또는 서버가 끊기면 wifi_mgr.c 의 상태 머신이 자동으로 재접속한다.
  *  - 접속 중에는 APP_TX_PERIOD_MS 마다 서버에 문자열을 보내고, 서버에서
  *    받은 데이터는 USART2 디버그 포트로 찍는다.
  *
  *  인터럽트는 SysTick(HAL 1ms 틱) 만 사용한다.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "debug_log.h"
#include "esp32_at.h"
#include "wifi_mgr.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ICACHE_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
static void App_Task(void);
static void Led_Task(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  데모 애플리케이션: 주기 송신 + 수신 데이터 출력
  */
static void App_Task(void)
{
#if (APP_TX_PERIOD_MS > 0U)
  static uint32_t last_tx   = 0U;
  static uint32_t tx_count  = 0U;
#endif
  uint8_t  rx[128];
  uint16_t n;

  /* 서버에서 온 데이터 → 디버그 포트로 출력 */
  n = WIFI_Receive(rx, (uint16_t)(sizeof(rx) - 1U));
  if (n > 0U)
  {
    rx[n] = '\0';
    LOG("APP: rx %u bytes: %s", (unsigned)n, (const char *)rx);
  }

#if (APP_TX_PERIOD_MS > 0U)
  if (WIFI_IsOnline() && ((HAL_GetTick() - last_tx) >= APP_TX_PERIOD_MS))
  {
    char msg[96];
    int  len;

    last_tx = HAL_GetTick();
    len = snprintf(msg, sizeof(msg), "STM32L562 #%lu mac=%s ip=%s\r\n",
                   (unsigned long)++tx_count, WIFI_GetMac(), WIFI_GetIp());
    if (len > 0)
    {
      if (WIFI_Send((const uint8_t *)msg, (uint16_t)len))
      {
        LOG("APP: tx #%lu ok", (unsigned long)tx_count);
      }
      else
      {
        LOG("APP: tx #%lu failed", (unsigned long)tx_count);
      }
    }
  }
#endif
}

/**
  * @brief  상태 LED
  *           RESET / INIT      : 빠르게 (100ms)
  *           AP / SERVER 접속중: 중간   (250ms)
  *           WAIT (재시도 대기) : 느리게 (1s)
  *           ONLINE            : 켜짐 (2초마다 짧게 깜빡)
  */
static void Led_Task(void)
{
  static uint32_t last = 0U;
  static bool     on   = false;
  uint32_t        now  = HAL_GetTick();
  uint32_t        period;

  switch (WIFI_GetState())
  {
    case WIFI_STATE_RESET:
    case WIFI_STATE_INIT:           period = 100U;  break;
    case WIFI_STATE_AP_CONNECT:
    case WIFI_STATE_SERVER_CONNECT: period = 250U;  break;
    case WIFI_STATE_WAIT:           period = 1000U; break;
    case WIFI_STATE_ONLINE:
    default:
      /* 켜져 있다가 2초마다 50ms 꺼짐 */
      period = on ? 2000U : 50U;
      break;
  }

  if ((now - last) >= period)
  {
    last = now;
    on   = !on;
#if (STATUS_LED_ACTIVE_HIGH == 1U)
    HAL_GPIO_WritePin(STATUS_LED_GPIO_PORT, STATUS_LED_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(STATUS_LED_GPIO_PORT, STATUS_LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  LOG("");
  LOG("=== STM32L562CET6 + ESP32-C3 AT Wi-Fi client ===");
  LOG("SYSCLK=%lu Hz", (unsigned long)HAL_RCC_GetSysClockFreq());

  /* 기본 설정(wifi_config.h)으로 시작. 실행 중 바꾸려면 WIFI_Config_t 를
     채워서 WIFI_Init(&cfg) 또는 WIFI_Reconfigure(&cfg) 를 부르면 된다. */
  WIFI_Init(NULL);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    WIFI_Process();     /* ESP32 폴링 + 접속/재접속 상태 머신 */
    App_Task();         /* 데이터 송수신 데모                  */
    Led_Task();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  *
  *   SYSCLK        = MSI 48 MHz (외부 크리스탈 불필요)
  *   USART1/USART2 = HSI16      (16MHz / 115200 = 138.9 → 오차 -0.08%)
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_11;      /* 48 MHz */
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;           /* USART 용 16 MHz */
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                     RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** USART1 / USART2 커널 클럭 = HSI16 */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_HSI;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ICACHE Initialization Function
  */
static void MX_ICACHE_Init(void)
{
  /* USER CODE BEGIN ICACHE_Init 0 */
  /* USER CODE END ICACHE_Init 0 */

  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN ICACHE_Init 2 */
  /* USER CODE END ICACHE_Init 2 */
}

/**
  * @brief USART1 Initialization Function (ESP32-C3 AT 포트)
  */
static void MX_USART1_UART_Init(void)
{
  /* USER CODE BEGIN USART1_Init 0 */
  /* 실제 초기화는 esp32_at.c 의 ESP_Init() 안에서 한다 (WIFI_Init 에서 호출).
     CubeMX 재생성 시 이 함수 본문이 채워지더라도 ESP_Init() 이 같은 설정으로
     다시 초기화하므로 문제 없다. */
  /* USER CODE END USART1_Init 0 */
}

/**
  * @brief USART2 Initialization Function (디버그 로그)
  */
static void MX_USART2_UART_Init(void)
{
  /* USER CODE BEGIN USART2_Init 0 */
  DBG_Init();
  /* USER CODE END USART2_Init 0 */
}

/**
  * @brief GPIO Initialization Function
  *
  *   PA5 : STATUS_LED (출력)
  *   PB0 : ESP_EN     (출력, 기본 H = ESP32 동작)
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* LED off */
#if (STATUS_LED_ACTIVE_HIGH == 1U)
  HAL_GPIO_WritePin(STATUS_LED_GPIO_PORT, STATUS_LED_PIN, GPIO_PIN_RESET);
#else
  HAL_GPIO_WritePin(STATUS_LED_GPIO_PORT, STATUS_LED_PIN, GPIO_PIN_SET);
#endif
  /* ESP32 EN = H (동작). 리셋은 ESP_HardReset() 이 담당 */
  HAL_GPIO_WritePin(ESP_EN_GPIO_PORT, ESP_EN_PIN, GPIO_PIN_SET);

  GPIO_InitStruct.Pin   = STATUS_LED_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(STATUS_LED_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin   = ESP_EN_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ESP_EN_GPIO_PORT, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
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
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif /* USE_FULL_ASSERT */
