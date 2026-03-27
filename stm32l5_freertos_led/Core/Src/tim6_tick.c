/**
 * @file tim6_tick.c
 * @brief FreeRTOS 틱을 TIM6으로 공급하는 포트 오버라이드 구현
 *
 * configOVERRIDE_DEFAULT_TICK_CONFIGURATION = 1 로 설정하면
 * FreeRTOS 포트는 vPortSetupTimerInterrupt() 를 외부에서 구현할 것을 요구함.
 *
 * TIM6 설정:
 *   - APB1 타이머 클럭에서 1 ms 주기(configTICK_RATE_HZ = 1000) 생성
 *   - 업데이트 인터럽트(TIM6_DAC_IRQn) 에서 xPortSysTickHandler() 호출
 */

#include "stm32l5xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

/* TIM6 핸들 (main.c 에서도 접근 가능하도록 외부 공개) */
TIM_HandleTypeDef htim6;

/**
 * @brief  FreeRTOS 포트가 호출하는 틱 타이머 초기화 함수
 *         (configOVERRIDE_DEFAULT_TICK_CONFIGURATION = 1 시 필수 구현)
 *
 * APB1 타이머 클럭(PCLK1)을 기반으로 TIM6 주기를 계산한다.
 * STM32L5의 기본 클럭 트리: MSI 4 MHz -> HSI 16 MHz 또는
 * PLL -> SystemCoreClock.
 * HAL_RCC_GetPCLK1TimFreq() 로 실제 타이머 입력 클럭을 얻는다.
 */
void vPortSetupTimerInterrupt(void)
{
    uint32_t ulTimerClockHz;
    uint32_t ulPrescaler;
    uint32_t ulPeriod;

    /* APB1 타이머 클럭 획득 */
    ulTimerClockHz = HAL_RCC_GetPCLK1TimFreq();

    /*
     * TIM6 주기 계산:
     *   원하는 틱 주기 = 1 / configTICK_RATE_HZ  (예: 1 ms)
     *   Period  = (TimerClk / (Prescaler+1) / configTICK_RATE_HZ) - 1
     *
     * Prescaler = 0 으로 두고, Period 를 직접 계산.
     * Period 가 65535 를 초과하면 Prescaler 를 올린다.
     */
    ulPrescaler = ( ulTimerClockHz / ( 65535u * configTICK_RATE_HZ ) );
    ulPeriod    = ( ulTimerClockHz / ( ( ulPrescaler + 1u ) * configTICK_RATE_HZ ) ) - 1u;

    /* RCC TIM6 클럭 활성화 */
    __HAL_RCC_TIM6_CLK_ENABLE();

    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = (uint16_t)ulPrescaler;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = (uint16_t)ulPeriod;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
    {
        /* 초기화 실패 시 Error_Handler 호출 */
        extern void Error_Handler(void);
        Error_Handler();
    }

    /* 업데이트 인터럽트 활성화 */
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);

    /* NVIC 우선순위 설정 (커널보다 낮아야 함) */
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);

    /* 타이머 시작 */
    __HAL_TIM_ENABLE(&htim6);
}

/**
 * @brief  TIM6 & DAC 전역 인터럽트 핸들러
 *
 * FreeRTOS 틱 핸들러(xPortSysTickHandler)를 직접 호출한다.
 * xPortSysTickHandler 는 스케줄러가 실행 중일 때만 안전하게 호출 가능.
 */
void TIM6_DAC_IRQHandler(void)
{
    /* 업데이트 플래그 클리어 */
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);

    /* FreeRTOS 틱 처리 */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}
