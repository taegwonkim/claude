/**
 ******************************************************************************
 * @file    main.c
 * @brief   STM32H5 SD 카드 GPDMA 예제 – 읽기/쓰기 테스트 + UART 로그
 *
 *  ┌────────────────────────────────────────────────────────────────────┐
 *  │  DMA 구성 요약                                                     │
 *  │                                                                    │
 *  │  SDMMC1 IDMA ─────────────── SD 카드 블록 R/W (내장 DMA 엔진)    │
 *  │  GPDMA1 Channel 0 ─────────── USART3 TX  (비동기 로그 전송)       │
 *  │                                                                    │
 *  │  두 DMA 는 서로 다른 엔진 + 독립 메모리 버퍼 → 충돌 없음          │
 *  │  SDMMC IRQ(Preempt=4) > GPDMA IRQ(Preempt=6) → SD 우선 처리      │
 *  └────────────────────────────────────────────────────────────────────┘
 *
 *  동작 순서:
 *    1. 시스템 초기화 (250 MHz, GPIO, USART3+GPDMA, SDMMC1)
 *    2. 쓰기: SD_TEST_SECTOR 에 패턴 데이터 기록
 *    3. 읽기: 동일 섹터 읽어 패턴 검증
 *    4. UART 로 결과 출력 (GPDMA 비동기)
 *    5. LED 점멸 루프 (녹색=성공, 빨간=실패)
 *
 *  대상 보드: NUCLEO-H563ZI (STM32H563ZI)
 *  UART 출력: USART3  115200-8N1  PD8(TX) → ST-Link VCP
 ******************************************************************************
 */
#include "main.h"
#include "sd_driver.h"
#include "uart_gpdma.h"
#include <string.h>
#include <stdio.h>

/* ── SD 카드 DMA 버퍼 ───────────────────────────────────────────────────── */
/*
 * 규칙:
 *   ① 32바이트 정렬  : D-Cache 라인 경계 정합 필수
 *   ② .DMABufferSection : 링커가 SRAM 의 DMA 전용 섹션에 배치
 *   ③ SD 버퍼와 UART 버퍼가 완전히 분리된 영역에 위치 (충돌 원천 차단)
 */
static uint8_t sd_wr_buf[SD_TEST_COUNT * 512U]
    __attribute__((aligned(32), section(".DMABufferSection")));

static uint8_t sd_rd_buf[SD_TEST_COUNT * 512U]
    __attribute__((aligned(32), section(".DMABufferSection")));

/* ── 함수 원형 ──────────────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void FillPattern(uint8_t *buf, uint32_t len, uint8_t seed);
static int  VerifyPattern(const uint8_t *buf, uint32_t len, uint8_t seed);

/* ============================================================
 *  main()
 * ============================================================ */
int main(void)
{
    /* ── 1. 시스템 초기화 ──────────────────────────────────────────────── */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    /* UART GPDMA 초기화 (먼저 초기화하여 이후 로그 출력 가능) */
    UART_GPDMA_Init();

    UART_Printf("\r\n");
    UART_Printf("========================================================\r\n");
    UART_Printf("  STM32H563ZI  SD Card GPDMA R/W Test\r\n");
    UART_Printf("  Board  : NUCLEO-H563ZI\r\n");
    UART_Printf("  SD DMA : SDMMC1 IDMA (internal)\r\n");
    UART_Printf("  UART   : GPDMA1 Channel 0 (non-blocking TX)\r\n");
    UART_Printf("========================================================\r\n\r\n");

    /* SD 카드 초기화 */
    UART_Printf("[1/4] SD 카드 초기화...\r\n");
    if (SD_Init() != SD_OK)
    {
        UART_Printf("  FAIL: SD 초기화 실패\r\n");
        UART_Printf("  확인: 카드 삽입, FAT32 포맷, 핀 배선\r\n");
        Error_Handler();
    }
    UART_Printf("  OK: SDMMC1 4-bit IDMA 준비 완료\r\n\r\n");

    /* ── 2. 쓰기 테스트 ────────────────────────────────────────────────── */
    UART_Printf("[2/4] 쓰기 테스트 (섹터 %lu, %u 섹터)...\r\n",
                (unsigned long)SD_TEST_SECTOR, SD_TEST_COUNT);

    /*
     * 테스트 패턴 생성:
     *   각 바이트 = (인덱스 + 시드) & 0xFF
     *   시드: 쓰기=0xA5, 읽기 검증=0xA5
     */
    FillPattern(sd_wr_buf, sizeof(sd_wr_buf), 0xA5U);

    SD_Status st = SD_WriteBlocks(sd_wr_buf, SD_TEST_SECTOR, SD_TEST_COUNT);
    if (st != SD_OK)
    {
        UART_Printf("  FAIL: 쓰기 오류 (code=%d)\r\n", (int)st);
        Error_Handler();
    }
    UART_Printf("  OK: %u 섹터 쓰기 완료 (%u 바이트)\r\n\r\n",
                SD_TEST_COUNT, (unsigned)(SD_TEST_COUNT * 512U));

    /* ── 3. 읽기 테스트 ────────────────────────────────────────────────── */
    UART_Printf("[3/4] 읽기 테스트 (섹터 %lu)...\r\n",
                (unsigned long)SD_TEST_SECTOR);

    memset(sd_rd_buf, 0x00U, sizeof(sd_rd_buf));

    st = SD_ReadBlocks(sd_rd_buf, SD_TEST_SECTOR, SD_TEST_COUNT);
    if (st != SD_OK)
    {
        UART_Printf("  FAIL: 읽기 오류 (code=%d)\r\n", (int)st);
        Error_Handler();
    }
    UART_Printf("  OK: %u 섹터 읽기 완료\r\n", SD_TEST_COUNT);

    /* ── 4. 데이터 검증 ────────────────────────────────────────────────── */
    UART_Printf("[4/4] 데이터 검증...\r\n");

    int mismatch = VerifyPattern(sd_rd_buf, sizeof(sd_rd_buf), 0xA5U);
    if (mismatch != 0)
    {
        UART_Printf("  FAIL: %d 바이트 불일치\r\n", mismatch);

        /* 처음 불일치 지점 덤프 */
        for (uint32_t i = 0; i < sizeof(sd_rd_buf) && mismatch > 0; i++)
        {
            uint8_t expected = (uint8_t)((i + 0xA5U) & 0xFFU);
            if (sd_rd_buf[i] != expected)
            {
                UART_Printf("  [offset=%lu] got=0x%02X expected=0x%02X\r\n",
                            (unsigned long)i, sd_rd_buf[i], expected);
                if (i >= 8U) break;  /* 최대 8개 항목만 출력 */
            }
        }
        Error_Handler();
    }

    UART_Printf("  OK: 모든 바이트 일치 (%u 바이트)\r\n\r\n",
                (unsigned)sizeof(sd_rd_buf));

    /* ── 완료 ──────────────────────────────────────────────────────────── */
    UART_Printf("========================================================\r\n");
    UART_Printf("  테스트 완료 – 녹색 LED 점멸\r\n");
    UART_Printf("========================================================\r\n");

    /* UART 전송이 완전히 끝날 때까지 대기 */
    UART_WaitTxDone();

    /* ── 5. LED 점멸 무한 루프 ─────────────────────────────────────────── */
    while (1)
    {
        HAL_GPIO_TogglePin(LED_GREEN_PORT, LED_GREEN_PIN);
        HAL_Delay(500U);
    }
}

/* ============================================================
 *  패턴 유틸리티
 * ============================================================ */

/**
 * @brief  버퍼에 테스트 패턴 채우기
 *         buf[i] = (i + seed) & 0xFF
 */
static void FillPattern(uint8_t *buf, uint32_t len, uint8_t seed)
{
    for (uint32_t i = 0U; i < len; i++)
        buf[i] = (uint8_t)((i + (uint32_t)seed) & 0xFFU);
}

/**
 * @brief  버퍼 내용이 패턴과 일치하는지 검증
 * @retval 불일치 바이트 수 (0 = 전부 일치)
 */
static int VerifyPattern(const uint8_t *buf, uint32_t len, uint8_t seed)
{
    int errors = 0;
    for (uint32_t i = 0U; i < len; i++)
    {
        uint8_t expected = (uint8_t)((i + (uint32_t)seed) & 0xFFU);
        if (buf[i] != expected)
            errors++;
    }
    return errors;
}

/* ============================================================
 *  시스템 클럭 설정
 *  HSE(8 MHz) → PLL1 → SYSCLK 250 MHz
 *  PLL1Q = 100 MHz → SDMMC1 커널 클럭
 * ============================================================ */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* VOS0 설정 (250 MHz 지원) */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* HSE + PLL1 설정 */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_BYPASS;          /* NUCLEO: HSE bypass */
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 1U;
    osc.PLL.PLLN       = 125U;
    osc.PLL.PLLP       = 4U;    /* 8/1*125/4 = 250 MHz → SYSCLK */
    osc.PLL.PLLQ       = 10U;   /* 8/1*125/10 = 100 MHz → SDMMC1 */
    osc.PLL.PLLR       = 2U;
    osc.PLL.PLLRGE     = RCC_PLL1_VCIRANGE_1;
    osc.PLL.PLLVCOSEL  = RCC_PLL1_VCORANGE_WIDE;
    osc.PLL.PLLFRACN   = 0U;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    /* AHB/APB 분주 설정 */
    clk.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                       | RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 250 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 = 125 MHz (USART3) */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    clk.APB3CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();

    /* SDMMC1 커널 클럭 소스 → PLL1Q (100 MHz) */
    RCC_PeriphCLKInitTypeDef periph = {0};
    periph.PeriphClockSelection = RCC_PERIPHCLK_SDMMC1;
    periph.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_PLL1Q;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK)
        Error_Handler();
}

/* ============================================================
 *  GPIO 초기화 – LED (NUCLEO-H563ZI)
 * ============================================================ */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* 초기 상태: 모두 끔 */
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

/* ============================================================
 *  오류 핸들러
 * ============================================================ */
void Error_Handler(void)
{
    __disable_irq();
    HAL_GPIO_WritePin(LED_GREEN_PORT,  LED_GREEN_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT,    LED_RED_PIN,    GPIO_PIN_SET);  /* 빨간 LED */
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
    Error_Handler();
}
#endif
