/**
 ******************************************************************************
 * @file    main.c
 * @brief   STM32H5 RAM 구성 통합 예제 - 메인 파일
 *
 * 이 파일은 STM32H563 Nucleo 보드를 기준으로 작성된 통합 예제입니다.
 * CubeMX로 생성된 코드 구조를 따르며, 다음 기능을 시연합니다:
 *
 * [초기화 순서]
 * 1. HAL 초기화
 * 2. 시스템 클럭 설정 (250MHz)
 * 3. MPU 설정 (SRAM2 Non-Cacheable)
 * 4. 캐시 활성화 (I-Cache, D-Cache)
 * 5. SRAM2 섹션 초기화
 * 6. BKPRAM 초기화 및 데이터 복원
 * 7. 주변장치 초기화 (UART, DMA)
 * 8. 메인 루프
 *
 * [하드웨어 요구사항]
 * - STM32H563ZI Nucleo-144 보드 (또는 호환 보드)
 * - VBAT에 CR2032 코인 배터리 (BKPRAM 테스트용, 선택)
 * - UART3: ST-Link VCP (115200 baud, printf 리다이렉션)
 ******************************************************************************
 */

#include "main.h"
#include "ram_config.h"
#include "bkpram_example.h"
#include "dma_sram2_example.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 * 전역 핸들 (CubeMX 생성)
 * ============================================================ */
UART_HandleTypeDef huart3;   /* ST-Link VCP */
DMA_HandleTypeDef  hdma_usart3_rx;
DMA_HandleTypeDef  hdma_usart3_tx;

/* ============================================================
 * 다양한 RAM 영역에 배치된 전역 변수 예시
 * ============================================================ */

/* SRAM1 (기본) - 일반 전역 변수 */
static uint32_t sram1Counter = 0;
static uint8_t  sram1WorkBuf[512];

/* SRAM1 명시적 배치 */
PLACE_IN_SRAM1
static uint32_t sram1ExplicitData[64] = {
    /* 초기값 있음: FLASH에 저장 후 부팅 시 SRAM1으로 복사 */
    0x11111111, 0x22222222, 0x33333333, 0x44444444,
};

/* SRAM2 배치 - 초기값 있음 */
PLACE_IN_SRAM2
static uint32_t sram2ConfigData[8] = {
    /* 초기값: FLASH에 저장, SRAM2_Init()에서 복사 */
    0xDEAD0001, 0xDEAD0002, 0xDEAD0003, 0xDEAD0004,
};

/* SRAM2 BSS - 초기화 없음 (DMA 버퍼는 dma_sram2_example.c에 정의) */
PLACE_IN_SRAM2_BSS CACHE_ALIGNED
static uint8_t sram2WorkBuf[256];

/* BKPRAM 배치 - 소프트 리셋 횟수 (전원 차단 후에도 유지) */
PLACE_IN_BKPRAM
static uint32_t softResetCounter;

/* ============================================================
 * 내부 함수 프로토타입
 * ============================================================ */
static void SystemClock_Config(void);
static void MX_UART3_Init(void);
static void MX_DMA_Init(void);
static void PrintBanner(void);
static void Demo_RAM_Regions(void);
static void Demo_BKPRAM(void);
static void Demo_SRAM2_DMA(void);

/* ============================================================
 * printf 리다이렉션 (UART3 → ST-Link VCP)
 * ============================================================ */
#ifdef __GNUC__
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, 100);
    return ch;
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart3, (uint8_t *)ptr, len, 1000);
    return len;
}
#endif

/* ============================================================
 * 메인 함수
 * ============================================================ */

/**
 * @brief  메인 함수
 * @retval int
 */
int main(void)
{
    /* ---------------------------------------------------
     * 1. HAL 초기화 (SysTick, NVIC 기본 설정)
     * --------------------------------------------------- */
    HAL_Init();

    /* ---------------------------------------------------
     * 2. 시스템 클럭 설정 (250MHz)
     * --------------------------------------------------- */
    SystemClock_Config();

    /* ---------------------------------------------------
     * 3. MPU 설정 (SRAM2 Non-Cacheable)
     *    반드시 캐시 활성화 이전에 설정
     * --------------------------------------------------- */
    MPU_ConfigSRAM2NonCacheable();

    /* ---------------------------------------------------
     * 4. 캐시 활성화
     *    STM32H5는 I-Cache, D-Cache 모두 지원
     * --------------------------------------------------- */
    SCB_EnableICache();  /* Instruction Cache ON */
    SCB_EnableDCache();  /* Data Cache ON */

    /* ---------------------------------------------------
     * 5. SRAM2 섹션 초기화
     *    startup 파일에서 처리 안 됨 → 명시적 호출 필요
     * --------------------------------------------------- */
    SRAM2_Init();

    /* ---------------------------------------------------
     * 6. UART 초기화 (printf 사용 전에 필요)
     * --------------------------------------------------- */
    MX_UART3_Init();

    /* ---------------------------------------------------
     * 7. 시작 배너 출력
     * --------------------------------------------------- */
    PrintBanner();

    /* ---------------------------------------------------
     * 8. BKPRAM 초기화 및 데이터 복원
     * --------------------------------------------------- */
    BKPRAM_Example_Init();

    /* ---------------------------------------------------
     * 9. DMA 초기화
     * --------------------------------------------------- */
    MX_DMA_Init();
    DMA_MemToMem_Init();

    /* ---------------------------------------------------
     * 10. RAM 정보 출력
     * --------------------------------------------------- */
    RAM_PrintInfo();

    /* ---------------------------------------------------
     * 11. 데모 실행
     * --------------------------------------------------- */
    Demo_RAM_Regions();
    Demo_BKPRAM();
    Demo_SRAM2_DMA();

    printf("\r\n[MAIN] 초기화 완료. 메인 루프 진입...\r\n");

    /* ---------------------------------------------------
     * 메인 루프
     * --------------------------------------------------- */
    while (1)
    {
        HAL_Delay(1000);
        sram1Counter++;

        /* 10초마다 상태 출력 */
        if (sram1Counter % 10 == 0)
        {
            printf("[LOOP] 실행 시간: %lu초, BKPRAM 앱모드: %lu\r\n",
                   sram1Counter,
                   BKPRAM_Example_GetAppMode());
        }

        /* LED 토글 (Nucleo-H563ZI: LD1 = PB0) */
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    }
}

/* ============================================================
 * 데모 함수
 * ============================================================ */

/**
 * @brief RAM 영역별 주소 및 배치 데모
 */
static void Demo_RAM_Regions(void)
{
    printf("\r\n===== RAM 영역별 변수 배치 확인 =====\r\n");

    printf("SRAM1 영역 (0x20000000 ~ 0x2003FFFF):\r\n");
    printf("  sram1Counter:      0x%08lX (SRAM1: %s)\r\n",
           (uint32_t)&sram1Counter,
           RAM_IsInSRAM1(&sram1Counter) ? "OK" : "ERROR");
    printf("  sram1WorkBuf:      0x%08lX (SRAM1: %s)\r\n",
           (uint32_t)sram1WorkBuf,
           RAM_IsInSRAM1(sram1WorkBuf) ? "OK" : "ERROR");
    printf("  sram1ExplicitData: 0x%08lX (SRAM1: %s)\r\n",
           (uint32_t)sram1ExplicitData,
           RAM_IsInSRAM1(sram1ExplicitData) ? "OK" : "ERROR");
    printf("  초기값 확인[0]:    0x%08lX (예상: 0x11111111)\r\n",
           sram1ExplicitData[0]);

    printf("\r\nSRAM2 영역 (0x20040000 ~ 0x2004FFFF):\r\n");
    printf("  sram2ConfigData:   0x%08lX (SRAM2: %s)\r\n",
           (uint32_t)sram2ConfigData,
           RAM_IsInSRAM2(sram2ConfigData) ? "OK" : "ERROR");
    printf("  초기값 확인[0]:    0x%08lX (예상: 0xDEAD0001)\r\n",
           sram2ConfigData[0]);
    printf("  sram2WorkBuf:      0x%08lX (SRAM2: %s)\r\n",
           (uint32_t)sram2WorkBuf,
           RAM_IsInSRAM2(sram2WorkBuf) ? "OK" : "ERROR");
    printf("  uartRxDmaBuf:      0x%08lX (SRAM2: %s)\r\n",
           (uint32_t)uartRxDmaBuf,
           RAM_IsInSRAM2(uartRxDmaBuf) ? "OK" : "ERROR");

    printf("\r\nBKPRAM 영역 (0x40003C00 ~ 0x40003FFF):\r\n");
    printf("  softResetCounter:  0x%08lX (BKPRAM: %s)\r\n",
           (uint32_t)&softResetCounter,
           RAM_IsInBKPRAM(&softResetCounter) ? "OK" : "ERROR");

    printf("=====================================\r\n");
}

/**
 * @brief BKPRAM 백업/복원 데모
 */
static void Demo_BKPRAM(void)
{
    BKPRAM_Example_PrintStats();

    /* 앱 모드 변경 및 저장 */
    uint32_t currentMode = BKPRAM_Example_GetAppMode();
    BKPRAM_Example_SetAppMode(currentMode + 1);
    printf("[BKPRAM] 앱 모드 변경: %lu → %lu (전원 차단 후에도 유지됨)\r\n",
           currentMode, BKPRAM_Example_GetAppMode());
}

/**
 * @brief SRAM2 DMA 전송 데모
 */
static void Demo_SRAM2_DMA(void)
{
    DMA_SRAM2_Test();
}

/**
 * @brief 시작 배너 출력
 */
static void PrintBanner(void)
{
    printf("\r\n");
    printf("****************************************************\r\n");
    printf("*   STM32H563 RAM 구성 예제                        *\r\n");
    printf("*   SRAM1: 256KB | SRAM2: 64KB | BKPRAM: 4KB      *\r\n");
    printf("*   Core: Cortex-M33 @ 250MHz                      *\r\n");
    printf("****************************************************\r\n");
    printf("\r\n");
}

/* ============================================================
 * 시스템 클럭 설정 (250MHz, HSE 기반)
 * ============================================================ */

/**
 * @brief 시스템 클럭 설정
 *
 * HSE (8MHz, Nucleo 기본) → PLL → SYSCLK 250MHz
 * HCLK  = 250 MHz
 * PCLK1 = 125 MHz (APB1)
 * PCLK2 = 125 MHz (APB2)
 * PCLK3 = 125 MHz (APB3)
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* PWR 클럭 활성화 및 전압 스케일 설정 */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* HSE + PLL 설정 */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_BYPASS;  /* Nucleo: HSE Bypass */
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM            = 4;    /* HSE(8MHz) / 4 = 2MHz */
    RCC_OscInitStruct.PLL.PLLN            = 250;  /* 2MHz × 250 = 500MHz */
    RCC_OscInitStruct.PLL.PLLP            = 2;    /* 500MHz / 2 = 250MHz → SYSCLK */
    RCC_OscInitStruct.PLL.PLLQ            = 2;
    RCC_OscInitStruct.PLL.PLLR            = 2;
    RCC_OscInitStruct.PLL.PLLRGE          = RCC_PLLVCIRANGE_0;
    RCC_OscInitStruct.PLL.PLLFRACN        = 0;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* 시스템 클럭 분주 설정 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK   |
                                       RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1  |
                                       RCC_CLOCKTYPE_PCLK2  |
                                       RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK = 250MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 = 125MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;     /* PCLK2 = 125MHz */
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV2;     /* PCLK3 = 125MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================
 * UART3 초기화 (ST-Link VCP, 115200 baud)
 * ============================================================ */
static void MX_UART3_Init(void)
{
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================
 * DMA 초기화
 * ============================================================ */
static void MX_DMA_Init(void)
{
    __HAL_RCC_GPDMA1_CLK_ENABLE();

    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
}

/* ============================================================
 * 에러 핸들러 및 인터럽트 핸들러
 * ============================================================ */

/**
 * @brief 치명적 오류 처리 함수
 */
void Error_Handler(void)
{
    __disable_irq();
    printf("[ERROR] Error_Handler 호출!\r\n");

    /* LED를 빠르게 점멸하여 오류 표시 */
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
        HAL_Delay(100);
    }
}

/**
 * @brief HardFault 핸들러 (MPU 위반 등)
 */
void HardFault_Handler(void)
{
    printf("[FAULT] HardFault 발생!\r\n");
    while (1) {}
}

/**
 * @brief SysTick 핸들러
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
