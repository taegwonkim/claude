/**
 * @file    main.c
 * @brief   STM32H5 SD Card + GPDMA1/2 메인 파일
 *
 * 동작 요약
 * ---------
 * 1. 시스템 클럭 250MHz 설정 (HSE 24MHz → PLL1)
 * 2. GPDMA1 Ch0(SDMMC1_RX) / Ch1(SDMMC1_TX) 초기화
 * 3. GPDMA2 Ch0(USART1_TX) 초기화
 * 4. SDMMC1 4-bit 버스 / FatFS 마운트
 * 5. 테스트: 파일 쓰기(DMA) → 파일 읽기(DMA) → 결과 UART 출력
 *
 * 인터럽트 우선순위
 * -----------------
 * GPDMA1(SD)  : 5   → UART DMA(6)보다 높아 SD 전송 중 UART 지연 없음
 * SDMMC1      : 5
 * GPDMA2(UART): 6
 * USART1      : 6
 */

#include "main.h"
#include "fatfs.h"
#include "sd_card.h"
#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * 전역 핸들
 * ----------------------------------------------------------------------- */
SD_HandleTypeDef   hsd1;
DMA_HandleTypeDef  hdma_sdmmc1_rx;
DMA_HandleTypeDef  hdma_sdmmc1_tx;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_tx;

/* -----------------------------------------------------------------------
 * 테스트 버퍼 — 4바이트 정렬 필수 (GPDMA word 전송)
 * ----------------------------------------------------------------------- */
static __attribute__((aligned(4))) uint8_t s_WriteBuf[512];
static __attribute__((aligned(4))) uint8_t s_ReadBuf[512];

/* -----------------------------------------------------------------------
 * 내부 함수 프로토타입
 * ----------------------------------------------------------------------- */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_GPDMA1_Init(void);
static void MX_GPDMA2_Init(void);
static void MX_SDMMC1_SD_Init(void);
static void MX_USART1_UART_Init(void);
static void FatFS_Demo(void);

/* -----------------------------------------------------------------------
 * printf 리다이렉트 (USART1 DMA)
 * ----------------------------------------------------------------------- */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* 주변장치 초기화 순서:
     * GPIO → GPDMA → SDMMC → UART → FatFS */
    MX_GPIO_Init();
    MX_GPDMA1_Init();
    MX_GPDMA2_Init();
    MX_USART1_UART_Init();
    MX_SDMMC1_SD_Init();
    MX_FATFS_Init();

    printf("\r\n=== STM32H5 SD GPDMA Demo ===\r\n");
    printf("SYSCLK : %lu MHz\r\n", HAL_RCC_GetSysClockFreq() / 1000000UL);

    /* SD 카드 감지 확인 */
    if (!SD_IsPresent()) {
        printf("[ERROR] SD card not detected!\r\n");
        Error_Handler();
    }

    /* FatFS 데모 실행 */
    FatFS_Demo();

    /* 메인 루프 */
    while (1) {
        HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
        HAL_Delay(500);
    }
}

/* -----------------------------------------------------------------------
 * FatFS 읽기/쓰기 데모
 * ----------------------------------------------------------------------- */
static void FatFS_Demo(void)
{
    FATFS   fs;
    FIL     fil;
    FRESULT fres;
    UINT    bw, br;
    const char *testFile = "0:test_dma.txt";

    /* FatFS 마운트 */
    fres = f_mount(&fs, "0:", 1);
    if (fres != FR_OK) {
        printf("[ERROR] f_mount failed: %d\r\n", (int)fres);
        /* 포맷 후 재시도 */
        BYTE workBuf[FF_MAX_SS];
        printf("[INFO] Formatting SD card...\r\n");
        fres = f_mkfs("0:", 0, workBuf, sizeof(workBuf));
        if (fres != FR_OK) {
            printf("[ERROR] f_mkfs failed: %d\r\n", (int)fres);
            Error_Handler();
        }
        fres = f_mount(&fs, "0:", 1);
        if (fres != FR_OK) {
            printf("[ERROR] f_mount (2nd) failed: %d\r\n", (int)fres);
            Error_Handler();
        }
    }
    printf("[OK] FatFS mounted\r\n");

    /* --- 파일 쓰기 --- */
    const char *writeData =
        "STM32H5 GPDMA SD Card Test\r\n"
        "GPDMA1 Ch0: SDMMC1_RX\r\n"
        "GPDMA1 Ch1: SDMMC1_TX\r\n"
        "Timestamp: 2025\r\n";

    fres = f_open(&fil, testFile, FA_WRITE | FA_CREATE_ALWAYS);
    if (fres != FR_OK) {
        printf("[ERROR] f_open (write) failed: %d\r\n", (int)fres);
        Error_Handler();
    }

    fres = f_write(&fil, writeData, strlen(writeData), &bw);
    f_close(&fil);

    if (fres != FR_OK || bw != strlen(writeData)) {
        printf("[ERROR] f_write failed: %d (written=%u)\r\n", (int)fres, bw);
        Error_Handler();
    }
    printf("[OK] Written %u bytes to %s\r\n", bw, testFile);

    /* --- 파일 읽기 --- */
    memset(s_ReadBuf, 0, sizeof(s_ReadBuf));

    fres = f_open(&fil, testFile, FA_READ);
    if (fres != FR_OK) {
        printf("[ERROR] f_open (read) failed: %d\r\n", (int)fres);
        Error_Handler();
    }

    fres = f_read(&fil, s_ReadBuf, sizeof(s_ReadBuf) - 1, &br);
    f_close(&fil);

    if (fres != FR_OK) {
        printf("[ERROR] f_read failed: %d\r\n", (int)fres);
        Error_Handler();
    }

    printf("[OK] Read %u bytes:\r\n%s\r\n", br, (char *)s_ReadBuf);

    /* 무결성 검증 */
    if (memcmp(s_ReadBuf, writeData, strlen(writeData)) == 0) {
        printf("[PASS] Data integrity check passed!\r\n");
    } else {
        printf("[FAIL] Data mismatch!\r\n");
    }

    /* --- 로우 레벨 DMA 직접 테스트 (블록 0 스킵, 블록 100 사용) --- */
    printf("\r\n--- Raw DMA Block Test ---\r\n");
    for (uint32_t i = 0; i < 512; i++) {
        s_WriteBuf[i] = (uint8_t)(i & 0xFF);
    }

    SD_StatusTypeDef sdSt = SD_WriteBlocks_DMA(s_WriteBuf, 100, 1);
    if (sdSt != SD_OK) {
        printf("[ERROR] Raw DMA write failed: %d\r\n", (int)sdSt);
    } else {
        memset(s_ReadBuf, 0, 512);
        sdSt = SD_ReadBlocks_DMA(s_ReadBuf, 100, 1);
        if (sdSt != SD_OK) {
            printf("[ERROR] Raw DMA read failed: %d\r\n", (int)sdSt);
        } else {
            bool pass = (memcmp(s_WriteBuf, s_ReadBuf, 512) == 0);
            printf("[%s] Raw 512-byte DMA block test\r\n", pass ? "PASS" : "FAIL");
        }
    }

    f_mount(NULL, "0:", 0);  /* 언마운트 */
}

/* -----------------------------------------------------------------------
 * 시스템 클럭 설정
 * HSE 24MHz → PLL1: SYSCLK 250MHz, PLL1Q 125MHz (SDMMC)
 * ----------------------------------------------------------------------- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       RCC_OscInit   = {0};
    RCC_ClkInitTypeDef       RCC_ClkInit   = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /* 전압 스케일링 (250MHz → Scale 0) */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInit.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInit.HSEState       = RCC_HSE_ON;
    RCC_OscInit.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInit.PLL.PLLSource  = RCC_PLL1_SOURCE_HSE;
    /* HSE=24MHz, DIVM1=4 → VCO_in=6MHz
     * MULN1=125 → VCO_out=750MHz
     * DIVP1=3   → PLL1P=250MHz (SYSCLK)
     * DIVQ1=6   → PLL1Q=125MHz (SDMMC)
     * DIVR1=2   → PLL1R=375MHz */
    RCC_OscInit.PLL.PLLM       = 4;
    RCC_OscInit.PLL.PLLN       = 125;
    RCC_OscInit.PLL.PLLP       = 3;
    RCC_OscInit.PLL.PLLQ       = 6;
    RCC_OscInit.PLL.PLLR       = 2;
    RCC_OscInit.PLL.PLLRGE     = RCC_PLL1_VCIRANGE_2;   /* 4~8MHz 입력 */
    RCC_OscInit.PLL.PLLVCOSEL  = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInit.PLL.PLLFRACN   = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInit) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInit.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                 RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInit.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInit.AHBCLKDivider  = RCC_SYSCLK_DIV1;    /* HCLK = 250MHz */
    RCC_ClkInit.APB1CLKDivider = RCC_HCLK_DIV2;      /* APB1 = 125MHz */
    RCC_ClkInit.APB2CLKDivider = RCC_HCLK_DIV2;      /* APB2 = 125MHz */
    RCC_ClkInit.APB3CLKDivider = RCC_HCLK_DIV2;      /* APB3 = 125MHz */
    /* Flash 레이턴시: 250MHz @ VOS0 → 5 wait states */
    if (HAL_RCC_ClockConfig(&RCC_ClkInit, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /* SDMMC1 클럭 소스: PLL1Q (125MHz) */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SDMMC1;
    PeriphClkInit.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_PLL1Q;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        Error_Handler();
    }
}

/* -----------------------------------------------------------------------
 * GPIO 초기화
 * ----------------------------------------------------------------------- */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 클럭 인가 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* LED (LD1) */
    HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin   = LD1_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD1_GPIO_Port, &GPIO_InitStruct);

    /* SD 카드 감지 핀 (Active Low, Pull-Up) */
    GPIO_InitStruct.Pin  = SD_DETECT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SD_DETECT_GPIO_Port, &GPIO_InitStruct);
}

/* -----------------------------------------------------------------------
 * GPDMA1 초기화 (SDMMC1 RX / TX)
 * STM32H5 GPDMA: HAL_DMA_Init + 링크드리스트 노드 방식
 * ----------------------------------------------------------------------- */
static void MX_GPDMA1_Init(void)
{
    __HAL_RCC_GPDMA1_CLK_ENABLE();

    /* ---- Ch0: SDMMC1_RX (Periph → Memory) ---- */
    hdma_sdmmc1_rx.Instance                       = GPDMA1_Channel0;
    hdma_sdmmc1_rx.Init.Request                   = GPDMA1_REQUEST_SDMMC1;
    hdma_sdmmc1_rx.Init.BlkHWRequest              = DMA_BREQ_SINGLE_BURST;
    hdma_sdmmc1_rx.Init.Direction                 = DMA_PERIPH_TO_MEMORY;
    hdma_sdmmc1_rx.Init.SrcInc                    = DMA_SINC_FIXED;
    hdma_sdmmc1_rx.Init.DestInc                   = DMA_DINC_INCREMENTED;
    hdma_sdmmc1_rx.Init.SrcDataWidth              = DMA_SRC_DATAWIDTH_WORD;
    hdma_sdmmc1_rx.Init.DestDataWidth             = DMA_DEST_DATAWIDTH_WORD;
    hdma_sdmmc1_rx.Init.Priority                  = DMA_HIGH_PRIORITY;
    hdma_sdmmc1_rx.Init.SrcBurstLength            = 4;
    hdma_sdmmc1_rx.Init.DestBurstLength           = 4;
    hdma_sdmmc1_rx.Init.TransferAllocatedPort     = DMA_SRC_ALLOCATED_PORT0 |
                                                    DMA_DEST_ALLOCATED_PORT1;
    hdma_sdmmc1_rx.Init.TransferEventMode         = DMA_TCEM_BLOCK_TRANSFER;
    hdma_sdmmc1_rx.Init.Mode                      = DMA_NORMAL;
    if (HAL_DMA_Init(&hdma_sdmmc1_rx) != HAL_OK) {
        Error_Handler();
    }

    /* SDMMC1과 DMA 핸들 연결 */
    __HAL_LINKDMA(&hsd1, hdmarx, hdma_sdmmc1_rx);

    /* NVIC */
    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);

    /* ---- Ch1: SDMMC1_TX (Memory → Periph) ---- */
    hdma_sdmmc1_tx.Instance                       = GPDMA1_Channel1;
    hdma_sdmmc1_tx.Init.Request                   = GPDMA1_REQUEST_SDMMC1;
    hdma_sdmmc1_tx.Init.BlkHWRequest              = DMA_BREQ_SINGLE_BURST;
    hdma_sdmmc1_tx.Init.Direction                 = DMA_MEMORY_TO_PERIPH;
    hdma_sdmmc1_tx.Init.SrcInc                    = DMA_SINC_INCREMENTED;
    hdma_sdmmc1_tx.Init.DestInc                   = DMA_DINC_FIXED;
    hdma_sdmmc1_tx.Init.SrcDataWidth              = DMA_SRC_DATAWIDTH_WORD;
    hdma_sdmmc1_tx.Init.DestDataWidth             = DMA_DEST_DATAWIDTH_WORD;
    hdma_sdmmc1_tx.Init.Priority                  = DMA_HIGH_PRIORITY;
    hdma_sdmmc1_tx.Init.SrcBurstLength            = 4;
    hdma_sdmmc1_tx.Init.DestBurstLength           = 4;
    hdma_sdmmc1_tx.Init.TransferAllocatedPort     = DMA_SRC_ALLOCATED_PORT1 |
                                                    DMA_DEST_ALLOCATED_PORT0;
    hdma_sdmmc1_tx.Init.TransferEventMode         = DMA_TCEM_BLOCK_TRANSFER;
    hdma_sdmmc1_tx.Init.Mode                      = DMA_NORMAL;
    if (HAL_DMA_Init(&hdma_sdmmc1_tx) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(&hsd1, hdmatx, hdma_sdmmc1_tx);

    HAL_NVIC_SetPriority(GPDMA1_Channel1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);
}

/* -----------------------------------------------------------------------
 * GPDMA2 초기화 (USART1 TX)
 * UART DMA는 우선순위를 SDMMC DMA보다 낮게 설정
 * ----------------------------------------------------------------------- */
static void MX_GPDMA2_Init(void)
{
    __HAL_RCC_GPDMA2_CLK_ENABLE();

    hdma_usart1_tx.Instance                   = GPDMA2_Channel0;
    hdma_usart1_tx.Init.Request               = GPDMA2_REQUEST_USART1_TX;
    hdma_usart1_tx.Init.BlkHWRequest          = DMA_BREQ_SINGLE_BURST;
    hdma_usart1_tx.Init.Direction             = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.SrcInc               = DMA_SINC_INCREMENTED;
    hdma_usart1_tx.Init.DestInc              = DMA_DINC_FIXED;
    hdma_usart1_tx.Init.SrcDataWidth         = DMA_SRC_DATAWIDTH_BYTE;
    hdma_usart1_tx.Init.DestDataWidth        = DMA_DEST_DATAWIDTH_BYTE;
    hdma_usart1_tx.Init.Priority             = DMA_LOW_PRIORITY | DMA_LOW_PRIORITY;
    hdma_usart1_tx.Init.SrcBurstLength       = 1;
    hdma_usart1_tx.Init.DestBurstLength      = 1;
    hdma_usart1_tx.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 |
                                               DMA_DEST_ALLOCATED_PORT0;
    hdma_usart1_tx.Init.TransferEventMode    = DMA_TCEM_BLOCK_TRANSFER;
    hdma_usart1_tx.Init.Mode                 = DMA_NORMAL;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

    HAL_NVIC_SetPriority(GPDMA2_Channel0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(GPDMA2_Channel0_IRQn);
}

/* -----------------------------------------------------------------------
 * SDMMC1 초기화 (4-bit 버스, DMA 모드)
 * ----------------------------------------------------------------------- */
static void MX_SDMMC1_SD_Init(void)
{
    hsd1.Instance                 = SDMMC1;
    hsd1.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide             = SDMMC_BUS_WIDE_4B;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    /* SDMMCclk(125MHz) / (ClkDiv+2) → ClkDiv=2 → ~31.25MHz */
    hsd1.Init.ClockDiv            = 2;

    if (HAL_SD_Init(&hsd1) != HAL_OK) {
        printf("[ERROR] SDMMC1 init failed!\r\n");
        Error_Handler();
    }

    /* 4-bit 버스 폭 전환 */
    if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) != HAL_OK) {
        printf("[ERROR] 4-bit bus config failed!\r\n");
        Error_Handler();
    }

    /* SDMMC1 전역 인터럽트 */
    HAL_NVIC_SetPriority(SDMMC1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(SDMMC1_IRQn);
}

/* -----------------------------------------------------------------------
 * USART1 초기화
 * ----------------------------------------------------------------------- */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* -----------------------------------------------------------------------
 * 에러 핸들러
 * ----------------------------------------------------------------------- */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
        HAL_Delay(100);
    }
}
