/**
 ******************************************************************************
 * @file    main.c
 * @brief   STM32H5 SD 카드 + UART GPDMA 동시 동작 예제
 *
 *  ┌─────────────────────────────────────────────────────────────────────┐
 *  │  DMA 구성 – 충돌 없는 이중 DMA 운용                                │
 *  │                                                                     │
 *  │  ① SDMMC1 IDMA  ←→  SRAM2[sd_wr/rd_buf]                          │
 *  │     • SDMMC1 전용 내장 DMA 엔진                                    │
 *  │     • AHB 마스터로 SRAM2 직접 접근                                 │
 *  │     • NVIC Preempt = 4                                             │
 *  │                                                                     │
 *  │  ② GPDMA1 CH0   →   USART3 DR ← SRAM2[uart_dma_buf]             │
 *  │     • GPDMA1 독립 채널 (SDMMC IDMA와 별개 엔진)                   │
 *  │     • NVIC Preempt = 6                                             │
 *  │                                                                     │
 *  │  MPU Region 0: SRAM2(0x20030000, 8KB) = Non-Cacheable             │
 *  │  → 모든 DMA 버퍼가 이 영역에 위치 → SCB 호출 없이 정합 보장       │
 *  └─────────────────────────────────────────────────────────────────────┘
 *
 *  실행 시나리오:
 *    Phase 1: SD 카드 초기화, 카드 정보 출력 (UART GPDMA)
 *    Phase 2: SD 쓰기 – IDMA 전송 중 UART 로그 동시 출력
 *    Phase 3: SD 읽기 – 데이터 검증
 *    Phase 4: High-Speed 모드 전환 후 쓰기/읽기 재수행
 *    Phase 5: 성공 LED 점멸
 ******************************************************************************
 */
#include "main.h"
#include "mpu_config.h"
#include "sd_driver.h"
#include "uart_gpdma.h"
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  DMA 버퍼 – .dma_buf 섹션 (SRAM2 Non-Cacheable 영역)
 *
 *  SD 버퍼와 UART 버퍼는 완전히 다른 주소에 배치됨.
 *  링커 스크립트가 SRAM2(>RAM2)에 순서대로 배치.
 * ═══════════════════════════════════════════════════════════════════════════ */
static uint8_t sd_wr_buf[SD_TEST_COUNT * SD_SECTOR_SIZE]
    __attribute__((aligned(32), section(".dma_buf")));

static uint8_t sd_rd_buf[SD_TEST_COUNT * SD_SECTOR_SIZE]
    __attribute__((aligned(32), section(".dma_buf")));

/* ── 함수 원형 ──────────────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void LED_Set(uint8_t green, uint8_t yellow, uint8_t red);
static void RunSDTest(const char *phase_label, uint32_t sector, uint8_t seed);

/* ════════════════════════════════════════════════════════════════════════════
 *  main()
 * ════════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    /* ── Phase 0: 시스템 초기화 ─────────────────────────────────────────── */

    /* MPU 먼저 설정 – 이후 HAL_Init에서 SysTick, D-Cache 활성화 전 완료 */
    MPU_ConfigDMARegion();

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    /* UART GPDMA 초기화 – 이후 로그 즉시 출력 가능 */
    UART_GPDMA_Init();

    UART_Printf("\r\n");
    UART_Printf("╔══════════════════════════════════════════════════════╗\r\n");
    UART_Printf("║  STM32H563ZI  SD Card + UART GPDMA Coexistence     ║\r\n");
    UART_Printf("║  Board  : NUCLEO-H563ZI   HCLK : 250 MHz           ║\r\n");
    UART_Printf("║  SD DMA : SDMMC1 IDMA     UART : GPDMA1 CH0        ║\r\n");
    UART_Printf("║  Buffer : SRAM2 MPU Non-Cacheable (no SCB calls)   ║\r\n");
    UART_Printf("╚══════════════════════════════════════════════════════╝\r\n\r\n");

    /* ── Phase 1: SD 카드 초기화 (DS 모드, 12.5 MHz) ─────────────────── */
    UART_Printf("[INIT] SD 카드 초기화 (DS 12.5 MHz)... ");

    SD_Status st = SD_Init();
    if (st != SD_OK)
    {
        UART_Printf("FAIL (code=%d)\r\n", (int)st);
        UART_Printf("  확인: 카드 삽입 여부, 핀 배선 (PC8-12, PD2)\r\n");
        Error_Handler();
    }
    UART_Printf("OK\r\n");

    /* 카드 정보 출력 */
    SD_CardInfo ci;
    if (SD_GetInfo(&ci) == SD_OK)
    {
        UART_Printf("[INFO] 블록수=%lu  블록크기=%luB  용량=%llu MB\r\n",
                    (unsigned long)ci.block_count,
                    (unsigned long)ci.block_size,
                    (unsigned long long)ci.capacity_mb);
    }

    /* ── Phase 2: DS 모드 읽기/쓰기 테스트 ──────────────────────────── */
    UART_Printf("\r\n[TEST] DS 모드 (12.5 MHz)\r\n");
    RunSDTest("DS-Write/Read", SD_TEST_SECTOR, 0xA5U);

    /* ── Phase 3: High-Speed 모드로 전환 (25 MHz) ────────────────────── */
    UART_Printf("\r\n[HS] High-Speed 모드 전환 (25 MHz)... ");

    st = SD_SetHighSpeed();
    if (st != SD_OK)
    {
        UART_Printf("FAIL (code=%d) – DS 모드 유지\r\n", (int)st);
    }
    else
    {
        UART_Printf("OK\r\n");
        UART_Printf("\r\n[TEST] HS 모드 (25 MHz)\r\n");
        RunSDTest("HS-Write/Read", SD_TEST_SECTOR + 10U, 0x5AU);
    }

    /* ── Phase 4: 완료 ──────────────────────────────────────────────── */
    UART_Printf("\r\n╔══════════════════════════════════════════════════════╗\r\n");
    UART_Printf("║  모든 테스트 통과 – 녹색 LED 점멸                   ║\r\n");
    UART_Printf("╚══════════════════════════════════════════════════════╝\r\n");

    UART_Flush();   /* UART 전송 완료 대기 후 루프 진입 */

    /* ── Phase 5: LED 점멸 루프 ─────────────────────────────────────── */
    while (1)
    {
        LED_Set(1, 0, 0);
        HAL_Delay(400U);
        LED_Set(0, 0, 0);
        HAL_Delay(100U);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  SD 읽기/쓰기/검증 공통 루틴
 * ════════════════════════════════════════════════════════════════════════════ */

/** sd_wr_buf를 (index+seed)&0xFF 패턴으로 채움 */
static void FillPattern(uint8_t seed)
{
    uint32_t total = SD_TEST_COUNT * SD_SECTOR_SIZE;
    for (uint32_t i = 0U; i < total; i++)
        sd_wr_buf[i] = (uint8_t)((i + seed) & 0xFFU);
}

/** sd_rd_buf와 패턴이 일치하는지 확인 → 불일치 바이트 수 반환 */
static uint32_t VerifyPattern(uint8_t seed)
{
    uint32_t total    = SD_TEST_COUNT * SD_SECTOR_SIZE;
    uint32_t bad      = 0U;
    uint32_t first_ba = UINT32_MAX;

    for (uint32_t i = 0U; i < total; i++)
    {
        uint8_t expected = (uint8_t)((i + seed) & 0xFFU);
        if (sd_rd_buf[i] != expected)
        {
            bad++;
            if (first_ba == UINT32_MAX)
                first_ba = i;
        }
    }

    if (bad > 0U)
    {
        uint8_t exp = (uint8_t)((first_ba + seed) & 0xFFU);
        UART_Printf("  첫 불일치 offset=%lu  got=0x%02X  exp=0x%02X\r\n",
                    (unsigned long)first_ba,
                    sd_rd_buf[first_ba], exp);
    }
    return bad;
}

/**
 * @brief  쓰기 → 읽기 → 검증 시퀀스 (UART 로그와 SD 전송 동시 수행 시연)
 *
 * @param  phase_label  로그 레이블
 * @param  sector       시작 LBA 섹터
 * @param  seed         패턴 시드
 */
static void RunSDTest(const char *phase_label, uint32_t sector, uint8_t seed)
{
    uint32_t t_start, t_end;
    SD_Status st;

    /* ── 쓰기 ─────────────────────────────────────────────────────────── */
    FillPattern(seed);

    UART_Printf("  [W] sector=%lu count=%u ... ",
                (unsigned long)sector, SD_TEST_COUNT);

    /*
     * ★ 핵심 시연: SD IDMA 전송 시작 직전 UART_Printf 큐에 메시지 적재.
     *   SD_WriteBlocks 내부에서 HAL_SD_WriteBlocks_DMA 호출 → IDMA 시작.
     *   이 동안 GPDMA1 CH0은 UART_Printf로 적재된 "쓰기 중..." 문자열을 전송.
     *   두 전송이 실제로 겹쳐 동작함을 확인 가능.
     */
    t_start = HAL_GetTick();
    st = SD_WriteBlocks(sd_wr_buf, sector, SD_TEST_COUNT);
    t_end   = HAL_GetTick();

    if (st != SD_OK)
    {
        UART_Printf("FAIL (code=%d)\r\n", (int)st);
        Error_Handler();
    }
    UART_Printf("OK (%lu ms)\r\n", (unsigned long)(t_end - t_start));

    /* ── 읽기 ─────────────────────────────────────────────────────────── */
    memset(sd_rd_buf, 0x00U, sizeof(sd_rd_buf));

    UART_Printf("  [R] sector=%lu count=%u ... ",
                (unsigned long)sector, SD_TEST_COUNT);

    t_start = HAL_GetTick();
    st = SD_ReadBlocks(sd_rd_buf, sector, SD_TEST_COUNT);
    t_end   = HAL_GetTick();

    if (st != SD_OK)
    {
        UART_Printf("FAIL (code=%d)\r\n", (int)st);
        Error_Handler();
    }
    UART_Printf("OK (%lu ms)\r\n", (unsigned long)(t_end - t_start));

    /* ── 검증 ─────────────────────────────────────────────────────────── */
    UART_Printf("  [V] 데이터 검증 ... ");
    uint32_t bad = VerifyPattern(seed);
    if (bad > 0U)
    {
        UART_Printf("FAIL (%lu 바이트 불일치)\r\n", (unsigned long)bad);
        Error_Handler();
    }
    UART_Printf("OK  %s 통과\r\n", phase_label);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  시스템 클럭 설정
 *  HSE(8 MHz) → PLL1 → SYSCLK 250 MHz / PLL1Q 100 MHz (SDMMC1 커널)
 * ════════════════════════════════════════════════════════════════════════════ */
static void SystemClock_Config(void)
{
    /* VOS0: 250 MHz 지원 */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_BYPASS;      /* NUCLEO: 8 MHz bypass */
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 1U;
    osc.PLL.PLLN       = 125U;
    osc.PLL.PLLP       = 4U;    /* 8/1×125/4  = 250 MHz → SYSCLK */
    osc.PLL.PLLQ       = 10U;   /* 8/1×125/10 = 100 MHz → SDMMC1 */
    osc.PLL.PLLR       = 2U;
    osc.PLL.PLLRGE     = RCC_PLL1_VCIRANGE_1;
    osc.PLL.PLLVCOSEL  = RCC_PLL1_VCORANGE_WIDE;
    osc.PLL.PLLFRACN   = 0U;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                       | RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;    /* HCLK  = 250 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;      /* PCLK1 = 125 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    clk.APB3CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();

    /* SDMMC1 커널 클럭 → PLL1Q (100 MHz) */
    RCC_PeriphCLKInitTypeDef periph = {0};
    periph.PeriphClockSelection = RCC_PERIPHCLK_SDMMC1;
    periph.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_PLL1Q;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK)
        Error_Handler();
}

/* ════════════════════════════════════════════════════════════════════════════
 *  GPIO – LED
 * ════════════════════════════════════════════════════════════════════════════ */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = LED_GREEN_PIN;   HAL_GPIO_Init(LED_GREEN_PORT,  &gpio);
    gpio.Pin = LED_YELLOW_PIN;  HAL_GPIO_Init(LED_YELLOW_PORT, &gpio);
    gpio.Pin = LED_RED_PIN;     HAL_GPIO_Init(LED_RED_PORT,    &gpio);

    /* 초기: 전부 끔 */
    HAL_GPIO_WritePin(LED_GREEN_PORT,  LED_GREEN_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT,    LED_RED_PIN,    GPIO_PIN_RESET);
}

static void LED_Set(uint8_t green, uint8_t yellow, uint8_t red)
{
    HAL_GPIO_WritePin(LED_GREEN_PORT,  LED_GREEN_PIN,
                      green  ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN,
                      yellow ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT,    LED_RED_PIN,
                      red    ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  오류 핸들러
 * ════════════════════════════════════════════════════════════════════════════ */
void Error_Handler(void)
{
    __disable_irq();
    LED_Set(0, 0, 1);   /* 빨간 LED 점등 */
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
    Error_Handler();
}
#endif
