/**
 * @file    main.c
 * @brief   STM32L552 USART1 DMA Normal + IDLE Line + FreeRTOS 메인
 *
 * ═══════════════════════════════════════════════════════════
 *  STM32CubeMX 설정 요약
 * ═══════════════════════════════════════════════════════════
 *
 *  [RCC]
 *    HSI16 → PLL → SYSCLK = 110 MHz
 *    APB2  = 110 MHz (USART1 클럭 소스)
 *
 *  [USART1]  Pinout: PA9(TX), PA10(RX)
 *    Mode          : Asynchronous
 *    Baud Rate     : 115200
 *    Word Length   : 8 Bits
 *    Parity        : None
 *    Stop Bits     : 1
 *    DMA Settings  : USART1_RX → DMA1 Channel1
 *                    Direction     : Peripheral to Memory
 *                    Mode          : Normal          ← 핵심
 *                    Priority      : Medium
 *                    Data Width    : Byte / Byte
 *    NVIC          : USART1 global interrupt  Enable ← IDLE 처리
 *                    DMA1 Channel1  Enable           ← TC 처리
 *
 *  [DMA1 Channel1]
 *    Request       : USART1_RX  (DMAMUX 자동 설정)
 *
 *  [NVIC]
 *    USART1 global interrupt   : Preemption=5, Sub=0 (FreeRTOS 범위)
 *    DMA1 Channel1             : Preemption=5, Sub=0
 *    ※ FreeRTOS configMAX_SYSCALL_INTERRUPT_PRIORITY = 5
 *       → 해당 우선순위 이하 ISR에서만 FromISR API 사용 가능
 *
 *  [FreeRTOS]  (Middleware → FreeRTOS → CMSIS_V2)
 *    configTICK_RATE_HZ       = 1000
 *    configMAX_SYSCALL_...    = 5
 *    USE_NEWLIB_REENTRANT     = 필요시 활성화
 *
 *  [GPIO]
 *    PA5 → Output Push-Pull (LED, 예시)
 *
 * ═══════════════════════════════════════════════════════════
 */

#include "main.h"
#include "cmsis_os.h"
#include "usart_dma.h"
#include "packet_parser.h"

/* ─────────────────────────────────────────────────────────
 *  HAL 핸들 (CubeMX 생성)
 * ───────────────────────────────────────────────────────── */
UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;

/* ─────────────────────────────────────────────────────────
 *  함수 전방 선언
 * ───────────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);

extern void MX_FREERTOS_Init(void);   /* freertos.c */
extern void app_process_packet(const Packet_t *pkt);

/* ═════════════════════════════════════════════════════════
 *  main
 * ═════════════════════════════════════════════════════════ */
int main(void)
{
    /* 1. HAL 초기화 */
    HAL_Init();

    /* 2. 클럭 설정 */
    SystemClock_Config();

    /* 3. 주변장치 초기화 (CubeMX 생성 순서 유지) */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();

    /* 4. FreeRTOS 커널 초기화 + 태스크/Queue 생성
     *    (USART_DMA_Init 포함) */
    MX_FREERTOS_Init();

    /* 5. 스케줄러 시작 (이후 리턴 없음) */
    osKernelStart();

    /* 정상적으로는 여기에 도달하지 않음 */
    while (1) { }
}

/* ═════════════════════════════════════════════════════════
 *  HAL 콜백 오버라이드
 *
 *  HAL_UART_RxCpltCallback:
 *    DMA Normal 모드에서 설정한 바이트 수가 모두 수신되면 호출됩니다.
 *    (버퍼가 가득 찬 경우)
 * ═════════════════════════════════════════════════════════ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    USART_DMA_RxCpltCallback(huart);
}

/* ─────────────────────────────────────────────────────────
 *  시스템 클럭 설정 (110 MHz, HSI16 + PLL)
 *  CubeMX가 자동 생성하는 내용과 동일합니다.
 * ───────────────────────────────────────────────────────── */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 전압 범위 1 (최고 성능) */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
        Error_Handler();
    }

    /* HSI16 + PLL 설정 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState       = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM      = 4;   /* 16/4  = 4 MHz VCO 입력  */
    RCC_OscInitStruct.PLL.PLLN      = 55;  /* 4×55  = 220 MHz VCO 출력 */
    RCC_OscInitStruct.PLL.PLLP      = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ      = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR      = RCC_PLLR_DIV2; /* 220/2 = 110 MHz */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* 버스 클럭 분주 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    /* Flash 레이턴시: 110 MHz, VOS1 → 5 WS */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/* ─────────────────────────────────────────────────────────
 *  DMA 초기화 (CubeMX 생성)
 *  반드시 MX_USART1_UART_Init() 보다 먼저 호출해야 합니다.
 * ───────────────────────────────────────────────────────── */
static void MX_DMA_Init(void)
{
    /* DMA1 클럭 활성화 */
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* NVIC: DMA1 Channel1 인터럽트
     * FreeRTOS configMAX_SYSCALL_INTERRUPT_PRIORITY 이하로 설정 */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/* ─────────────────────────────────────────────────────────
 *  USART1 초기화 (CubeMX 생성)
 * ───────────────────────────────────────────────────────── */
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
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }

    /* NVIC: USART1 인터럽트 (IDLE 플래그 처리용) */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ─────────────────────────────────────────────────────────
 *  HAL_UART_MspInit: USART1 MSP (CubeMX 생성, stm32l5xx_hal_msp.c 에 위치)
 *  여기서는 참고용으로 main.c 에 포함합니다.
 * ───────────────────────────────────────────────────────── */
void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (uartHandle->Instance == USART1) {

        /* 클럭 활성화 */
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA9  → USART1_TX
         * PA10 → USART1_RX */
        GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* DMA 링크 설정 */
        hdma_usart1_rx.Instance                 = DMA1_Channel1;
        hdma_usart1_rx.Init.Request             = DMA_REQUEST_USART1_RX;
        hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode                = DMA_NORMAL;  /* ← Normal 모드 */
        hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;

        if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) {
            Error_Handler();
        }

        /* DMA 핸들을 UART 핸들에 연결 */
        __HAL_LINKDMA(uartHandle, hdmarx, hdma_usart1_rx);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uartHandle)
{
    if (uartHandle->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
        HAL_DMA_DeInit(uartHandle->hdmarx);
    }
}

/* ─────────────────────────────────────────────────────────
 *  GPIO 초기화
 * ───────────────────────────────────────────────────────── */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA5: LED 제어 출력 핀 (Nucleo 보드 기본 LED) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* ─────────────────────────────────────────────────────────
 *  Error_Handler
 * ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
