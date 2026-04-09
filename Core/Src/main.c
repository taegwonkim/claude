/* Core/Src/main.c
 *
 * STM32L552R - TIM7 폴링 방식으로 GPIO 주기 제어 예제
 *
 * [핵심 원리]
 *  - HAL_TIM_Base_Start()  → 타이머 시작 (인터럽트 비활성)
 *  - 메인 루프에서 TIM_FLAG_UPDATE 비트를 직접 확인 (폴링)
 *  - 플래그가 셋되면 수동으로 클리어 후 GPIO 토글
 *  - NVIC 설정 없음, HAL_TIM_Base_Start_IT() 사용 안 함
 *
 * [타이머 주기 계산]
 *  TIM7 클럭 = APB1 = MSI 4 MHz (기본값)
 *  Toggle 주기 = (PSC+1) × (ARR+1) / CLK = 4000 × 500 / 4_000_000 = 0.5 s
 */

#include "main.h"

/* =========================================================================
 * 전역 핸들
 * ========================================================================= */
static TIM_HandleTypeDef htim7;

/* =========================================================================
 * 함수 선언
 * ========================================================================= */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM7_Init(void);

/* =========================================================================
 * main
 * ========================================================================= */
int main(void)
{
    /* 1) HAL 초기화 (SysTick 1 ms 설정 포함) */
    HAL_Init();

    /* 2) 시스템 클럭 설정 (MSI 4 MHz 기본값 사용) */
    SystemClock_Config();

    /* 3) 주변장치 초기화 */
    MX_GPIO_Init();
    MX_TIM7_Init();

    /* 4) TIM7 시작 - 인터럽트 없이 순수 카운터만 동작
     *    (HAL_TIM_Base_Start_IT() 가 아닌 HAL_TIM_Base_Start() 사용) */
    if (HAL_TIM_Base_Start(&htim7) != HAL_OK)
    {
        Error_Handler();
    }

    /* =====================================================================
     * 메인 루프
     *
     * TIM7 Update 플래그(UIF) 폴링:
     *   - __HAL_TIM_GET_FLAG() 로 UIF 비트 확인
     *   - 셋되어 있으면 __HAL_TIM_CLEAR_FLAG() 로 클리어 후 GPIO 처리
     *   - 인터럽트 핸들러 없이 동일한 동작 구현
     * ===================================================================== */
    while (1)
    {
        /* Update 이벤트(오버플로) 발생 확인 */
        if (__HAL_TIM_GET_FLAG(&htim7, TIM_FLAG_UPDATE) != RESET)
        {
            /* 반드시 플래그를 수동으로 클리어해야 합니다.
             * (인터럽트 핸들러에서는 HAL이 자동으로 처리하지만,
             *  폴링 모드에서는 직접 클리어해야 합니다.) */
            __HAL_TIM_CLEAR_FLAG(&htim7, TIM_FLAG_UPDATE);

            /* GPIO 토글 */
            HAL_GPIO_TogglePin(LED_GREEN_PORT, LED_GREEN_PIN);
            HAL_GPIO_TogglePin(LED_BLUE_PORT,  LED_BLUE_PIN);

            /* 필요시 여기서 다른 주기적 작업 수행 */
        }

        /* 폴링 사이에 다른 비주기적 작업을 여기서 처리할 수 있습니다. */
    }
}

/* =========================================================================
 * SystemClock_Config
 *  MSI 4 MHz 기본값 그대로 사용 (별도 PLL 설정 없음)
 *  → TIM7 클럭 = APB1 = 4 MHz
 * ========================================================================= */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* MSI 4 MHz 활성화 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState       = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange  = RCC_MSIRANGE_6;  /* 4 MHz */
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* SYSCLK ← MSI, HCLK/PCLK1/PCLK2 분주 없음 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

/* =========================================================================
 * MX_TIM7_Init
 *  - 기본 타이머 모드 (입력 캡처/PWM 없음)
 *  - NVIC 등록 없음 → 인터럽트 비활성
 * ========================================================================= */
static void MX_TIM7_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    /* APB1 클럭 게이트 ON */
    __HAL_RCC_TIM7_CLK_ENABLE();

    htim7.Instance               = TIM7;
    htim7.Init.Prescaler         = TIM7_PRESCALER;        /* 4000-1 → 1 kHz tick */
    htim7.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim7.Init.Period            = TIM7_PERIOD;            /* 500-1  → 500 ms     */
    htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
    {
        Error_Handler();
    }

    /* TRGO 출력 설정 (다른 타이머 연동 불필요하면 기본값) */
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* ※ HAL_NVIC_SetPriority / HAL_NVIC_EnableIRQ(TIM7_IRQn) 호출 안 함
     *    → 인터럽트 핸들러(TIM7_IRQHandler) 불필요 */
}

/* =========================================================================
 * MX_GPIO_Init
 *  LED 핀을 Push-Pull 출력으로 설정
 * ========================================================================= */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIOB, GPIOC 클럭 ON */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* 초기 출력 상태: OFF (Low) */
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_PORT,  LED_BLUE_PIN,  GPIO_PIN_RESET);

    /* Green LED (PC7) */
    GPIO_InitStruct.Pin   = LED_GREEN_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GREEN_PORT, &GPIO_InitStruct);

    /* Blue LED (PB7) */
    GPIO_InitStruct.Pin = LED_BLUE_PIN;
    HAL_GPIO_Init(LED_BLUE_PORT, &GPIO_InitStruct);
}

/* =========================================================================
 * Error_Handler
 * ========================================================================= */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        /* 오류 발생 시 여기서 멈춤 - 디버거로 확인 */
    }
}
