/**
 * @file    adc_dual.c
 * @brief   STM32H743 ADC1/ADC2 16-bit 듀얼 인터리브 8채널 + TIM6 트리거 + DMA
 *
 * 초기화 방식은 ADC_DUAL_USE_CUBEMX_INIT(adc_dual.h)로 고릅니다.
 *   0 (기본) : 이 파일이 클럭/GPIO/DMA/ADC/TIM6/IRQ 초기화를 모두 직접 합니다.
 *              CubeMX에서 ADC1/ADC2/TIM6/DMA를 설정하면 안 됩니다.
 *   1        : CubeMX가 생성한 MX_xxx_Init()와 hadc1/hadc2/htim6, IRQ 핸들러를 씁니다.
 *              이 파일은 설정 확인, 캘리브레이션, 시작/정지, 콜백 처리만 합니다.
 *              CubeMX 설정은 CubeMX_Setup.md를 따르세요.
 *
 * 클럭 전제
 *   - HSE 8 MHz(Nucleo-H743ZI2의 ST-LINK MCO, bypass)
 *   - ADC 커널 클럭 = PLL2P = 72 MHz
 *       Rev.V : ADC 내부에서 /2 → f_ADC = 36 MHz (프리스케일러 DIV1)
 *       Rev.Y : 내부 /2 없음   → 프리스케일러 DIV2로 f_ADC = 36 MHz
 *   - TIM6 클럭 = APB1 타이머 클럭(SystemClock 설정에 따라 자동 계산)
 */
#include "adc_dual.h"
#include <string.h>

#if !ADC_DUAL_USE_CUBEMX_INIT
/* ------------------------------------------------------------------------- */
/* 핀/채널 테이블                                                              */
/*  - 모두 ADC1/ADC2 공용(ADC12_INPx) 또는 해당 ADC 전용 핀입니다.              */
/*  - Nucleo-H743ZI2에서 이더넷, LED, ST-LINK VCP와 겹치지 않는 핀으로 골랐습니다. */
/* ------------------------------------------------------------------------- */
typedef struct
{
  GPIO_TypeDef *port;
  uint16_t      pin;
  uint32_t      channel;
} ADC_Dual_Pin_t;

/* ADC1(마스터): 논리 CH0, CH2, CH4, CH6 */
static const ADC_Dual_Pin_t s_master_pins[ADC_DUAL_RANKS_PER_ADC] =
{
  { GPIOA, GPIO_PIN_3,  ADC_CHANNEL_15 },  /* CH0 : PA3  ADC12_INP15 (Nucleo A0) */
  { GPIOC, GPIO_PIN_0,  ADC_CHANNEL_10 },  /* CH2 : PC0  ADC123_INP10 (Nucleo A1) */
  { GPIOB, GPIO_PIN_1,  ADC_CHANNEL_5  },  /* CH4 : PB1  ADC12_INP5  (Nucleo A3) */
  { GPIOA, GPIO_PIN_6,  ADC_CHANNEL_3  },  /* CH6 : PA6  ADC12_INP3  (Nucleo D12) */
};

/* ADC2(슬레이브): 논리 CH1, CH3, CH5, CH7 */
static const ADC_Dual_Pin_t s_slave_pins[ADC_DUAL_RANKS_PER_ADC] =
{
  { GPIOA, GPIO_PIN_4,  ADC_CHANNEL_18 },  /* CH1 : PA4  ADC12_INP18 */
  { GPIOA, GPIO_PIN_5,  ADC_CHANNEL_19 },  /* CH3 : PA5  ADC12_INP19 (Nucleo D13) */
  { GPIOF, GPIO_PIN_13, ADC_CHANNEL_2  },  /* CH5 : PF13 ADC2_INP2   (Nucleo D7) */
  { GPIOF, GPIO_PIN_14, ADC_CHANNEL_6  },  /* CH7 : PF14 ADC2_INP6   (Nucleo D4) */
};

static const uint32_t s_ranks[ADC_DUAL_RANKS_PER_ADC] =
{
  ADC_REGULAR_RANK_1, ADC_REGULAR_RANK_2, ADC_REGULAR_RANK_3, ADC_REGULAR_RANK_4
};

/* ------------------------------------------------------------------------- */
/* 핸들                                                                        */
/* ------------------------------------------------------------------------- */
ADC_HandleTypeDef hadc1;   /* 마스터 */
ADC_HandleTypeDef hadc2;   /* 슬레이브 */
DMA_HandleTypeDef hdma_adc1;
TIM_HandleTypeDef htim6;

#else  /* ADC_DUAL_USE_CUBEMX_INIT */

/* CubeMX가 main.c(또는 adc.c / tim.c)에 생성한 핸들 */
extern ADC_HandleTypeDef hadc1;   /* 마스터 */
extern ADC_HandleTypeDef hadc2;   /* 슬레이브 */
extern TIM_HandleTypeDef htim6;

#endif /* ADC_DUAL_USE_CUBEMX_INIT */

/* ------------------------------------------------------------------------- */
/* 버퍼 / 상태                                                                 */
/* ------------------------------------------------------------------------- */

/* DMA1은 DTCM(0x20000000)에 접근할 수 없으므로 D2 SRAM(0x30000000)에 둡니다.
 * 링커 스크립트에 .RAM_D2 섹션이 있어야 합니다(Linker/ 폴더 참고).
 * 캐시 라인(32바이트) 정렬 + 크기가 32바이트 배수여야 캐시 무효화가 안전합니다. */
__attribute__((section(".RAM_D2"), aligned(32)))
static uint32_t s_dma_buf[ADC_DUAL_DMA_WORDS];

_Static_assert((sizeof(s_dma_buf) / 2U) % 32U == 0U,
               "half buffer must be a multiple of the 32-byte cache line");

static volatile ADC_Dual_Stats_t s_stats;
static volatile uint32_t         s_error_count;

/* ------------------------------------------------------------------------- */
/* 내부 함수                                                                   */
/* ------------------------------------------------------------------------- */
#if !ADC_DUAL_USE_CUBEMX_INIT
static HAL_StatusTypeDef adc_clock_init(void)
{
  RCC_PeriphCLKInitTypeDef clk = {0};

  /* PLL2: HSE 8 MHz / M(1) = 8 MHz → ×N(36) = VCO 288 MHz → /P(4) = 72 MHz */
  clk.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  clk.PLL2.PLL2M      = 1;
  clk.PLL2.PLL2N      = 36;
  clk.PLL2.PLL2P      = 4;
  clk.PLL2.PLL2Q      = 4;
  clk.PLL2.PLL2R      = 4;
  clk.PLL2.PLL2RGE    = RCC_PLL2VCIRANGE_3;   /* 입력 8~16 MHz */
  clk.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;      /* VCO 192~836 MHz */
  clk.PLL2.PLL2FRACN  = 0;
  clk.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;

  return HAL_RCCEx_PeriphCLKConfig(&clk);
}

static void adc_gpio_init(void)
{
  GPIO_InitTypeDef g = {0};
  uint32_t i;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  g.Mode = GPIO_MODE_ANALOG;
  g.Pull = GPIO_NOPULL;

  for (i = 0; i < ADC_DUAL_RANKS_PER_ADC; i++)
  {
    g.Pin = s_master_pins[i].pin;
    HAL_GPIO_Init(s_master_pins[i].port, &g);
    g.Pin = s_slave_pins[i].pin;
    HAL_GPIO_Init(s_slave_pins[i].port, &g);
  }
}

static HAL_StatusTypeDef adc_dma_init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();

  hdma_adc1.Instance                 = DMA1_Stream0;
  hdma_adc1.Init.Request             = DMA_REQUEST_ADC1;
  hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;   /* CDR 32비트 */
  hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  hdma_adc1.Init.Mode                = DMA_CIRCULAR;
  hdma_adc1.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;  /* 워드→워드 direct 모드 */

  if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
  {
    return HAL_ERROR;
  }
  __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  return HAL_OK;
}

static void adc_fill_init(ADC_HandleTypeDef *h, ADC_TypeDef *inst, int is_master)
{
  h->Instance = inst;

  /* Rev.V는 ADC 안에서 커널 클럭을 /2 하므로 DIV1, Rev.Y는 DIV2로 36 MHz를 맞춥니다. */
  h->Init.ClockPrescaler = (HAL_GetREVID() <= REV_ID_Y) ? ADC_CLOCK_ASYNC_DIV2
                                                        : ADC_CLOCK_ASYNC_DIV1;
  h->Init.Resolution             = ADC_RESOLUTION_16B;
  h->Init.ScanConvMode           = ADC_SCAN_ENABLE;
  h->Init.EOCSelection           = ADC_EOC_SEQ_CONV;
  h->Init.LowPowerAutoWait       = DISABLE;
  h->Init.ContinuousConvMode     = DISABLE;           /* 트리거 1회 = 스캔 1회 */
  h->Init.NbrOfConversion        = ADC_DUAL_RANKS_PER_ADC;
  h->Init.DiscontinuousConvMode  = DISABLE;
  h->Init.NbrOfDiscConversion    = 1;
  h->Init.Overrun                = ADC_OVR_DATA_OVERWRITTEN;
  h->Init.LeftBitShift           = ADC_LEFTBITSHIFT_NONE;
  h->Init.OversamplingMode       = DISABLE;

  if (is_master)
  {
    /* 트리거는 마스터에만 겁니다. 슬레이브는 마스터를 따라갑니다. */
    h->Init.ExternalTrigConv     = ADC_EXTERNALTRIG_T6_TRGO;
    h->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    /* 듀얼 모드 데이터는 CDR → 마스터의 DMA 요청으로 전송됩니다. */
    h->Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  }
  else
  {
    h->Init.ExternalTrigConv     = ADC_SOFTWARE_START;  /* 듀얼 모드에서는 무시됨 */
    h->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    h->Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;  /* 슬레이브는 DMA 없음 */
  }
}

static HAL_StatusTypeDef adc_config_channels(ADC_HandleTypeDef *h, const ADC_Dual_Pin_t *pins)
{
  ADC_ChannelConfTypeDef c = {0};
  uint32_t i;

  c.SamplingTime           = ADC_DUAL_SAMPLING_TIME;
  c.SingleDiff             = ADC_SINGLE_ENDED;
  c.OffsetNumber           = ADC_OFFSET_NONE;
  c.Offset                 = 0;
  c.OffsetRightShift       = DISABLE;
  c.OffsetSignedSaturation = DISABLE;

  for (i = 0; i < ADC_DUAL_RANKS_PER_ADC; i++)
  {
    c.Channel = pins[i].channel;
    c.Rank    = s_ranks[i];
    if (HAL_ADC_ConfigChannel(h, &c) != HAL_OK)   /* PCSEL(채널 프리셀렉트)도 HAL이 설정 */
    {
      return HAL_ERROR;
    }
  }
  return HAL_OK;
}

#endif /* !ADC_DUAL_USE_CUBEMX_INIT */

static uint32_t tim6_clock_hz(void)
{
  /* APB1 프리스케일러가 1이 아니면 타이머 클럭은 PCLK1 × 2 */
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  if ((RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) == RCC_APB1_DIV1)
  {
    return pclk1;
  }
  return pclk1 * 2U;
}

/** TIM6 프리스케일러/주기 계산(ADC_DUAL_SCAN_RATE_HZ 기준) */
static void tim6_calc(uint32_t *psc, uint32_t *arr)
{
  uint32_t ticks = tim6_clock_hz() / ADC_DUAL_SCAN_RATE_HZ;
  uint32_t p = 0;

  /* TIM6은 16비트 카운터이므로 ARR이 넘치면 프리스케일러를 키웁니다. */
  while ((ticks / (p + 1U)) > 65536U)
  {
    p++;
  }
  *psc = p;
  *arr = (ticks / (p + 1U)) - 1U;
}

#if !ADC_DUAL_USE_CUBEMX_INIT
static HAL_StatusTypeDef tim6_init(void)
{
  TIM_MasterConfigTypeDef m = {0};
  uint32_t psc, arr;

  tim6_calc(&psc, &arr);
  __HAL_RCC_TIM6_CLK_ENABLE();

  htim6.Instance               = TIM6;
  htim6.Init.Prescaler         = psc;
  htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim6.Init.Period            = arr;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    return HAL_ERROR;
  }

  m.MasterOutputTrigger = TIM_TRGO_UPDATE;   /* 업데이트 이벤트 → TRGO → ADC1 */
  m.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  return HAL_TIMEx_MasterConfigSynchronization(&htim6, &m);
}
#endif /* !ADC_DUAL_USE_CUBEMX_INIT */

/** 하프 버퍼 통계 계산 */
static void process_block(const uint32_t *raw, uint32_t scans)
{
  uint32_t sum[ADC_DUAL_NUM_CH] = {0};
  uint16_t mn[ADC_DUAL_NUM_CH];
  uint16_t mx[ADC_DUAL_NUM_CH] = {0};
  uint32_t s, k, ch;

  for (ch = 0; ch < ADC_DUAL_NUM_CH; ch++)
  {
    mn[ch] = 0xFFFFU;
  }

  for (s = 0; s < scans; s++)
  {
    for (k = 0; k < ADC_DUAL_RANKS_PER_ADC; k++)
    {
      uint16_t v[2];
      ADC_Dual_Unpack(raw[s * ADC_DUAL_RANKS_PER_ADC + k], &v[0], &v[1]);

      for (uint32_t j = 0; j < 2U; j++)
      {
        ch = 2U * k + j;   /* 짝수 = 마스터, 홀수 = 슬레이브 */
        sum[ch] += v[j];
        if (v[j] < mn[ch]) { mn[ch] = v[j]; }
        if (v[j] > mx[ch]) { mx[ch] = v[j]; }
      }
    }
  }

  for (ch = 0; ch < ADC_DUAL_NUM_CH; ch++)
  {
    s_stats.min[ch]  = mn[ch];
    s_stats.max[ch]  = mx[ch];
    s_stats.mean[ch] = (uint16_t)((sum[ch] + scans / 2U) / scans);
  }
  s_stats.block_count++;
}

static void on_half_ready(const uint32_t *half)
{
  /* DMA가 메모리에 직접 쓴 데이터이므로 CPU 캐시의 옛 값을 버립니다. */
  SCB_InvalidateDCache_by_Addr((void *)half, (int32_t)(sizeof(s_dma_buf) / 2U));

  process_block(half, ADC_DUAL_SCANS_PER_HALF);
  ADC_Dual_BlockCallback(half, ADC_DUAL_SCANS_PER_HALF);
}

/* ------------------------------------------------------------------------- */
/* 공개 API                                                                    */
/* ------------------------------------------------------------------------- */
static void d2sram_enable(void)
{
  /* D2 SRAM1~3을 CPU에 할당(클럭 활성화). DMA 버퍼가 0x30000000에 있습니다. */
  __HAL_RCC_D2SRAM1_CLK_ENABLE();
  __HAL_RCC_D2SRAM2_CLK_ENABLE();
  __HAL_RCC_D2SRAM3_CLK_ENABLE();
}

static void dma_buf_clear(void)
{
  memset((void *)s_dma_buf, 0, sizeof(s_dma_buf));
  SCB_CleanInvalidateDCache_by_Addr((void *)s_dma_buf, (int32_t)sizeof(s_dma_buf));
}

#if !ADC_DUAL_USE_CUBEMX_INIT
HAL_StatusTypeDef ADC_Dual_Init(void)
{
  ADC_MultiModeTypeDef mm = {0};

  d2sram_enable();

  if (adc_clock_init() != HAL_OK) { return HAL_ERROR; }

  __HAL_RCC_ADC12_CLK_ENABLE();
  adc_gpio_init();
  if (adc_dma_init() != HAL_OK) { return HAL_ERROR; }

  /* ADC1(마스터) */
  adc_fill_init(&hadc1, ADC1, 1);
  if (HAL_ADC_Init(&hadc1) != HAL_OK) { return HAL_ERROR; }

  /* 듀얼 인터리브 모드 + 32비트 데이터 형식(DAMDF=0b10) + 샘플링 지연 */
  mm.Mode             = ADC_DUALMODE_INTERL;
  mm.DualModeData     = ADC_DUALMODEDATAFORMAT_32_10_BITS;
  mm.TwoSamplingDelay = ADC_DUAL_TWO_SAMPLING_DELAY;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &mm) != HAL_OK) { return HAL_ERROR; }

  if (adc_config_channels(&hadc1, s_master_pins) != HAL_OK) { return HAL_ERROR; }

  /* ADC2(슬레이브) */
  adc_fill_init(&hadc2, ADC2, 0);
  if (HAL_ADC_Init(&hadc2) != HAL_OK) { return HAL_ERROR; }
  if (adc_config_channels(&hadc2, s_slave_pins) != HAL_OK) { return HAL_ERROR; }

  /* 오버런 인터럽트(HAL_ADCEx_MultiModeStart_DMA가 ADC_IT_OVR을 켭니다) */
  HAL_NVIC_SetPriority(ADC_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(ADC_IRQn);

  if (tim6_init() != HAL_OK) { return HAL_ERROR; }

  dma_buf_clear();
  return HAL_OK;
}

#else  /* ADC_DUAL_USE_CUBEMX_INIT */

/**
 * CubeMX 모드: MX_ADC1_Init(), MX_ADC2_Init(), MX_TIM6_Init(), MX_DMA_Init()가
 * 먼저 호출되어 있어야 합니다. 여기서는 설정이 맞는지 확인하고,
 * TIM6 주기를 ADC_DUAL_SCAN_RATE_HZ에 맞춥니다.
 */
HAL_StatusTypeDef ADC_Dual_Init(void)
{
  uint32_t psc, arr;

  d2sram_enable();

  /* CubeMX 설정 확인: 하나라도 다르면 CubeMX_Setup.md를 다시 확인하세요. */
  if (hadc1.Instance != ADC1 || hadc2.Instance != ADC2)                         { return HAL_ERROR; }
  if (hadc1.DMA_Handle == NULL)                                                  { return HAL_ERROR; }
  if (hadc1.Init.Resolution != ADC_RESOLUTION_16B)                               { return HAL_ERROR; }
  if (hadc1.Init.NbrOfConversion != ADC_DUAL_RANKS_PER_ADC)                      { return HAL_ERROR; }
  if (hadc2.Init.NbrOfConversion != ADC_DUAL_RANKS_PER_ADC)                      { return HAL_ERROR; }
  if (LL_ADC_GetMultimode(ADC12_COMMON) != LL_ADC_MULTI_DUAL_REG_INTERL)         { return HAL_ERROR; }
  if (LL_ADC_GetMultiDMATransfer(ADC12_COMMON) != LL_ADC_MULTI_REG_DMA_RES_32_10B) { return HAL_ERROR; }
  if (htim6.Instance != TIM6)                                                    { return HAL_ERROR; }

  /* TIM6 주기를 adc_dual.h의 스캔 레이트에 맞춥니다(CubeMX 값보다 우선). */
  tim6_calc(&psc, &arr);
  __HAL_TIM_SET_PRESCALER(&htim6, psc);
  __HAL_TIM_SET_AUTORELOAD(&htim6, arr);
  htim6.Instance->EGR = TIM_EGR_UG;   /* 프리스케일러 즉시 반영 */
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);

  dma_buf_clear();
  return HAL_OK;
}

#endif /* ADC_DUAL_USE_CUBEMX_INIT */

HAL_StatusTypeDef ADC_Dual_Start(void)
{
  /* 캘리브레이션은 ADC가 비활성 상태일 때 각각 수행해야 합니다. */
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* 마스터와 슬레이브를 모두 Enable하고 CDR → s_dma_buf로 DMA를 시작합니다.
   * 외부 트리거 모드이므로 실제 변환은 TIM6 TRGO가 들어올 때 시작됩니다. */
  if (HAL_ADCEx_MultiModeStart_DMA(&hadc1, s_dma_buf, ADC_DUAL_DMA_WORDS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_TIM_Base_Start(&htim6);
}

HAL_StatusTypeDef ADC_Dual_Stop(void)
{
  (void)HAL_TIM_Base_Stop(&htim6);
  return HAL_ADCEx_MultiModeStop_DMA(&hadc1);
}

void ADC_Dual_GetStats(ADC_Dual_Stats_t *out)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  memcpy(out, (const void *)&s_stats, sizeof(*out));
  __set_PRIMASK(primask);
}

uint32_t ADC_Dual_GetErrorCount(void)
{
  return s_error_count;
}

__weak void ADC_Dual_BlockCallback(const uint32_t *raw, uint32_t scans)
{
  (void)raw;
  (void)scans;
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백                                                                    */
/* ------------------------------------------------------------------------- */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    on_half_ready(&s_dma_buf[0]);
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    on_half_ready(&s_dma_buf[ADC_DUAL_DMA_WORDS / 2U]);
  }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  (void)hadc;
  s_error_count++;
  /* 오버런 플래그는 HAL_ADC_IRQHandler에서 지워집니다. DMA circular는 계속 동작합니다. */
}

#if !ADC_DUAL_USE_CUBEMX_INIT
/* ------------------------------------------------------------------------- */
/* 인터럽트 핸들러                                                             */
/*  CubeMX 모드에서는 CubeMX가 생성한 stm32h7xx_it.c의 핸들러를 씁니다.          */
/* ------------------------------------------------------------------------- */
void DMA1_Stream0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_adc1);
}

void ADC_IRQHandler(void)
{
  HAL_ADC_IRQHandler(&hadc1);
}
#endif /* !ADC_DUAL_USE_CUBEMX_INIT */
