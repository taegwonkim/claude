/**
 ******************************************************************************
 * @file    usart1_dma_idle.c
 * @brief   STM32L552R USART1 + DMA1(Normal mode) + IDLE Line interrupt 수신
 *
 *  - PC --> USART1 RX : STX(0x02) DATA... ETX(0x03)
 *  - DMA Normal Mode 로 RX 버퍼에 적재
 *  - IDLE Line Interrupt 발생 시 현재까지 수신된 길이를 계산
 *      ( buf_size - DMA CNDTR )
 *  - 버퍼 안에서 STX..ETX 프레임을 파싱하여 큐에 넣고 콜백 호출
 *  - DMA Normal 모드이므로 IDLE 또는 TC 처리 후 DMA 를 재시작한다.
 *
 *  USART1 : PA9(TX) / PA10(RX)  AF7
 *  DMA1 Channel 5 : USART1_RX  (STM32L5 DMAMUX REQ_USART1_RX = 24)
 ******************************************************************************
 */
#include "usart1_dma_idle.h"
#include <string.h>

/* ===== 핸들 ===== */
UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;

/* ===== 내부 버퍼 ===== */
static uint8_t  s_rx_buf[USART1_RX_BUF_SIZE];
static uint16_t s_old_pos = 0;

/* 단일 프레임 슬롯 (간단한 더블 버퍼) */
static volatile bool        s_frame_ready = false;
static Usart1_Frame_t       s_frame;

/* STX 시작 위치 추적용 (프레임이 두 IDLE 에 걸쳐 분할 수신되는 경우 대비) */
static uint8_t  s_assemble[USART1_FRAME_MAX_SIZE + 2];
static uint16_t s_assemble_len = 0;
static bool     s_in_frame = false;

/* ===== 내부 함수 ===== */
static void USART1_ProcessByte(uint8_t b);
static void USART1_ProcessChunk(const uint8_t *p, uint16_t len);
static void USART1_RestartDMA(void);

/* =====================================================================
 *  HAL MSP : USART1 GPIO/DMA/NVIC 초기화
 * ===================================================================== */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMAMUX1_CLK_ENABLE();

    /* PA9 = TX, PA10 = RX */
    GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* DMA1 Channel5 : USART1_RX, Normal mode */
    hdma_usart1_rx.Instance                 = DMA1_Channel5;
    hdma_usart1_rx.Init.Request             = DMA_REQUEST_USART1_RX;
    hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode                = DMA_NORMAL;     /* Normal mode */
    hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(huart, hdmarx, hdma_usart1_rx);

    /* NVIC */
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* =====================================================================
 *  USART1 + DMA(Normal) + IDLE 초기화
 * ===================================================================== */
HAL_StatusTypeDef USART1_DMA_IDLE_Init(void)
{
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = 115200;
    huart1.Init.WordLength             = UART_WORDLENGTH_8B;
    huart1.Init.StopBits               = UART_STOPBITS_1;
    huart1.Init.Parity                 = UART_PARITY_NONE;
    huart1.Init.Mode                   = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK)            return HAL_ERROR;
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) return HAL_ERROR;
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) return HAL_ERROR;
    if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK) return HAL_ERROR;

    s_old_pos      = 0;
    s_assemble_len = 0;
    s_in_frame     = false;
    s_frame_ready  = false;

    /* DMA 수신 시작 (Normal mode) */
    if (HAL_UART_Receive_DMA(&huart1, s_rx_buf, USART1_RX_BUF_SIZE) != HAL_OK) {
        return HAL_ERROR;
    }

    /* IDLE Line 인터럽트 enable */
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    return HAL_OK;
}

/* =====================================================================
 *  USART1 IRQ Handler  (stm32l5xx_it.c 의 USART1_IRQHandler 에서 호출)
 *
 *  void USART1_IRQHandler(void) { USART1_DMA_IDLE_IRQHandler(); }
 * ===================================================================== */
void USART1_DMA_IDLE_IRQHandler(void)
{
    /* IDLE Line 검출 */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);

        /* 현재 DMA 가 채운 위치 = 버퍼크기 - 남은 카운트 */
        uint16_t cndtr   = (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
        uint16_t new_pos = (uint16_t)(USART1_RX_BUF_SIZE - cndtr);

        if (new_pos != s_old_pos)
        {
            if (new_pos > s_old_pos) {
                USART1_ProcessChunk(&s_rx_buf[s_old_pos],
                                    (uint16_t)(new_pos - s_old_pos));
            } else {
                /* wrap (Normal 모드에서는 발생하지 않지만 안전 처리) */
                USART1_ProcessChunk(&s_rx_buf[s_old_pos],
                                    (uint16_t)(USART1_RX_BUF_SIZE - s_old_pos));
                if (new_pos > 0) {
                    USART1_ProcessChunk(&s_rx_buf[0], new_pos);
                }
            }
            s_old_pos = new_pos;
        }

        /* Normal 모드에서 버퍼가 거의 다 찼다면 재시작 */
        if (s_old_pos >= (USART1_RX_BUF_SIZE - 1)) {
            USART1_RestartDMA();
        }
    }

    /* HAL 의 다른 에러/플래그 처리 */
    HAL_UART_IRQHandler(&huart1);
}

/* =====================================================================
 *  DMA RX 전송완료 (Normal mode → 버퍼 다 채워짐)
 *  HAL_UART_RxCpltCallback 으로도 들어옴 → 재시작.
 * ===================================================================== */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    uint16_t new_pos = USART1_RX_BUF_SIZE;
    if (new_pos > s_old_pos) {
        USART1_ProcessChunk(&s_rx_buf[s_old_pos],
                            (uint16_t)(new_pos - s_old_pos));
    }
    USART1_RestartDMA();
}

/* =====================================================================
 *  DMA 재시작 (Normal mode 이므로 매번 다시 걸어야 함)
 * ===================================================================== */
static void USART1_RestartDMA(void)
{
    HAL_UART_AbortReceive(&huart1);
    s_old_pos = 0;
    HAL_UART_Receive_DMA(&huart1, s_rx_buf, USART1_RX_BUF_SIZE);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

/* =====================================================================
 *  바이트 단위 STX/ETX 파서
 * ===================================================================== */
static void USART1_ProcessByte(uint8_t b)
{
    if (!s_in_frame)
    {
        if (b == FRAME_STX) {
            s_in_frame     = true;
            s_assemble_len = 0;
        }
        return;
    }

    if (b == FRAME_ETX)
    {
        /* 프레임 완성 */
        if (s_assemble_len <= USART1_FRAME_MAX_SIZE)
        {
            memcpy(s_frame.data, s_assemble, s_assemble_len);
            s_frame.length = s_assemble_len;
            s_frame_ready  = true;

            USART1_FrameReceivedCallback(s_frame.data, s_frame.length);
        }
        s_in_frame     = false;
        s_assemble_len = 0;
        return;
    }

    if (b == FRAME_STX) {
        /* ETX 없이 새 STX → 재동기 */
        s_assemble_len = 0;
        return;
    }

    if (s_assemble_len < USART1_FRAME_MAX_SIZE) {
        s_assemble[s_assemble_len++] = b;
    } else {
        /* overflow → 폐기 */
        s_in_frame     = false;
        s_assemble_len = 0;
    }
}

static void USART1_ProcessChunk(const uint8_t *p, uint16_t len)
{
    for (uint16_t i = 0; i < len; ++i) {
        USART1_ProcessByte(p[i]);
    }
}

/* =====================================================================
 *  메인 루프 polling API
 * ===================================================================== */
bool USART1_GetFrame(Usart1_Frame_t *out)
{
    if (!s_frame_ready || out == NULL) return false;

    __disable_irq();
    memcpy(out, &s_frame, sizeof(Usart1_Frame_t));
    s_frame_ready = false;
    __enable_irq();
    return true;
}

/* =====================================================================
 *  사용자가 override 가능한 weak 콜백
 * ===================================================================== */
__weak void USART1_FrameReceivedCallback(const uint8_t *data, uint16_t len)
{
    (void)data; (void)len;
    /* override in user code */
}
