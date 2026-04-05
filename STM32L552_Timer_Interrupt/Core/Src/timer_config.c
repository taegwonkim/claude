/**
 ******************************************************************************
 * @file    timer_config.c
 * @brief   STM32L552 Timer Interrupt 설정 및 콜백 처리
 *
 *  [Timer 주파수 계산 공식]
 *    Update Freq = Timer_CLK / ((Prescaler + 1) * (Period + 1))
 *
 *  [예시: 110MHz 기준]
 *    1ms   = 110,000,000 / ((10999 + 1) * (9 + 1))    = 1000 Hz
 *    500ms = 110,000,000 / ((10999 + 1) * (4999 + 1))  = 2 Hz
 *    1s    = 110,000,000 / ((10999 + 1) * (9999 + 1))  = 1 Hz
 ******************************************************************************
 */

#include "timer_config.h"

/* ======================== Timer Handles ======================== */
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim1;

/* =================================================================
 *  TIM6 초기화 - Basic Timer (APB1), 1ms 주기
 *  용도: 시스템 틱, 시간 측정, 주기적 폴링
 * ================================================================= */
void MX_TIM6_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    /* TIM6 클럭 활성화 */
    __HAL_RCC_TIM6_CLK_ENABLE();

    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = 10999;   /* 110MHz / (10999+1) = 10kHz */
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = 9;       /* 10kHz / (9+1) = 1000Hz = 1ms */
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
    {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig);

    /* NVIC 인터럽트 설정 */
    HAL_NVIC_SetPriority(TIM6_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM6_IRQn);

    /* 인터럽트 모드로 타이머 시작 */
    HAL_TIM_Base_Start_IT(&htim6);
}

/* =================================================================
 *  TIM2 초기화 - General Purpose Timer (APB1), 500ms 주기
 *  용도: LED 깜빡임, 주기적 센서 읽기
 * ================================================================= */
void MX_TIM2_Init(void)
{
    /* TIM2 클럭 활성화 */
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 10999;   /* 110MHz / (10999+1) = 10kHz */
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 4999;    /* 10kHz / (4999+1) = 2Hz = 500ms */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    /* NVIC 인터럽트 설정 */
    HAL_NVIC_SetPriority(TIM2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    /* 인터럽트 모드로 타이머 시작 */
    HAL_TIM_Base_Start_IT(&htim2);
}

/* =================================================================
 *  TIM1 초기화 - Advanced Timer (APB2), 1초 주기
 *  용도: 정밀 시간 측정, 1초 카운터
 * ================================================================= */
void MX_TIM1_Init(void)
{
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};

    /* TIM1 클럭 활성화 */
    __HAL_RCC_TIM1_CLK_ENABLE();

    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 10999;   /* 110MHz / (10999+1) = 10kHz */
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 9999;    /* 10kHz / (9999+1) = 1Hz = 1s */
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig);

    /* NVIC 인터럽트 설정 */
    HAL_NVIC_SetPriority(TIM1_UP_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);

    /* 인터럽트 모드로 타이머 시작 */
    HAL_TIM_Base_Start_IT(&htim1);
}

/* =================================================================
 *  IRQ Handlers (stm32l5xx_it.c 에 넣어도 됨)
 * ================================================================= */

void TIM6_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

void TIM1_UP_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim1);
}

/* =================================================================
 *  HAL 콜백 - 모든 타이머의 Update 인터럽트가 여기로 들어옴
 *  HAL_TIM_IRQHandler() 내부에서 자동 호출됨
 * ================================================================= */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        Timer6_1ms_Callback();
    }
    else if (htim->Instance == TIM2)
    {
        Timer2_500ms_Callback();
    }
    else if (htim->Instance == TIM1)
    {
        Timer1_1s_Callback();
    }
}
