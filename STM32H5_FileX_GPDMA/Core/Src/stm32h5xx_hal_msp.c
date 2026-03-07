/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h5xx_hal_msp.c
  * @brief   HAL MSP 초기화 / 역초기화
  *
  *  GPDMA 채널 할당:
  *    GPDMA1 Channel 0 → SDMMC1 TX (Memory→Peripheral)
  *    GPDMA1 Channel 1 → SDMMC1 RX (Peripheral→Memory)
  *    GPDMA2 Channel 0 → USART1 TX (Memory→Peripheral)
  *    GPDMA2 Channel 1 → USART1 RX (Peripheral→Memory)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* DMA 핸들 정의 (stm32h5xx_it.c에서도 extern으로 사용) */
DMA_HandleTypeDef hdma_sdmmc1_tx;
DMA_HandleTypeDef hdma_sdmmc1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart1_rx;

/* ============================================================================
 *  HAL_SD_MspInit / HAL_SD_MspDeInit
 *  SDMMC1 + GPDMA1 Channel 0(TX), Channel 1(RX)
 * ============================================================================ */
void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hsd->Instance == SDMMC1)
    {
        /* ------ 클록 활성화 ------ */
        __HAL_RCC_SDMMC1_CLK_ENABLE();
        __HAL_RCC_GPDMA1_CLK_ENABLE();

        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        /* ------ SDMMC1 GPIO 설정 ------ */
        /*
         * PC8  : SDMMC1_D0   (AF12)
         * PC9  : SDMMC1_D1   (AF12)
         * PC10 : SDMMC1_D2   (AF12)
         * PC11 : SDMMC1_D3   (AF12)
         * PC12 : SDMMC1_CK   (AF12)
         * PD2  : SDMMC1_CMD  (AF12)
         */
        GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9 |
                                    GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        GPIO_InitStruct.Pin       = GPIO_PIN_2;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        /* ------ SD 카드 감지 핀 (PD3, 선택) ------ */
        GPIO_InitStruct.Pin   = SD_DETECT_PIN;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(SD_DETECT_GPIO_PORT, &GPIO_InitStruct);

        /* ------ GPDMA1 Channel 0: SDMMC1 TX (Memory → Peripheral) ------ */
        hdma_sdmmc1_tx.Instance                   = GPDMA1_Channel0;
        hdma_sdmmc1_tx.Init.Request               = GPDMA1_REQUEST_SDMMC1;
        hdma_sdmmc1_tx.Init.BlkHWRequest          = DMA_BREQ_SINGLE_BURST;
        hdma_sdmmc1_tx.Init.Direction             = DMA_MEMORY_TO_PERIPH;
        hdma_sdmmc1_tx.Init.SrcInc               = DMA_SINC_INCREMENTED;
        hdma_sdmmc1_tx.Init.DestInc              = DMA_DINC_FIXED;
        hdma_sdmmc1_tx.Init.SrcDataWidth         = DMA_SRC_DATAWIDTH_WORD;
        hdma_sdmmc1_tx.Init.DestDataWidth        = DMA_DEST_DATAWIDTH_WORD;
        hdma_sdmmc1_tx.Init.Priority             = DMA_HIGH_PRIORITY;
        hdma_sdmmc1_tx.Init.SrcBurstLength       = 4U;
        hdma_sdmmc1_tx.Init.DestBurstLength      = 4U;
        hdma_sdmmc1_tx.Init.TransferAllocatedPort =
            DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
        hdma_sdmmc1_tx.Init.TransferEventMode    = DMA_TCEM_BLOCK_TRANSFER;
        hdma_sdmmc1_tx.Init.Mode                 = DMA_NORMAL;

        if (HAL_DMA_Init(&hdma_sdmmc1_tx) != HAL_OK)
        {
            Error_Handler();
        }
        /* SD 핸들의 TX DMA 포인터에 연결 */
        __HAL_LINKDMA(hsd, hdmatx, hdma_sdmmc1_tx);

        /* ------ GPDMA1 Channel 1: SDMMC1 RX (Peripheral → Memory) ------ */
        hdma_sdmmc1_rx.Instance                   = GPDMA1_Channel1;
        hdma_sdmmc1_rx.Init.Request               = GPDMA1_REQUEST_SDMMC1;
        hdma_sdmmc1_rx.Init.BlkHWRequest          = DMA_BREQ_SINGLE_BURST;
        hdma_sdmmc1_rx.Init.Direction             = DMA_PERIPH_TO_MEMORY;
        hdma_sdmmc1_rx.Init.SrcInc               = DMA_SINC_FIXED;
        hdma_sdmmc1_rx.Init.DestInc              = DMA_DINC_INCREMENTED;
        hdma_sdmmc1_rx.Init.SrcDataWidth         = DMA_SRC_DATAWIDTH_WORD;
        hdma_sdmmc1_rx.Init.DestDataWidth        = DMA_DEST_DATAWIDTH_WORD;
        hdma_sdmmc1_rx.Init.Priority             = DMA_HIGH_PRIORITY;
        hdma_sdmmc1_rx.Init.SrcBurstLength       = 4U;
        hdma_sdmmc1_rx.Init.DestBurstLength      = 4U;
        hdma_sdmmc1_rx.Init.TransferAllocatedPort =
            DMA_SRC_ALLOCATED_PORT1 | DMA_DEST_ALLOCATED_PORT0;
        hdma_sdmmc1_rx.Init.TransferEventMode    = DMA_TCEM_BLOCK_TRANSFER;
        hdma_sdmmc1_rx.Init.Mode                 = DMA_NORMAL;

        if (HAL_DMA_Init(&hdma_sdmmc1_rx) != HAL_OK)
        {
            Error_Handler();
        }
        /* SD 핸들의 RX DMA 포인터에 연결 */
        __HAL_LINKDMA(hsd, hdmarx, hdma_sdmmc1_rx);

        /* ------ NVIC: GPDMA1 채널 인터럽트 ------ */
        HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);

        HAL_NVIC_SetPriority(GPDMA1_Channel1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);

        /* ------ NVIC: SDMMC1 인터럽트 ------ */
        HAL_NVIC_SetPriority(SDMMC1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(SDMMC1_IRQn);
    }
}

void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1)
    {
        __HAL_RCC_SDMMC1_CLK_DISABLE();

        HAL_GPIO_DeInit(GPIOC,
            GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
        HAL_GPIO_DeInit(SD_DETECT_GPIO_PORT, SD_DETECT_PIN);

        HAL_DMA_DeInit(hsd->hdmatx);
        HAL_DMA_DeInit(hsd->hdmarx);

        HAL_NVIC_DisableIRQ(GPDMA1_Channel0_IRQn);
        HAL_NVIC_DisableIRQ(GPDMA1_Channel1_IRQn);
        HAL_NVIC_DisableIRQ(SDMMC1_IRQn);
    }
}

/* ============================================================================
 *  HAL_UART_MspInit / HAL_UART_MspDeInit
 *  USART1 + GPDMA2 Channel 0(TX), Channel 1(RX)
 * ============================================================================ */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == USART1)
    {
        /* ------ 클록 활성화 ------ */
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPDMA2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* ------ USART1 GPIO: PA9(TX), PA10(RX) ------ */
        GPIO_InitStruct.Pin       = USART1_TX_PIN | USART1_RX_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(USART1_TX_GPIO_PORT, &GPIO_InitStruct);

        /* ------ GPDMA2 Channel 0: USART1 TX (Memory → Peripheral) ------ */
        hdma_usart1_tx.Instance                   = GPDMA2_Channel0;
        hdma_usart1_tx.Init.Request               = GPDMA2_REQUEST_USART1_TX;
        hdma_usart1_tx.Init.BlkHWRequest          = DMA_BREQ_SINGLE_BURST;
        hdma_usart1_tx.Init.Direction             = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.SrcInc               = DMA_SINC_INCREMENTED;
        hdma_usart1_tx.Init.DestInc              = DMA_DINC_FIXED;
        hdma_usart1_tx.Init.SrcDataWidth         = DMA_SRC_DATAWIDTH_BYTE;
        hdma_usart1_tx.Init.DestDataWidth        = DMA_DEST_DATAWIDTH_BYTE;
        hdma_usart1_tx.Init.Priority             = DMA_LOW_PRIORITY_LOW_WEIGHT;
        hdma_usart1_tx.Init.SrcBurstLength       = 1U;
        hdma_usart1_tx.Init.DestBurstLength      = 1U;
        hdma_usart1_tx.Init.TransferAllocatedPort =
            DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
        hdma_usart1_tx.Init.TransferEventMode    = DMA_TCEM_BLOCK_TRANSFER;
        hdma_usart1_tx.Init.Mode                 = DMA_NORMAL;

        if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
        {
            Error_Handler();
        }
        __HAL_LINKDMA(huart, hdmatx, hdma_usart1_tx);

        /* ------ GPDMA2 Channel 1: USART1 RX (Peripheral → Memory) ------ */
        hdma_usart1_rx.Instance                   = GPDMA2_Channel1;
        hdma_usart1_rx.Init.Request               = GPDMA2_REQUEST_USART1_RX;
        hdma_usart1_rx.Init.BlkHWRequest          = DMA_BREQ_SINGLE_BURST;
        hdma_usart1_rx.Init.Direction             = DMA_PERIPH_TO_MEMORY;
        hdma_usart1_rx.Init.SrcInc               = DMA_SINC_FIXED;
        hdma_usart1_rx.Init.DestInc              = DMA_DINC_INCREMENTED;
        hdma_usart1_rx.Init.SrcDataWidth         = DMA_SRC_DATAWIDTH_BYTE;
        hdma_usart1_rx.Init.DestDataWidth        = DMA_DEST_DATAWIDTH_BYTE;
        hdma_usart1_rx.Init.Priority             = DMA_LOW_PRIORITY_LOW_WEIGHT;
        hdma_usart1_rx.Init.SrcBurstLength       = 1U;
        hdma_usart1_rx.Init.DestBurstLength      = 1U;
        hdma_usart1_rx.Init.TransferAllocatedPort =
            DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
        hdma_usart1_rx.Init.TransferEventMode    = DMA_TCEM_BLOCK_TRANSFER;
        hdma_usart1_rx.Init.Mode                 = DMA_NORMAL;

        if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
        {
            Error_Handler();
        }
        __HAL_LINKDMA(huart, hdmarx, hdma_usart1_rx);

        /* ------ NVIC: GPDMA2 채널 인터럽트 ------ */
        HAL_NVIC_SetPriority(GPDMA2_Channel0_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(GPDMA2_Channel0_IRQn);

        HAL_NVIC_SetPriority(GPDMA2_Channel1_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(GPDMA2_Channel1_IRQn);

        /* ------ NVIC: USART1 인터럽트 ------ */
        HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_DISABLE();

        HAL_GPIO_DeInit(USART1_TX_GPIO_PORT,
                        USART1_TX_PIN | USART1_RX_PIN);

        HAL_DMA_DeInit(huart->hdmatx);
        HAL_DMA_DeInit(huart->hdmarx);

        HAL_NVIC_DisableIRQ(GPDMA2_Channel0_IRQn);
        HAL_NVIC_DisableIRQ(GPDMA2_Channel1_IRQn);
        HAL_NVIC_DisableIRQ(USART1_IRQn);
    }
}
