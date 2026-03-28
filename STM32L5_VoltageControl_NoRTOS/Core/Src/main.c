/* =========================================================
 * main.c - STM32L5 전압 제어 (Non-RTOS / Bare-metal HAL)
 *
 * 동작 흐름:
 *   TIM6 ISR (10ms) → g_flags.ctrl_tick = 1
 *   TIM7 ISR (20ms) → g_flags.adc_tick  = 1
 *   UART IDLE ISR   → g_flags.uart_rx_ready = 1
 *   MCP3465R IRQ EXTI ISR → g_flags.adc_ready = 1
 *
 *   슈퍼루프에서 플래그 확인 후 처리:
 *     ctrl_tick → VoltageCtrl_Update()
 *     adc_tick  → MCP3465R_ReadAllBlocking()
 *     adc_ready → MCP3465R_ReadOneResult()  (IRQ 방식 사용 시)
 *     uart_rx_ready → Proto_RxProcess()
 *     항상 → Proto_TxPump()
 * =========================================================*/

#include "main.h"
#include "mcp3465r.h"
#include "voltage_control.h"
#include "uart_protocol.h"
#include "current_monitor.h"

/* ----- HAL 핸들 정의 ----- */
SPI_HandleTypeDef  hspi1;
TIM_HandleTypeDef  htim2;
TIM_HandleTypeDef  htim6;
TIM_HandleTypeDef  htim7;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;
DMA_HandleTypeDef  hdma_usart1_tx;

/* ----- 전역 이벤트 플래그 ----- */
AppFlags_t g_flags = {0};

/* ----- 애플리케이션 전역 객체 ----- */
MCP3465R_Handle_t  g_adc;
VoltageCtrl_t      g_vctrl;
CurrentMonitor_t   g_currmon;

/* ----- 함수 선언 ----- */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM7_Init(void);

/* =========================================================
 * main()
 * =========================================================*/
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();         /* 반드시 UART 전에 */
    MX_USART1_UART_Init();
    MX_SPI1_Init();
    MX_TIM2_Init();
    MX_TIM6_Init();        /* 10ms 제어 타이머 */
    MX_TIM7_Init();        /* 20ms ADC 타이머  */

    /* ----- 애플리케이션 초기화 ----- */
    VoltageCtrl_Init(&g_vctrl);
    VoltageCtrl_EnableAll(&g_vctrl, true);
    CurrMon_Init(&g_currmon);

    /* MCP3465R 초기화 (블로킹, HAL_Delay 사용) */
    if (MCP3465R_Init(&g_adc) != HAL_OK) {
        Proto_SendEvent("ERROR", "MCP3465R init failed");
    } else {
        Proto_SendEvent("INFO", "MCP3465R OK");
    }

    /* UART 초기화 (버전 + 도움말 출력, DMA 수신 시작) */
    Proto_Init();

    /* ----- 타이머 시작 ----- */
    HAL_TIM_Base_Start_IT(&htim6);  /* 10ms 제어 루프 */
    HAL_TIM_Base_Start_IT(&htim7);  /* 20ms ADC 트리거 */

    /* ----- LED 초기화 ----- */
    HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);

    /* =========================================================
     * 슈퍼루프 (Super-loop)
     * ========================================================= */
    uint32_t led_tick = 0;

    while (1) {
        /* ---- 1. 10ms 제어 루프 (TIM6 ISR 플래그) ---- */
        if (g_flags.ctrl_tick) {
            g_flags.ctrl_tick = 0;
            VoltageCtrl_Update(&g_vctrl, &g_adc);
        }

        /* ---- 2. 20ms ADC 읽기 (TIM7 ISR 플래그) ---- */
        if (g_flags.adc_tick) {
            g_flags.adc_tick = 0;
            /* 8채널 전부 폴링 읽기 (최대 ~160ms 블로킹 가능 → IRQ 방식 권장) */
            if (g_adc.initialized) {
                MCP3465R_ReadAllBlocking(&g_adc);
                /* 전류 모니터 업데이트 */
                CurrMon_Update(&g_currmon, &g_adc, &g_vctrl);
            }
        }

        /* ---- 3. MCP3465R IRQ (EXTI 플래그, adc_tick 대신 사용 가능) ----
         * SCAN 모드에서 IRQ마다 단일 채널 읽기 (인터럽트 기반, 빠름)
         * adc_tick 방식과 병행 사용 시 중복 읽기 주의 → 하나만 사용할 것.
         * 여기서는 adc_tick(폴링) 방식을 기본으로 사용하고
         * IRQ 방식은 주석 처리. */
        /*
        if (g_flags.adc_ready) {
            g_flags.adc_ready = 0;
            MCP3465R_ReadOneResult(&g_adc);
            // 8채널 모두 읽혔는지 확인 후 전류 모니터 업데이트
        }
        */

        /* ---- 4. UART RX 처리 (IDLE 인터럽트 플래그) ---- */
        if (g_flags.uart_rx_ready) {
            g_flags.uart_rx_ready = 0;
            uint16_t cur_pos = PROTO_RX_BUF_SIZE
                             - (uint16_t)__HAL_DMA_GET_COUNTER(huart1.hdmarx);
            Proto_RxProcess(cur_pos);
        }

        /* ---- 5. TX DMA 구동 (항상 호출, 큐에 데이터 있을 때만 동작) ---- */
        Proto_TxPump();

        /* ---- 6. LED 하트비트 (1초) ---- */
        if ((HAL_GetTick() - led_tick) >= 1000) {
            led_tick = HAL_GetTick();
            HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
        }
    }
}

/* =========================================================
 * 클럭 설정 (80MHz, RTOS 버전과 동일)
 * =========================================================*/
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 1; osc.PLL.PLLN = 10;
    osc.PLL.PLLP = RCC_PLLP_DIV7;
    osc.PLL.PLLQ = RCC_PLLQ_DIV2;
    osc.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|
                         RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

/* =========================================================
 * GPIO 초기화
 * =========================================================*/
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* MCP3465R CS (PB6, 출력 HIGH) */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    g.Pin = GPIO_PIN_6; g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);

    /* MCP3465R IRQ (PB7, EXTI 하강엣지) */
    g.Pin  = GPIO_PIN_7;
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    /* LED (PC7, 출력) */
    g.Pin = GPIO_PIN_7; g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
}

/* =========================================================
 * DMA 초기화 (UART RX/TX)
 * =========================================================*/
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
}

/* =========================================================
 * USART1 초기화
 * =========================================================*/
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
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void HAL_UART_MspInit(UART_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        g.Pin = GPIO_PIN_9|GPIO_PIN_10; g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &g);

        /* DMA RX (Circular) */
        hdma_usart1_rx.Instance                 = DMA1_Channel1;
        hdma_usart1_rx.Init.Request             = DMA_REQUEST_USART1_RX;
        hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode                = DMA_CIRCULAR;
        hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;
        HAL_DMA_Init(&hdma_usart1_rx);
        __HAL_LINKDMA(h, hdmarx, hdma_usart1_rx);

        /* DMA TX (Normal) */
        hdma_usart1_tx.Instance                 = DMA1_Channel2;
        hdma_usart1_tx.Init.Request             = DMA_REQUEST_USART1_TX;
        hdma_usart1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode                = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority            = DMA_PRIORITY_LOW;
        HAL_DMA_Init(&hdma_usart1_tx);
        __HAL_LINKDMA(h, hdmatx, hdma_usart1_tx);
    }
}

/* =========================================================
 * SPI1 초기화 (MCP3465R)
 * =========================================================*/
static void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* 80/8=10MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
        g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_VERY_HIGH; g.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

/* =========================================================
 * TIM2 - PWM 4채널 (~19.5kHz)
 * =========================================================*/
static void MX_TIM2_Init(void)
{
    TIM_OC_InitTypeDef oc = {0};
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 0;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 4095;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();

    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_4);
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3;
        g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_HIGH; g.Alternate = GPIO_AF1_TIM2;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

/* =========================================================
 * TIM6 - 10ms 기본 타이머 (PI 제어 주기)
 * PSC=799, ARR=999 → 80MHz/800/1000 = 100Hz = 10ms
 * =========================================================*/
static void MX_TIM6_Init(void)
{
    __HAL_RCC_TIM6_CLK_ENABLE();
    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = 799;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = 999;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(TIM6_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(TIM6_IRQn);
}

/* =========================================================
 * TIM7 - 20ms 기본 타이머 (ADC 읽기 주기)
 * PSC=799, ARR=1999 → 80MHz/800/2000 = 50Hz = 20ms
 * =========================================================*/
static void MX_TIM7_Init(void)
{
    __HAL_RCC_TIM7_CLK_ENABLE();
    htim7.Instance               = TIM7;
    htim7.Init.Prescaler         = 799;
    htim7.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim7.Init.Period            = 1999;
    htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim7) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(TIM7_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
}

/* =========================================================
 * 에러 핸들러
 * =========================================================*/
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
        /* HAL_Delay는 SysTick 의존 → 인터럽트 비활성화 후 사용 불가
         * 단순 카운터 루프로 지연 */
        for (volatile uint32_t i = 0; i < 1000000; i++);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { Error_Handler(); }
#endif
