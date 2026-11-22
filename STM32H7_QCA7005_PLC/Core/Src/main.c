/**
  ******************************************************************************
  * @file    main.c
  * @brief   STM32H743 (NUCLEO-H743ZI2) + QCA7005 HomePlug Green PHY
  *
  *  - QCA7005 on SPI1 (mode 3, 5 MHz), INT on PE13, RESET_L on PE14
  *  - ISO 15118-3 SLAC as charger (EVSE) or vehicle (EV), see app_conf.h
  *  - Control Pilot: TIM1_CH1 1 kHz PWM (EVSE), ADC1 PA3 (both), S2 PE11 (EV)
  *  - Log on USART3 (ST-LINK VCP) 115200-8-N-1
  *  - Clock: HSE 8 MHz bypass (ST-LINK MCO) -> SYSCLK 400 MHz, HCLK 200 MHz
  ******************************************************************************
  */
#include <stdio.h>
#include "main.h"
#include "app_plc.h"

SPI_HandleTypeDef  hspi1;
UART_HandleTypeDef huart3;
RNG_HandleTypeDef  hrng;
ADC_HandleTypeDef  hadc1;
TIM_HandleTypeDef  htim1;

static void MPU_Config(void);
void SystemClock_Config(void);
static void PeriphClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_RNG_Init(void);
static void MX_ADC1_Init(void);
#if PLC_ROLE == PLC_ROLE_EVSE
static void MX_TIM1_Init(void);
#endif

int main(void)
{
  MPU_Config();
  SCB_EnableICache();
  SCB_EnableDCache();      /* SPI runs in polling mode: no DMA buffers to keep coherent */

  HAL_Init();
  SystemClock_Config();
  PeriphClock_Config();

  /* DWT cycle counter, used for the CP sampling window */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  MX_GPIO_Init();
  MX_USART3_UART_Init();
  MX_SPI1_Init();
  MX_RNG_Init();
  MX_ADC1_Init();
#if PLC_ROLE == PLC_ROLE_EVSE
  MX_TIM1_Init();
#endif

  APP_PLC_Init();

  while (1)
  {
    APP_PLC_Process();
  }
}

/* printf -> USART3 (used by syscalls.c _write) */
int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;
  HAL_UART_Transmit(&huart3, &c, 1, 10);
  return ch;
}

/**
  * @brief System Clock Configuration
  *        HSE 8 MHz (bypass) -> PLL1 (M4 N400) -> P 400 MHz SYSCLK, Q 80 MHz (SPI123), R 400 MHz
  *        HCLK 200 MHz, APB1..4 100 MHz (timers 200 MHz). HSI48 on for the RNG.
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;          /* 64 MHz, per_ck for the ADC */
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 400;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 10;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                              | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* kernel clocks: SPI1 = PLL1Q 80 MHz, ADC = per_ck (HSI 64 MHz), RNG = HSI48 */
static void PeriphClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef p = {0};

  p.PeriphClockSelection = RCC_PERIPHCLK_SPI123 | RCC_PERIPHCLK_ADC | RCC_PERIPHCLK_RNG | RCC_PERIPHCLK_CKPER;
  p.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
  p.CkperClockSelection = RCC_CLKPSOURCE_HSI;
  p.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;
  p.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&p) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;          /* QCA7000/7005: SPI mode 3 */
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;                       /* CS on PD14 by software */
  hspi1.Init.BaudRatePrescaler = QCA_SPI_PRESCALER;    /* 80 MHz / 16 = 5 MHz */
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x0;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  /* keep SCK at its idle level (high) between HAL calls while CS stays low */
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_RNG_Init(void)
{
  hrng.Instance = RNG;
  hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
  if (HAL_RNG_Init(&hrng) != HAL_OK)
  {
    Error_Handler();
  }
}

/* ADC1 IN15 (PA3) continuous, 12 bit, read by polling in cp.c */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;    /* 64 MHz / 2 */
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_15;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_8CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }
}

#if PLC_ROLE == PLC_ROLE_EVSE
/* TIM1 CH1 (PE9): 200 MHz / 200 = 1 MHz, ARR 999 -> 1 kHz, CCR = duty in 0.1 % */
static void MX_TIM1_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBDT = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 199;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1000;                              /* 100 %: static +12 V (state A/B1) */
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  sBDT.OffStateRunMode = TIM_OSSR_DISABLE;
  sBDT.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBDT.LockLevel = TIM_LOCKLEVEL_OFF;
  sBDT.DeadTime = 0;
  sBDT.BreakState = TIM_BREAK_DISABLE;
  sBDT.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBDT.BreakFilter = 0;
  sBDT.Break2State = TIM_BREAK2_DISABLE;
  sBDT.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBDT.Break2Filter = 0;
  sBDT.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBDT) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_TIM_MspPostInit(&htim1);
}
#endif

static void MX_USART3_UART_Init(void)
{
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /* idle levels first: CS high, QCA7005 held in reset, S2 open, LEDs off */
  HAL_GPIO_WritePin(QCA_CS_PORT, QCA_CS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(QCA_RST_PORT, QCA_RST_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(CP_S2_PORT, CP_S2_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN | LED_RED_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);

  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  g.Pin = QCA_CS_PIN;
  HAL_GPIO_Init(QCA_CS_PORT, &g);

  g.Speed = GPIO_SPEED_FREQ_LOW;
  g.Pin = QCA_RST_PIN;
  HAL_GPIO_Init(QCA_RST_PORT, &g);
  g.Pin = CP_S2_PIN;
  HAL_GPIO_Init(CP_S2_PORT, &g);
  g.Pin = LED_GREEN_PIN | LED_RED_PIN;
  HAL_GPIO_Init(LED_GREEN_PORT, &g);
  g.Pin = LED_YELLOW_PIN;
  HAL_GPIO_Init(LED_YELLOW_PORT, &g);

  /* QCA7005 INTR: rising edge, pull-down keeps it low while the modem is in reset */
  g.Pin = QCA_INT_PIN;
  g.Mode = GPIO_MODE_IT_RISING;
  g.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(QCA_INT_PORT, &g);
  HAL_NVIC_SetPriority(QCA_INT_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(QCA_INT_IRQn);
}

/* background region: no access to unused address space (speculative reads) */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  HAL_MPU_Disable();
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void Error_Handler(void)
{
  __disable_irq();
  HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
