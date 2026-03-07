/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   STM32H5 FileX + GPDMA1/2 메인 애플리케이션
  *
  *  동작 순서:
  *   1. 시스템 클록, HAL 초기화
  *   2. SDMMC1 + GPDMA1 초기화 (SD 카드)
  *   3. USART1 + GPDMA2 초기화 (디버그 UART)
  *   4. FileX 초기화 및 SD 미디어 마운트
  *   5. 파일 쓰기 / 읽기 / 검증 루프
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "app_filex.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 *  전역 핸들 (stm32h5xx_hal_msp.c에서 DMA 핸들 초기화됨)
 * ============================================================================ */
SD_HandleTypeDef   hsd1;
UART_HandleTypeDef huart1;

/* UART DMA 수신 버퍼 (IDLE 감지 + DMA 연속 수신용) */
static uint8_t uart_rx_buf[UART_RX_BUF_SIZE];

/* ============================================================================
 *  함수 프로토타입
 * ============================================================================ */
static void SystemClock_Config(void);
static void MX_SDMMC1_SD_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ICACHE_Init(void);

/* ============================================================================
 *  main
 * ============================================================================ */
int main(void)
{
    /* ----- HAL 초기화 (SysTick, Flash latency 기본 설정) ----- */
    HAL_Init();

    /* ----- 시스템 클록 설정: PLL 250MHz ----- */
    SystemClock_Config();

    /* ----- 명령어 캐시 활성화 (성능 향상) ----- */
    MX_ICACHE_Init();

    /* ----- 주변장치 초기화 ----- */
    MX_USART1_UART_Init();   /* GPDMA2 기반 UART */
    MX_SDMMC1_SD_Init();     /* GPDMA1 기반 SDMMC */

    /* ----- 디버그 메시지 ----- */
    printf("=== STM32H5 FileX GPDMA Demo ===\r\n");
    printf("SDMMC1(GPDMA1) + USART1(GPDMA2)\r\n\r\n");

    /* ----- UART DMA 수신 시작 (IDLE 라인 감지 모드) ----- */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_buf, UART_RX_BUF_SIZE);

    /* ----- FileX 초기화 및 SD 미디어 마운트 ----- */
    if (MX_FileX_Init() != FX_SUCCESS)
    {
        printf("[ERROR] FileX Init failed!\r\n");
        Error_Handler();
    }

    printf("[OK] FileX ready. Starting file operations...\r\n\r\n");

    /* ----- 메인 루프: 파일 쓰기 / 읽기 반복 ----- */
    while (1)
    {
        /* FileX 파일 쓰기 테스트 */
        if (FileX_WriteTest() != FX_SUCCESS)
        {
            printf("[ERROR] Write test failed\r\n");
        }

        /* FileX 파일 읽기 테스트 */
        if (FileX_ReadTest() != FX_SUCCESS)
        {
            printf("[ERROR] Read test failed\r\n");
        }

        /* 디렉터리 목록 출력 */
        FileX_ListDirectory();

        HAL_Delay(3000);
        printf("--- 다음 사이클 ---\r\n\r\n");
    }
}

/* ============================================================================
 *  SystemClock_Config
 *  HSE 25MHz → PLL1 → SYSCLK 250MHz
 * ============================================================================ */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct   = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct   = {0};

    /* SMPS/LDO 출력 전압 설정 (250MHz 동작 시 VOS0 필요) */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0);

    /* HSE 활성화 + PLL1 설정 */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    /*
     * HSE = 25MHz
     * PLL1M = 5  → VCO input = 25/5 = 5MHz
     * PLL1N = 100 → VCO output = 5*100 = 500MHz
     * PLL1P = 2  → SYSCLK = 500/2 = 250MHz
     * PLL1Q = 4  → PLL1Q  = 500/4 = 125MHz (SDMMC)
     * PLL1R = 2  → PLL1R  = 500/2 = 250MHz
     */
    RCC_OscInitStruct.PLL.PLLM            = 5U;
    RCC_OscInitStruct.PLL.PLLN            = 100U;
    RCC_OscInitStruct.PLL.PLLP            = 2U;
    RCC_OscInitStruct.PLL.PLLQ            = 4U;
    RCC_OscInitStruct.PLL.PLLR            = 2U;
    RCC_OscInitStruct.PLL.PLLRGE         = RCC_PLL1_VCIRANGE_1; /* 4-8MHz 범위 */
    RCC_OscInitStruct.PLL.PLLVCOSEL      = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN       = 0U;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* 버스 클록 설정 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2  |
                                  RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 250MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;     /* PCLK1 = 62.5MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;     /* PCLK2 = 125MHz */
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV4;     /* PCLK3 = 62.5MHz */

    /*
     * Flash wait states: 250MHz @ VOS0 → 5 WS
     * (STM32H5 Reference Manual Table 14)
     */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================================
 *  MX_ICACHE_Init
 *  명령어 캐시 활성화 (STM32H5 성능 최적화)
 * ============================================================================ */
static void MX_ICACHE_Init(void)
{
    ICACHE_RegionConfigTypeDef pRegionConfig = {0};

    /* 기본 direct-mapped 모드로 캐시 활성화 */
    if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_ICACHE_Enable() != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================================
 *  MX_SDMMC1_SD_Init
 *  SDMMC1 초기화 (DMA 설정은 HAL_SD_MspInit에서 처리)
 * ============================================================================ */
static void MX_SDMMC1_SD_Init(void)
{
    hsd1.Instance                 = SDMMC1;
    hsd1.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide             = SDMMC_BUS_WIDE_4B;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    /*
     * ClockDiv: PLL1Q(125MHz) / (2 * (4+1)) = 12.5MHz (일반 전송)
     * 초기화는 HAL이 자동으로 ~400kHz로 설정함
     */
    hsd1.Init.ClockDiv            = 4U;

    if (HAL_SD_Init(&hsd1) != HAL_OK)
    {
        printf("[ERROR] SDMMC1 Init failed!\r\n");
        Error_Handler();
    }

    /* 4비트 버스 모드로 전환 */
    if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) != HAL_OK)
    {
        printf("[WARN] 4-bit bus mode failed, staying at 1-bit\r\n");
    }

    printf("[OK] SDMMC1 initialized (GPDMA1 Ch0=TX, Ch1=RX)\r\n");
}

/* ============================================================================
 *  MX_USART1_UART_Init
 *  USART1 DMA 초기화 (DMA 설정은 HAL_UART_MspInit에서 처리)
 * ============================================================================ */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200U;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling       = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler       = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }

    /* FIFO 모드 활성화 (STM32H5 UART 성능 향상) */
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_EnableFifoMode(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================================
 *  HAL UART 콜백: IDLE 라인 감지 + DMA RX 완료
 * ============================================================================ */

/**
 * @brief UART IDLE 라인 감지 또는 DMA 수신 완료 콜백
 * @param huart  UART 핸들
 * @param Size   수신된 바이트 수
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        /* 수신된 데이터 처리 */
        uart_rx_buf[Size] = '\0';
        printf("[UART RX] %u bytes: %s\r\n", Size, uart_rx_buf);

        /* DMA 수신 재시작 */
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_buf, UART_RX_BUF_SIZE);
    }
}

/**
 * @brief UART 에러 콜백
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* 에러 발생 시 DMA 수신 재시작 */
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_buf, UART_RX_BUF_SIZE);
    }
}

/* ============================================================================
 *  printf 리다이렉션 (USART1 DMA TX)
 *
 *  주의: DMA TX는 비동기이므로 tx_buf는 static 배열 사용.
 *        실제 제품에서는 링 버퍼 + 세마포어 구조 권장.
 * ============================================================================ */
int __io_putchar(int ch)
{
    /* 단순 폴링 방식 (초기화 시 디버그용) */
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, UART_DMA_TIMEOUT_MS);
    return ch;
}

/* ============================================================================
 *  Error_Handler
 * ============================================================================ */
void Error_Handler(void)
{
    __disable_irq();
    /* LED 점멸 또는 다른 에러 표시 (보드별 구현) */
    while (1)
    {
        /* 무한 루프로 JTAG 디버거에서 중단점 확인 */
    }
}
