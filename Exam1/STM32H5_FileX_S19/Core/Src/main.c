/**
 ******************************************************************************
 * @file    main.c
 * @brief   STM32H5 + FileX(Standalone) – SD 카드 .s19 파일 추출 → UART 전송
 *
 *  ┌────────────────────────────────────────────────────────────────┐
 *  │  동작 순서                                                     │
 *  │                                                                │
 *  │  1. 시스템 초기화 (250 MHz, SDMMC1, USART3, GPIO)             │
 *  │  2. FileX 초기화 + SD FAT 마운트                               │
 *  │  3. SCAN_ROOT_PATH 에서 .s19 파일 전체 추출                    │
 *  │  4. 매치된 파일명·경로·크기를 UART(115200) 로 전송             │
 *  │  5. 결과 요약 출력 → LED 점멸 루프                             │
 *  │                                                                │
 *  │  UART 출력 :  USART3  115200-8N1  PD8(TX)/PD9(RX)            │
 *  │               → ST-Link VCP (TeraTerm, PuTTY 등)              │
 *  └────────────────────────────────────────────────────────────────┘
 *
 *  대상 보드 : NUCLEO-H563ZI (STM32H563ZI)
 ******************************************************************************
 */
#include "main.h"
#include "app_s19_scan.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ── HAL 핸들 정의 ──────────────────────────────────────────────────────── */
SD_HandleTypeDef   hsd1;
UART_HandleTypeDef huart3;

/* ── 함수 원형 ──────────────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SDMMC1_SD_Init(void);
static void MX_USART3_UART_Init(void);

/* ============================================================
 *  UART printf 래퍼
 * ============================================================ */
static void UPrintf(const char *fmt, ...)
{
    char    buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    HAL_UART_Transmit(&huart3, (const uint8_t *)buf,
                      (uint16_t)strlen(buf), 1000U);
}

/* ============================================================
 *  main()
 * ============================================================ */
int main(void)
{
    UINT           ret;
    S19_ScanResult result;

    /* ── 1. 시스템 초기화 ────────────────────────────────────────────── */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART3_UART_Init();
    MX_SDMMC1_SD_Init();

    UPrintf("\r\n");
    UPrintf("############################################################\r\n");
    UPrintf("  STM32H5  FileX(Standalone)  .s19 File Scanner\r\n");
    UPrintf("  Board : NUCLEO-H563ZI\r\n");
    UPrintf("  Root  : %s\r\n", SCAN_ROOT_PATH);
    UPrintf("  Recur : %s\r\n", SCAN_RECURSIVE ? "ON" : "OFF");
    UPrintf("############################################################\r\n\r\n");

    /* ── 2. FileX 초기화 + SD 마운트 ─────────────────────────────────── */
    UPrintf("[1/3] SD 카드 마운트 중...\r\n");

    ret = S19_Init();
    if (ret != FX_SUCCESS)
    {
        UPrintf("  FAIL: SD 마운트 오류 (FX code: 0x%02X)\r\n", ret);
        UPrintf("  확인사항:\r\n");
        UPrintf("    - SD 카드가 슬롯에 삽입되어 있는가?\r\n");
        UPrintf("    - FAT32 로 포맷되어 있는가?\r\n");
        UPrintf("    - SDMMC1 핀/클럭 설정이 올바른가?\r\n");
        Error_Handler();
    }
    UPrintf("  OK: SD FAT 마운트 성공\r\n\r\n");

    /* ── 3. .s19 파일 스캔 + UART 전송 ───────────────────────────────── */
    UPrintf("[2/3] .s19 파일 스캔 + UART 전송 중...\r\n");

    ret = S19_ScanAndSendUART(SCAN_ROOT_PATH,
                               &huart3,
                               SCAN_RECURSIVE,
                               &result);

    if (ret != FX_SUCCESS)
    {
        UPrintf("  WARN: 스캔 중 오류 발생 (FX code: 0x%02X)\r\n", ret);
        /*
         * 주요 오류 코드:
         *   0x04 FX_NOT_FOUND       : 경로 없음
         *   0x05 FX_IO_ERROR        : SD 읽기 실패
         *   0x0A FX_NO_MORE_ENTRIES : 정상 종료 (내부 처리됨)
         */
    }

    /* ── 결과 요약 (변수 활용 예시) ──────────────────────────────────── */
    if (result.found == 0U)
    {
        UPrintf("  >> .s19 파일이 없습니다. SD 카드 내용을 확인하세요.\r\n\r\n");
    }
    else
    {
        UPrintf("  >> 총 %u 개 .s19 파일 발견 (%lu 바이트)\r\n\r\n",
                result.found, result.total_bytes);
    }

    /* ── 4. FileX 종료 ───────────────────────────────────────────────── */
    UPrintf("[3/3] FileX 종료...\r\n");
    S19_DeInit();
    UPrintf("  OK: 미디어 해제 완료\r\n\r\n");

    UPrintf("############################################################\r\n");
    UPrintf("  완료. 녹색 LED 점멸 루프 진입.\r\n");
    UPrintf("############################################################\r\n");

    /* ── 5. LED 점멸 무한 루프 ───────────────────────────────────────── */
    while (1)
    {
        HAL_GPIO_TogglePin(LED_GREEN_PORT, LED_GREEN_PIN);
        HAL_Delay(500U);
    }
}

/* ============================================================
 *  주변 장치 초기화
 * ============================================================ */

/**
 * @brief  SDMMC1 초기화
 *         4비트 버스, IDMA 모드
 *         ClockDiv=4 → SDMMC_CK ≈ 12.5 MHz (PLL1Q 100 MHz 기준)
 */
static void MX_SDMMC1_SD_Init(void)
{
    hsd1.Instance                 = SDMMC1;
    hsd1.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide             = SDMMC_BUS_WIDE_4B;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv            = 4U;

    if (HAL_SD_Init(&hsd1) != HAL_OK)
        Error_Handler();
    if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) != HAL_OK)
        Error_Handler();
}

/**
 * @brief  USART3 초기화 (115200-8N1)
 *         NUCLEO-H563ZI: PD8(TX) / PD9(RX) → ST-Link VCP
 */
static void MX_USART3_UART_Init(void)
{
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200U;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK)
        Error_Handler();
}

/**
 * @brief  GPIO 초기화 – LED (NUCLEO-H563ZI)
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_GREEN_PORT,  LED_GREEN_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT,    LED_RED_PIN,    GPIO_PIN_RESET);

    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = LED_GREEN_PIN;
    HAL_GPIO_Init(LED_GREEN_PORT, &gpio);
    gpio.Pin = LED_YELLOW_PIN;
    HAL_GPIO_Init(LED_YELLOW_PORT, &gpio);
    gpio.Pin = LED_RED_PIN;
    HAL_GPIO_Init(LED_RED_PORT, &gpio);
}

/**
 * @brief  시스템 클럭 설정
 *         HSE(8 MHz) → PLL1 → SYSCLK 250 MHz
 *         PLL1Q = 100 MHz → SDMMC1 커널 클럭
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_BYPASS;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 1U;
    osc.PLL.PLLN       = 125U;
    osc.PLL.PLLP       = 4U;       /* SYSCLK = 250 MHz */
    osc.PLL.PLLQ       = 10U;      /* SDMMC  = 100 MHz */
    osc.PLL.PLLR       = 2U;
    osc.PLL.PLLRGE     = RCC_PLL1_VCIRANGE_1;
    osc.PLL.PLLVCOSEL  = RCC_PLL1_VCORANGE_WIDE;
    osc.PLL.PLLFRACN   = 0U;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                       | RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    clk.APB3CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
}

/* ============================================================
 *  오류 핸들러
 * ============================================================ */
void Error_Handler(void)
{
    __disable_irq();
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
    Error_Handler();
}
#endif
