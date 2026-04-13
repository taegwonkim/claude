/**
 ******************************************************************************
 * @file    stm32l5xx_hal_msp.c
 * @brief   HAL MSP (MCU Support Package) 초기화
 *
 * HAL_xxx_Init() 호출 시 자동으로 호출되는 저수준 하드웨어 초기화 함수들.
 * GPIO 대체 기능(AF) 핀 매핑을 설정합니다.
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Global MSP Init -----------------------------------------------------------*/

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

/* SPI MSP Init/DeInit -------------------------------------------------------*/

/**
 * @brief  SPI MSP 초기화 (GPIO AF 핀 설정)
 *
 * SPI1 (AD5641):
 *   PA5 = SPI1_SCK  (AF5)
 *   PA7 = SPI1_MOSI (AF5)
 *   (MISO 불필요 - DAC는 송신만)
 *
 * SPI2 (MCP3465R):
 *   PB13 = SPI2_SCK  (AF5)
 *   PB14 = SPI2_MISO (AF5)
 *   PB15 = SPI2_MOSI (AF5)
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hspi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA5 = SPI1_SCK, PA7 = SPI1_MOSI */
        GPIO_InitStruct.Pin = SPI1_SCK_PIN | SPI1_MOSI_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
    else if (hspi->Instance == SPI2) {
        __HAL_RCC_SPI2_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* PB13 = SPI2_SCK, PB14 = SPI2_MISO, PB15 = SPI2_MOSI */
        GPIO_InitStruct.Pin = SPI2_SCK_PIN | SPI2_MISO_PIN | SPI2_MOSI_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, SPI1_SCK_PIN | SPI1_MOSI_PIN);
    }
    else if (hspi->Instance == SPI2) {
        __HAL_RCC_SPI2_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, SPI2_SCK_PIN | SPI2_MISO_PIN | SPI2_MOSI_PIN);
    }
}

/* UART MSP Init/DeInit ------------------------------------------------------*/

/**
 * @brief  UART MSP 초기화 (GPIO AF 핀 설정)
 *
 * USART2:
 *   PA2 = USART2_TX (AF7)
 *   PA3 = USART2_RX (AF7)
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA2 = USART2_TX, PA3 = USART2_RX */
        GPIO_InitStruct.Pin = USART2_TX_PIN | USART2_RX_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, USART2_TX_PIN | USART2_RX_PIN);
    }
}
