/**
 * @file    stm32h5xx_hal_msp.c
 * @brief   HAL MSP (MCU Support Package) 초기화/역초기화
 *
 * CubeMX가 자동 생성하는 파일 형식을 따름.
 * HAL_SD_MspInit / HAL_UART_MspInit 에서 GPIO 핀 AF 및 클럭을 설정함.
 */

#include "main.h"

/* -----------------------------------------------------------------------
 * SDMMC1 MSP
 * ----------------------------------------------------------------------- */
void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hsd->Instance == SDMMC1) {
        /* 클럭 인가 */
        __HAL_RCC_SDMMC1_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        /**
         * SDMMC1 GPIO 핀 설정
         *
         * PC8  → SDMMC1_D0   AF12
         * PC9  → SDMMC1_D1   AF12
         * PC10 → SDMMC1_D2   AF12
         * PC11 → SDMMC1_D3   AF12
         * PC12 → SDMMC1_CK   AF12
         * PD2  → SDMMC1_CMD  AF12
         */

        /* D0~D3, CK: GPIOC */
        GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9 |
                                    GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* CMD: GPIOD */
        GPIO_InitStruct.Pin       = GPIO_PIN_2;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        /* DMA 핸들은 MX_GPDMA1_Init()에서 이미 연결됨 (__HAL_LINKDMA) */
        /* 여기서는 추가 NVIC 설정 없이 main.c에서 관리 */
    }
}

void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1) {
        __HAL_RCC_SDMMC1_CLK_DISABLE();

        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8 | GPIO_PIN_9 |
                                GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);

        HAL_DMA_DeInit(hsd->hdmarx);
        HAL_DMA_DeInit(hsd->hdmatx);

        HAL_NVIC_DisableIRQ(SDMMC1_IRQn);
    }
}

/* -----------------------------------------------------------------------
 * USART1 MSP
 * ----------------------------------------------------------------------- */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /**
         * PA9  → USART1_TX  AF7
         * PA10 → USART1_RX  AF7
         */
        GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* DMA TX 핸들은 MX_GPDMA2_Init()에서 연결됨 */
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
        HAL_DMA_DeInit(huart->hdmatx);
        HAL_NVIC_DisableIRQ(USART1_IRQn);
    }
}
