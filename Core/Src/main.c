/**
 ******************************************************************************
 * @file    main.c
 * @brief   4채널 PID 전압 제어 시스템 - 메인 애플리케이션
 *
 * 시스템 초기화 순서:
 *   1. HAL 초기화
 *   2. 시스템 클럭 설정 (110MHz)
 *   3. GPIO 초기화 (CS, LED, IRQ 핀)
 *   4. SPI1 초기화 (AD5641 DAC용)
 *   5. SPI2 초기화 (MCP3465R ADC용)
 *   6. USART2 초기화 (디버그/모니터)
 *   7. 전압 제어 시스템 초기화 (VCS_Init)
 *   8. FreeRTOS 태스크 생성 및 스케줄러 시작
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "voltage_control.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
UART_HandleTypeDef huart2;

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART2_UART_Init(void);

/* Main function -------------------------------------------------------------*/

int main(void)
{
    /* HAL 라이브러리 초기화 (SysTick, Flash prefetch 등) */
    HAL_Init();

    /* 시스템 클럭 설정: 110MHz (MSI PLL) */
    SystemClock_Config();

    /* 주변 장치 초기화 */
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_USART2_UART_Init();

    /* 초기화 완료 LED 표시 */
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_SET);

    /* 전압 제어 시스템 초기화 */
    if (VCS_Init() != HAL_OK) {
        /* 초기화 실패 시 에러 LED 켜고 멈춤 */
        HAL_GPIO_WritePin(LED_ERROR_PORT, LED_ERROR_PIN, GPIO_PIN_SET);
        Error_Handler();
    }

    /* FreeRTOS 태스크 생성 */
    if (VCS_StartTasks() != HAL_OK) {
        HAL_GPIO_WritePin(LED_ERROR_PORT, LED_ERROR_PIN, GPIO_PIN_SET);
        Error_Handler();
    }

    /* FreeRTOS 스케줄러 시작 (이 이후로 main()은 반환하지 않음) */
    vTaskStartScheduler();

    /* 여기에 도달하면 안됨 (메모리 부족 등 치명적 오류) */
    Error_Handler();

    return 0;
}

/* System Clock Configuration ------------------------------------------------*/

/**
 * @brief  시스템 클럭을 110MHz로 설정
 *
 * 클럭 소스: MSI (4MHz) → PLL → 110MHz
 * AHB  = 110MHz
 * APB1 = 110MHz
 * APB2 = 110MHz
 * Flash Latency: 5 wait states (110MHz @ 1.2V)
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 전압 레귤레이터 출력 범위 설정 (Range 0 = Boost mode for 110MHz) */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK) {
        Error_Handler();
    }

    /* MSI 오실레이터 및 PLL 설정 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;  /* 4MHz */
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM = 1;    /* 4MHz / 1 = 4MHz */
    RCC_OscInitStruct.PLL.PLLN = 55;   /* 4MHz × 55 = 220MHz VCO */
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;  /* 220MHz / 2 = 110MHz */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* CPU, AHB, APB 버스 클럭 설정 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/* Peripheral Initialization -------------------------------------------------*/

/**
 * @brief  GPIO 초기화
 *
 * 설정 핀:
 *   - DAC CS0~CS3: 출력 (Push-Pull, High 초기값)
 *   - ADC CS: 출력 (Push-Pull, High 초기값)
 *   - ADC IRQ: 입력 (풀업, 하강 에지 인터럽트)
 *   - LED_STATUS, LED_ERROR: 출력 (Push-Pull)
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO 포트 클럭 활성화 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* --- DAC CS 핀 (PA4, PB0, PB1, PB2): 출력, High 초기값 --- */
    HAL_GPIO_WritePin(DAC_CS0_PORT, DAC_CS0_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = DAC_CS0_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DAC_CS0_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(DAC_CS1_PORT, DAC_CS1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC_CS2_PORT, DAC_CS2_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC_CS3_PORT, DAC_CS3_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = DAC_CS1_PIN | DAC_CS2_PIN | DAC_CS3_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* --- ADC CS 핀 (PB12): 출력, High 초기값 --- */
    HAL_GPIO_WritePin(ADC_CS_PORT, ADC_CS_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = ADC_CS_PIN;
    HAL_GPIO_Init(ADC_CS_PORT, &GPIO_InitStruct);

    /* --- ADC IRQ 핀 (PC6): 입력, 풀업 --- */
    GPIO_InitStruct.Pin = ADC_IRQ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(ADC_IRQ_PORT, &GPIO_InitStruct);

    /* --- Status LEDs (PC8, PC9): 출력 --- */
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ERROR_PORT, LED_ERROR_PIN, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = LED_STATUS_PIN | LED_ERROR_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/**
 * @brief  SPI1 초기화 (AD5641 DAC 통신용)
 *
 * 설정:
 *   - Master mode
 *   - CPOL=1, CPHA=1 (Mode 3, AD5641 지원)
 *   - 8-bit data size
 *   - MSB first
 *   - Software NSS (CS는 GPIO로 수동 제어)
 *   - Prescaler: /16 → 110MHz/16 ≈ 6.875MHz (AD5641 max 30MHz)
 */
static void MX_SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  SPI2 초기화 (MCP3465R ADC 통신용)
 *
 * 설정:
 *   - Master mode
 *   - CPOL=0, CPHA=0 (Mode 0, MCP346x 지원)
 *   - 8-bit data size
 *   - MSB first
 *   - Software NSS
 *   - Prescaler: /32 → 110MHz/32 ≈ 3.4375MHz (MCP3465R max 20MHz)
 */
static void MX_SPI2_Init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();

    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  USART2 초기화 (디버그/모니터링)
 *
 * 설정:
 *   - 115200 baud
 *   - 8N1
 *   - TX/RX 모드
 */
static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

/* Error Handler -------------------------------------------------------------*/

void Error_Handler(void)
{
    __disable_irq();
    HAL_GPIO_WritePin(LED_ERROR_PORT, LED_ERROR_PIN, GPIO_PIN_SET);
    while (1) {
        /* 무한 루프 - 디버거로 확인 */
    }
}

/* FreeRTOS Hook Functions ---------------------------------------------------*/

/**
 * @brief  FreeRTOS 스택 오버플로 감지 콜백
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    Error_Handler();
}

/**
 * @brief  FreeRTOS 메모리 할당 실패 콜백
 */
void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

/**
 * @brief  FreeRTOS Idle 태스크 hook
 */
void vApplicationIdleHook(void)
{
    /* 저전력 모드 진입 가능 */
}

/**
 * @brief  FreeRTOS Tick hook
 */
void vApplicationTickHook(void)
{
    /* 매 틱마다 호출 */
}

/* Assert failed handler (HAL debug) -----------------------------------------*/
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
