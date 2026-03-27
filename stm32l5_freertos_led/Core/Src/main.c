/**
 * @file main.c
 * @brief STM32L5 FreeRTOS - 5개 LED 토글 태스크
 *
 * 구성:
 *   - MCU   : STM32L552/562 (Cortex-M33)
 *   - RTOS  : FreeRTOS (ARM_CM33_NTZ 포트)
 *   - 틱 소스: TIM6 (vPortSetupTimerInterrupt 오버라이드, tim6_tick.c)
 *   - LEDs  : PB0~PB4 (5개), 각각 독립 태스크에서 서로 다른 주기로 토글
 *
 * 태스크 주기:
 *   Task1(LED1/PB0): 500 ms
 *   Task2(LED2/PB1): 300 ms
 *   Task3(LED3/PB2): 700 ms
 *   Task4(LED4/PB3): 1000 ms
 *   Task5(LED5/PB4): 200 ms
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* ---------------------------------------------------------------
 * 태스크 파라미터 구조체
 * --------------------------------------------------------------- */
typedef struct {
    uint16_t    pin;        /* GPIO 핀 번호 */
    uint32_t    period_ms;  /* 토글 주기 (ms) */
    const char *name;       /* 태스크 이름 (디버그용) */
} LedTaskParam_t;

/* 5개 LED 파라미터 (상수 -> ROM에 배치) */
static const LedTaskParam_t led_params[5] = {
    { LED1_PIN, LED1_PERIOD_MS, "LED1" },
    { LED2_PIN, LED2_PERIOD_MS, "LED2" },
    { LED3_PIN, LED3_PERIOD_MS, "LED3" },
    { LED4_PIN, LED4_PERIOD_MS, "LED4" },
    { LED5_PIN, LED5_PERIOD_MS, "LED5" },
};

/* ---------------------------------------------------------------
 * 정적 할당용 태스크 TCB / 스택
 * (configSUPPORT_STATIC_ALLOCATION = 1)
 * --------------------------------------------------------------- */
static StaticTask_t  led_task_tcb[5];
static StackType_t   led_task_stack[5][LED_TASK_STACK_SIZE];
static TaskHandle_t  led_task_handle[5];

/* Idle 태스크 정적 할당 버퍼 */
static StaticTask_t  idle_task_tcb;
static StackType_t   idle_task_stack[configMINIMAL_STACK_SIZE];

/* ---------------------------------------------------------------
 * 내부 함수 선언
 * --------------------------------------------------------------- */
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void LedToggleTask(void *pvParam);

/* ---------------------------------------------------------------
 * main
 * --------------------------------------------------------------- */
int main(void)
{
    /* HAL 초기화 (SysTick은 TIM6으로 대체되므로 이후 재구성됨) */
    HAL_Init();

    /* 시스템 클럭 설정 (80 MHz PLL, MSI -> PLL) */
    SystemClock_Config();

    /* GPIO 초기화 (LED 핀) */
    GPIO_Init();

    /* -------------------------------------------------------
     * FreeRTOS 태스크 생성 (정적 할당)
     * ------------------------------------------------------- */
    for (int i = 0; i < 5; i++)
    {
        led_task_handle[i] = xTaskCreateStatic(
            LedToggleTask,              /* 태스크 함수 */
            led_params[i].name,         /* 태스크 이름 */
            LED_TASK_STACK_SIZE,        /* 스택 크기 (words) */
            (void *)&led_params[i],     /* 파라미터 */
            LED_TASK_PRIORITY,          /* 우선순위 */
            led_task_stack[i],          /* 스택 버퍼 */
            &led_task_tcb[i]            /* TCB 버퍼 */
        );

        configASSERT(led_task_handle[i] != NULL);
    }

    /* 스케줄러 시작 -> vPortSetupTimerInterrupt() 가 TIM6 설정 */
    vTaskStartScheduler();

    /* 여기까지 오면 오류 */
    Error_Handler();
    return 0;
}

/* ---------------------------------------------------------------
 * LED 토글 태스크
 * 동일한 함수를 5개 태스크에서 공유, 파라미터로 핀/주기 구분
 * --------------------------------------------------------------- */
static void LedToggleTask(void *pvParam)
{
    const LedTaskParam_t *p = (const LedTaskParam_t *)pvParam;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(p->period_ms);

    for (;;)
    {
        HAL_GPIO_TogglePin(LED_PORT, p->pin);

        /* 정확한 주기 유지 (드리프트 방지) */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ---------------------------------------------------------------
 * 시스템 클럭 설정
 * MSI 4 MHz -> PLL -> 80 MHz (HCLK)
 * PCLK1 = 80 MHz (APB1, TIM6 클럭 소스)
 * --------------------------------------------------------------- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 전원 전압 범위 설정 (Range 1 = 최대 80 MHz) */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        Error_Handler();
    }

    /* MSI 4 MHz를 PLL 소스로 사용 */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_6;   /* 4 MHz */
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM            = 1;   /* /1  -> 4 MHz */
    RCC_OscInitStruct.PLL.PLLN            = 40;  /* x40 -> 160 MHz VCO */
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;  /* /2 -> 80 MHz */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* 버스 클럭 설정 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1
                                     | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 80 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;     /* PCLK1 = 80 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* PCLK2 = 80 MHz */

    /* Flash 레이턴시: 80 MHz / Range1 -> 4WS */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ---------------------------------------------------------------
 * GPIO 초기화 (LED 핀 출력 설정)
 * --------------------------------------------------------------- */
static void GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIOB 클럭 활성화 */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 모든 LED 핀 초기 상태: LOW (소등) */
    HAL_GPIO_WritePin(LED_PORT,
                      LED1_PIN | LED2_PIN | LED3_PIN | LED4_PIN | LED5_PIN,
                      GPIO_PIN_RESET);

    /* Push-Pull 출력, No Pull, Low Speed */
    GPIO_InitStruct.Pin   = LED1_PIN | LED2_PIN | LED3_PIN | LED4_PIN | LED5_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
}

/* ---------------------------------------------------------------
 * FreeRTOS 정적 할당 콜백
 * configSUPPORT_STATIC_ALLOCATION = 1 시 필수 구현
 * --------------------------------------------------------------- */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &idle_task_tcb;
    *ppxIdleTaskStackBuffer = idle_task_stack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

/* ---------------------------------------------------------------
 * FreeRTOS Stack Overflow Hook
 * --------------------------------------------------------------- */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    Error_Handler();
}

/* ---------------------------------------------------------------
 * Malloc Failed Hook
 * --------------------------------------------------------------- */
void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

/* ---------------------------------------------------------------
 * Error Handler
 * --------------------------------------------------------------- */
void Error_Handler(void)
{
    __disable_irq();
    for (;;)
    {
        /* 디버거 연결 후 이 루프에서 원인 파악 */
    }
}

/* ---------------------------------------------------------------
 * HAL SysTick 콜백 비활성화
 * TIM6 를 틱 소스로 사용하므로 HAL의 SysTick 증가 함수 재정의.
 * HAL_Init() 이 SysTick 을 1 ms 로 설정하지만,
 * 스케줄러 시작 후 TIM6 가 틱을 제공하므로 충돌 방지를 위해
 * HAL 타임베이스를 TIM6가 아닌 다른 타이머(예: TIM7)로 변경하거나
 * 아래처럼 SysTick_Handler에서 HAL_IncTick 만 호출하도록 유지한다.
 *
 * 주의: 이 예제에서는 HAL 타임베이스를 SysTick으로 유지하고
 *       FreeRTOS 틱만 TIM6로 공급한다.
 *       SysTick은 HAL 전용(HAL_Delay, HAL_GetTick)으로만 사용된다.
 * --------------------------------------------------------------- */
