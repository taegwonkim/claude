#include "spi_hal.h"

/* -----------------------------------------------------------------------
 * STM32L552 SPI initialisation (bare-metal, no HAL library)
 *
 * SPI1  → MCP3465R  (ADC)  CPOL=0, CPHA=0, 8-bit, MSB first
 *   PA5 SCK  AF5
 *   PA6 MISO AF5
 *   PA7 MOSI AF5
 *   PA4 NSS  (GPIO, software)
 *
 * SPI2  → DAC80501  (DAC)  CPOL=1, CPHA=1, 8-bit, MSB first
 *   PB13 SCK  AF5
 *   PB14 MISO AF5
 *   PB15 MOSI AF5
 *   PB0  NSS  (GPIO, software)
 * ----------------------------------------------------------------------- */

#define TIMEOUT_CYCLES  1000000U

static uint32_t spi_wait_flag(SPI_TypeDef *spi, uint32_t flag, uint32_t set)
{
    uint32_t t = TIMEOUT_CYCLES;
    if (set) {
        while (!(spi->SR & flag) && --t);
    } else {
        while ((spi->SR & flag) && --t);
    }
    return t;
}

void SPI_Init(void)
{
    /* ---- Clock enable ---- */
    RCC_APB2ENR |= (1U << 12);  /* SPI1 */
    RCC_APB1ENR1 |= (1U << 14); /* SPI2 */

    /* GPIOA and GPIOB clocks (AHB2) */
    *(volatile uint32_t *)(RCC_BASE_ADDR + 0x4C) |= (1U << 0) | (1U << 1);

    /* ---- GPIOA: PA4=output(CS), PA5/6/7=AF5 ---- */
    /* PA4 output */
    GPIOA->MODER &= ~(3U << 8);
    GPIOA->MODER |=  (1U << 8);
    GPIOA->OSPEEDR |= (3U << 8);  /* high speed */
    GPIOA->BSRR = (1U << 4);      /* CS deasserted (high) */

    /* PA5, PA6, PA7 → AF5 */
    GPIOA->MODER &= ~((3U << 10) | (3U << 12) | (3U << 14));
    GPIOA->MODER |=  ((2U << 10) | (2U << 12) | (2U << 14));
    GPIOA->OSPEEDR |= ((3U << 10) | (3U << 12) | (3U << 14));
    /* AFR[0]: PA5=AF5, PA6=AF5, PA7=AF5 */
    GPIOA->AFR[0] &= ~((0xFU << 20) | (0xFU << 24) | (0xFU << 28));
    GPIOA->AFR[0] |=  ((5U  << 20) | (5U  << 24) | (5U  << 28));

    /* ---- GPIOB: PB0=output(CS), PB13/14/15=AF5 ---- */
    GPIOB->MODER &= ~(3U << 0);
    GPIOB->MODER |=  (1U << 0);
    GPIOB->OSPEEDR |= (3U << 0);
    GPIOB->BSRR = (1U << 0);      /* CS high */

    /* PB13, PB14, PB15 → AF5 */
    GPIOB->MODER &= ~((3U << 26) | (3U << 28) | (3U << 30));
    GPIOB->MODER |=  ((2U << 26) | (2U << 28) | (2U << 30));
    GPIOB->OSPEEDR |= ((3U << 26) | (3U << 28) | (3U << 30));
    GPIOB->AFR[1] &= ~((0xFU << 20) | (0xFU << 24) | (0xFU << 28));
    GPIOB->AFR[1] |=  ((5U  << 20) | (5U  << 24) | (5U  << 28));

    /* ---- SPI1 (MCP3465R): CPOL=0, CPHA=0, Master, 8-bit ---- */
    SPI1->CR1  = 0;
    SPI1->CFG1 = ((7U << SPI_CFG1_DSIZE_Pos) |   /* 8-bit */
                  (3U << SPI_CFG1_MBR_Pos));       /* fPCLK/16 ≈ 3.4 MHz */
    SPI1->CFG2 = (SPI_CFG2_MASTER | SPI_CFG2_SSM | SPI_CFG2_SSOE);
    SPI1->CR1  = SPI_CR1_SSI;

    /* ---- SPI2 (DAC80501): CPOL=1, CPHA=1, Master, 8-bit ---- */
    SPI2->CR1  = 0;
    SPI2->CFG1 = ((7U << SPI_CFG1_DSIZE_Pos) |
                  (3U << SPI_CFG1_MBR_Pos));
    SPI2->CFG2 = (SPI_CFG2_MASTER | SPI_CFG2_SSM | SPI_CFG2_SSOE |
                  SPI_CFG2_CPOL | SPI_CFG2_CPHA);
    SPI2->CR1  = SPI_CR1_SSI;
}

void SPI_CS_Assert(SPI_Device dev)
{
    if (dev == SPI_DEVICE_MCP3465R) {
        GPIOA->BSRR = (1U << (MCP3465R_CS_PIN + 16U)); /* low */
    } else {
        GPIOB->BSRR = (1U << (DAC80501_CS_PIN + 16U));
    }
    delay_us(1);
}

void SPI_CS_Deassert(SPI_Device dev)
{
    delay_us(1);
    if (dev == SPI_DEVICE_MCP3465R) {
        GPIOA->BSRR = (1U << MCP3465R_CS_PIN);  /* high */
    } else {
        GPIOB->BSRR = (1U << DAC80501_CS_PIN);
    }
}

SPI_Status SPI_TransmitReceive(SPI_Device dev, const uint8_t *tx,
                                uint8_t *rx, uint16_t len)
{
    SPI_TypeDef *spi = (dev == SPI_DEVICE_MCP3465R) ? SPI1 : SPI2;

    /* Configure transfer size */
    spi->CR2 = (uint32_t)len << SPI_CR2_TSIZE_Pos;

    /* Enable SPI and start transfer */
    spi->CR1 |= SPI_CR1_SPE;
    spi->CR1 |= SPI_CR1_CSTART;

    for (uint16_t i = 0; i < len; i++) {
        if (!spi_wait_flag(spi, SPI_SR_TXP, 1)) {
            spi->CR1 &= ~SPI_CR1_SPE;
            return SPI_TIMEOUT;
        }
        *(volatile uint8_t *)&spi->TXDR = tx ? tx[i] : 0x00U;

        if (!spi_wait_flag(spi, SPI_SR_RXP, 1)) {
            spi->CR1 &= ~SPI_CR1_SPE;
            return SPI_TIMEOUT;
        }
        uint8_t byte = *(volatile uint8_t *)&spi->RXDR;
        if (rx) rx[i] = byte;
    }

    /* Wait for end of transfer */
    if (!spi_wait_flag(spi, SPI_SR_EOT, 1)) {
        spi->CR1 &= ~SPI_CR1_SPE;
        return SPI_TIMEOUT;
    }

    spi->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
    spi->CR1 &= ~SPI_CR1_SPE;

    return SPI_OK;
}
