/**
 ******************************************************************************
 * @file    main.c
 * @brief   STM32L5 FreeRTOS Voltage Control - Main Entry Point
 ******************************************************************************
 *
 * 이 파일은 STM32CubeMX가 생성하는 main.c의 구조를 따릅니다.
 * CubeMX로 프로젝트를 생성한 후, USER CODE 영역에 아래 코드를 삽입하세요.
 *
 * CubeMX 자동생성 코드와 사용자 코드의 통합 가이드:
 *   1. CubeMX에서 프로젝트 생성 (설정 문서 참고)
 *   2. 생성된 main.c의 USER CODE BEGIN/END 사이에 아래 코드 삽입
 *   3. Drivers 폴더의 파일들을 프로젝트에 추가
 *   4. Include path에 Drivers 하위 폴더 추가
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os2.h"
#include "app_freertos.h"

/* USER CODE BEGIN Includes */
/* (CubeMX가 자동으로 peripheral 헤더를 포함함) */
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* CubeMX가 아래 핸들들을 자동 생성합니다:
 *   UART_HandleTypeDef huart1;
 *   SPI_HandleTypeDef  hspi1;
 *   DAC_HandleTypeDef  hdac1;
 *   TIM_HandleTypeDef  htim2;
 *   TIM_HandleTypeDef  htim3;
 *   ADC_HandleTypeDef  hadc1;
 *   FDCAN_HandleTypeDef hfdcan1;
 *   DMA_HandleTypeDef  hdma_usart1_rx;
 *   DMA_HandleTypeDef  hdma_usart1_tx;
 *   DMA_HandleTypeDef  hdma_adc1;
 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
extern void App_Init(void);  /* Defined in main_app.c */
/* USER CODE END PFP */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* MCU Configuration -------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */
    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    /* CubeMX 자동생성 함수들:
     * MX_GPIO_Init();
     * MX_DMA_Init();        ← 반드시 DMA를 먼저 초기화!
     * MX_USART1_UART_Init();
     * MX_SPI1_Init();
     * MX_DAC1_Init();
     * MX_TIM2_Init();
     * MX_TIM3_Init();
     * MX_ADC1_Init();
     * MX_FDCAN1_Init();
     */

    /* USER CODE BEGIN 2 */

    /* ── Application-level initialization ──────────────────────────────── */
    App_Init();

    /* ── Create FreeRTOS objects (tasks, queues, mutexes, semaphores) ─── */
    App_FreeRTOS_Init();

    /* USER CODE END 2 */

    /* ── Start FreeRTOS Scheduler ──────────────────────────────────────── */
    /* Init scheduler */
    osKernelInitialize();

    /* NOTE: App_FreeRTOS_Init()에서 이미 태스크/큐/뮤텍스 생성 완료.
     * CubeMX에서 FreeRTOS를 활성화하면, MX_FREERTOS_Init()이 자동 호출됩니다.
     * 이 경우 App_FreeRTOS_Init() 호출을 MX_FREERTOS_Init() 내부로 이동하세요. */

    /* Start scheduler */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */
    /* Infinite loop */
    for (;;) {
    }
}

/**
 * @brief System Clock Configuration
 *        HSE 8MHz → PLL → SYSCLK 110MHz
 *
 * 주의: 이 함수는 CubeMX가 자동 생성합니다.
 * 아래는 참고용 수동 구현입니다.
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK) {
        Error_Handler();
    }

    /* Configure HSE and PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 55;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* Select PLL as system clock source and configure bus clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  This function is executed in case of error occurrence.
 */
void Error_Handler(void)
{
    __disable_irq();
    /* Blink fault LED */
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
        for (volatile uint32_t i = 0; i < 500000; i++);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
