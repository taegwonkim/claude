/**
 ******************************************************************************
 * @file    main.c
 * @brief   STM32L552 Timer Interrupt 사용 예제
 *
 *  [구성]
 *    TIM6 : 1ms   주기 인터럽트 -> 1ms 카운터 증가
 *    TIM2 : 500ms 주기 인터럽트 -> LED 토글 (PA5)
 *    TIM1 : 1s    주기 인터럽트 -> 초 카운터 증가
 *
 *  [인터럽트 흐름]
 *    Timer Overflow
 *      -> TIMx_IRQHandler()           (IRQ Handler)
 *        -> HAL_TIM_IRQHandler()      (HAL 내부 처리)
 *          -> HAL_TIM_PeriodElapsedCallback()  (사용자 콜백)
 *            -> Timer6_1ms_Callback() / Timer2_500ms_Callback() / ...
 ******************************************************************************
 */

#include "stm32l5xx_hal.h"
#include "timer_config.h"

/* ======================== 전역 변수 ======================== */
volatile uint32_t g_tick_1ms  = 0;   /* 1ms 카운터 */
volatile uint32_t g_seconds   = 0;   /* 초 카운터 */
volatile uint8_t  g_led_state = 0;   /* LED 상태 */

/* ======================== Function Prototypes =============== */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void Error_Handler(void);

/* =================================================================
 *  main
 * ================================================================= */
int main(void)
{
    /* HAL 초기화 */
    HAL_Init();

    /* 시스템 클럭 설정 (110MHz) */
    SystemClock_Config();

    /* GPIO 초기화 (LED: PA5) */
    MX_GPIO_Init();

    /* Timer 초기화 및 인터럽트 시작 */
    MX_TIM6_Init();    /* 1ms 주기 */
    MX_TIM2_Init();    /* 500ms 주기 */
    MX_TIM1_Init();    /* 1s 주기 */

    /* 메인 루프 */
    while (1)
    {
        /*
         * 타이머 인터럽트가 백그라운드에서 동작하므로
         * 메인 루프에서는 다른 작업을 수행할 수 있음
         *
         * 예: g_tick_1ms 값을 이용한 시간 측정
         */
        if (g_tick_1ms >= 10000)  /* 10초마다 */
        {
            g_tick_1ms = 0;
            /* 10초마다 수행할 작업 */
        }
    }
}

/* =================================================================
 *  Timer 콜백 함수 구현
 * ================================================================= */

/* TIM6: 1ms마다 호출 */
void Timer6_1ms_Callback(void)
{
    g_tick_1ms++;
}

/* TIM2: 500ms마다 호출 */
void Timer2_500ms_Callback(void)
{
    g_led_state ^= 1;
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
}

/* TIM1: 1초마다 호출 */
void Timer1_1s_Callback(void)
{
    g_seconds++;
}

/* =================================================================
 *  시스템 클럭 설정 - 110MHz (MSI PLL)
 * ================================================================= */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef       RCC_ClkInitStruct = {0};

    /* Power 설정 */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK)
    {
        Error_Handler();
    }

    /* MSI -> PLL -> 110MHz */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_6;  /* 4MHz */
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM            = 1;   /* 4MHz / 1 = 4MHz */
    RCC_OscInitStruct.PLL.PLLN            = 55;   /* 4MHz * 55 = 220MHz (VCO) */
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;  /* 220MHz / 2 = 110MHz */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* SYSCLK, HCLK, PCLK1, PCLK2 설정 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;  /* 110MHz */
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;          /* HCLK  = 110MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;            /* PCLK1 = 110MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;            /* PCLK2 = 110MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* =================================================================
 *  GPIO 초기화 - LED (PA5)
 * ================================================================= */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA5 = LED (NUCLEO-L552ZE-Q 보드 기준) */
    GPIO_InitStruct.Pin   = GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

/* =================================================================
 *  Error Handler
 * ================================================================= */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
