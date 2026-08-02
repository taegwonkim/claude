/*
 * FreeRTOS가 SysTick을 커널 틱으로 전용 사용하므로, HAL의 1ms 타임베이스
 * (HAL_GetTick/HAL_Delay가 의존)는 별도 타이머(TIM6)로 옮긴다.
 * CubeMX에서 SYS -> Timebase Source를 TIM6로 지정하면 이 파일이 자동 생성된다.
 */
#include "stm32l5xx_hal.h"

TIM_HandleTypeDef htim6; /* stm32l5xx_it.c의 TIM6_IRQHandler에서 extern으로 참조 */

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef clkconfig;
    uint32_t uwTimclock;
    uint32_t uwPrescalerValue;
    uint32_t pFLatency;
    HAL_StatusTypeDef status;

    __HAL_RCC_TIM6_CLK_ENABLE();

    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    if (clkconfig.APB1CLKDivider == RCC_HCLK_DIV1) {
        uwTimclock = HAL_RCC_GetPCLK1Freq();
    } else {
        uwTimclock = 2UL * HAL_RCC_GetPCLK1Freq();
    }

    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

    htim6.Instance = TIM6;
    htim6.Init.Period = (1000000U / 1000U) - 1U;
    htim6.Init.Prescaler = uwPrescalerValue;
    htim6.Init.ClockDivision = 0U;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    status = HAL_TIM_Base_Init(&htim6);
    if (status == HAL_OK) {
        HAL_NVIC_SetPriority(TIM6_IRQn, TickPriority, 0U);
        HAL_NVIC_EnableIRQ(TIM6_IRQn);
        status = HAL_TIM_Base_Start_IT(&htim6);
    }

    return status;
}

void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        HAL_IncTick();
    }
}

/* stm32l5xx_it.c의 TIM6_IRQHandler에서 호출해야 함:
 *   void TIM6_IRQHandler(void) { HAL_TIM_IRQHandler(&htim6); }
 * (CubeMX가 자동 생성) */
