/**
 * @file    main.c
 * @brief   STM32H743 16-bit 듀얼 인터리브 ADC 8채널 예제 (Nucleo-H743ZI2 기준)
 *
 *  - SYSCLK 400 MHz (HSE 8 MHz bypass → PLL1), AHB 200 MHz, APB 100 MHz
 *  - ADC1+ADC2 듀얼 인터리브, 16-bit, 8채널, 채널당 500 kSps(전체 4 Msps)
 *  - USART3(PD8 TX / PD9 RX, ST-LINK 가상 COM) 115200 bps로
 *    1초마다 채널별 평균/최소/최대(mV)를 출력합니다.
 */
#include "main.h"
#include "adc_dual.h"
#include <stdio.h>

UART_HandleTypeDef huart3;

static void SystemClock_Config(void);
static void UART3_Init(void);
static void LED_Init(void);

/* printf → USART3 (CubeIDE의 syscalls.c에 있는 _write가 __io_putchar를 호출) */
int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;
  if (c == '\n')
  {
    uint8_t cr = '\r';
    HAL_UART_Transmit(&huart3, &cr, 1, 10);
  }
  HAL_UART_Transmit(&huart3, &c, 1, 10);
  return ch;
}

int main(void)
{
  ADC_Dual_Stats_t st;
  uint32_t last_print = 0;
  uint32_t last_err = 0;
  uint32_t last_blocks = 0;
  uint32_t ch;

  /* 캐시 활성화. DMA 버퍼는 adc_dual.c에서 캐시 무효화로 일관성을 맞춥니다. */
  SCB_EnableICache();
  SCB_EnableDCache();

  HAL_Init();
  SystemClock_Config();

  LED_Init();
  UART3_Init();
  setvbuf(stdout, NULL, _IONBF, 0);

  printf("\nSTM32H743 dual interleaved ADC, 16-bit, 8ch @ %lu Hz/ch\n",
         (unsigned long)ADC_DUAL_SCAN_RATE_HZ);
  printf("SYSCLK=%lu Hz, REV_ID=0x%04lX\n",
         (unsigned long)HAL_RCC_GetSysClockFreq(), (unsigned long)HAL_GetREVID());

  if (ADC_Dual_Init() != HAL_OK)
  {
    printf("ADC_Dual_Init failed\n");
    Error_Handler();
  }
  if (ADC_Dual_Start() != HAL_OK)
  {
    printf("ADC_Dual_Start failed\n");
    Error_Handler();
  }

  while (1)
  {
    /* 오버런 등 오류가 나면 ADC가 DMA 요청을 멈출 수 있으므로 다시 시작합니다. */
    uint32_t err = ADC_Dual_GetErrorCount();
    if (err != last_err)
    {
      last_err = err;
      (void)ADC_Dual_Stop();
      if (ADC_Dual_Start() != HAL_OK)
      {
        Error_Handler();
      }
    }

    if ((HAL_GetTick() - last_print) >= 1000U)
    {
      last_print = HAL_GetTick();
      ADC_Dual_GetStats(&st);

      /* 블록 수 증가량으로 실제 스캔 레이트를 확인합니다. */
      printf("\nblocks/s=%lu (scans/s=%lu)  errors=%lu\n",
             (unsigned long)(st.block_count - last_blocks),
             (unsigned long)((st.block_count - last_blocks) * ADC_DUAL_SCANS_PER_HALF),
             (unsigned long)err);
      last_blocks = st.block_count;

      for (ch = 0; ch < ADC_DUAL_NUM_CH; ch++)
      {
        printf("CH%lu(%s) mean=%5u (%4lu mV)  min=%5u  max=%5u\n",
               (unsigned long)ch, (ch & 1U) ? "ADC2" : "ADC1",
               st.mean[ch], (unsigned long)ADC_Dual_RawToMilliVolt(st.mean[ch]),
               st.min[ch], st.max[ch]);
      }
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);   /* LD3(빨강): 동작 표시 */
    }
  }
}

/* ------------------------------------------------------------------------- */
/* 예: 실시간 처리 훅. 필요하면 주석을 풀고 내용을 채우세요.                    */
/* ------------------------------------------------------------------------- */
#if 0
void ADC_Dual_BlockCallback(const uint32_t *raw, uint32_t scans)
{
  for (uint32_t s = 0; s < scans; s++)
  {
    const uint32_t *w = &raw[s * ADC_DUAL_RANKS_PER_ADC];
    uint16_t ch0 = (uint16_t)(w[0] & 0xFFFF);  /* PA3  */
    uint16_t ch1 = (uint16_t)(w[0] >> 16);     /* PA4  */
    /* ... ch2 = w[1] & 0xFFFF, ch3 = w[1] >> 16, ... ch7 = w[3] >> 16 */
    (void)ch0; (void)ch1;
  }
}
#endif

/**
 * 시스템 클럭: HSE 8 MHz(bypass) → PLL1 → 400 MHz
 *   PLL1: M=1(8 MHz) × N=100 = VCO 800 MHz, P=2 → 400 MHz
 *   VOS1(최대 400 MHz), Flash latency 2 (AXI 200 MHz)
 * 보드의 HSE가 다르면 PLLM/PLLN과 adc_dual.c의 PLL2 설정을 함께 바꾸세요.
 */
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState            = RCC_HSE_BYPASS;
  osc.PLL.PLLState        = RCC_PLL_ON;
  osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLM            = 1;
  osc.PLL.PLLN            = 100;
  osc.PLL.PLLP            = 2;
  osc.PLL.PLLQ            = 4;
  osc.PLL.PLLR            = 2;
  osc.PLL.PLLRGE          = RCC_PLL1VCIRANGE_3;
  osc.PLL.PLLVCOSEL       = RCC_PLL1VCOWIDE;
  osc.PLL.PLLFRACN        = 0;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK)
  {
    Error_Handler();
  }

  clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                       RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;   /* 400 MHz */
  clk.AHBCLKDivider  = RCC_HCLK_DIV2;     /* 200 MHz */
  clk.APB3CLKDivider = RCC_APB3_DIV2;     /* 100 MHz */
  clk.APB1CLKDivider = RCC_APB1_DIV2;     /* 100 MHz (TIM6 200 MHz) */
  clk.APB2CLKDivider = RCC_APB2_DIV2;     /* 100 MHz */
  clk.APB4CLKDivider = RCC_APB4_DIV2;     /* 100 MHz */
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/** USART3: PD8 TX, PD9 RX (AF7) - Nucleo-H743ZI2 ST-LINK 가상 COM 포트 */
static void UART3_Init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_USART3_CLK_ENABLE();

  g.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = GPIO_NOPULL;
  g.Speed     = GPIO_SPEED_FREQ_LOW;
  g.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOD, &g);

  huart3.Instance            = USART3;
  huart3.Init.BaudRate       = 115200;
  huart3.Init.WordLength     = UART_WORDLENGTH_8B;
  huart3.Init.StopBits       = UART_STOPBITS_1;
  huart3.Init.Parity         = UART_PARITY_NONE;
  huart3.Init.Mode           = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling   = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
}

/** LD1(PB0, 녹색)는 쓰지 않고 LD3(PB14, 빨강)만 사용합니다. */
static void LED_Init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  g.Pin   = GPIO_PIN_14;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &g);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    /* 오류 시 LD3를 빠르게 깜박입니다(SysTick 없이 동작하는 지연 루프). */
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
    for (volatile uint32_t i = 0; i < 4000000U; i++) {}
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  printf("assert_failed: %s:%lu\n", file, (unsigned long)line);
  Error_Handler();
}
#endif
