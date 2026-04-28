#ifndef SPI_HAL_H
#define SPI_HAL_H

#include <stdint.h>
#include <stdbool.h>

/* STM32L552 SPI Register Base Addresses */
#define SPI1_BASE_ADDR      0x40013000UL
#define SPI2_BASE_ADDR      0x40003800UL
#define SPI3_BASE_ADDR      0x40003C00UL

/* RCC Base */
#define RCC_BASE_ADDR       0x40021000UL
#define RCC_APB1ENR1        (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x58))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x60))

/* GPIOA Base */
#define GPIOA_BASE_ADDR     0x42020000UL
#define GPIOB_BASE_ADDR     0x42020400UL

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CFG1;
    volatile uint32_t CFG2;
    volatile uint32_t IER;
    volatile uint32_t SR;
    volatile uint32_t IFCR;
    volatile uint32_t RESERVED0;
    volatile uint32_t TXDR;
    volatile uint32_t RESERVED1[3];
    volatile uint32_t RXDR;
    volatile uint32_t RESERVED2[3];
    volatile uint32_t CRCPOLY;
    volatile uint32_t TXCRC;
    volatile uint32_t RXCRC;
    volatile uint32_t UDRDR;
    volatile uint32_t I2SCFGR;
} SPI_TypeDef;

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
    volatile uint32_t BRR;
} GPIO_TypeDef;

#define SPI1    ((SPI_TypeDef *)SPI1_BASE_ADDR)
#define SPI2    ((SPI_TypeDef *)SPI2_BASE_ADDR)
#define GPIOA   ((GPIO_TypeDef *)GPIOA_BASE_ADDR)
#define GPIOB   ((GPIO_TypeDef *)GPIOB_BASE_ADDR)

/* SPI CFG1 bits */
#define SPI_CFG1_DSIZE_Pos  0U
#define SPI_CFG1_MBR_Pos    28U

/* SPI CR1 bits */
#define SPI_CR1_SPE         (1U << 0)
#define SPI_CR1_CSTART      (1U << 9)
#define SPI_CR1_CSUSP       (1U << 10)
#define SPI_CR1_HDDIR       (1U << 11)
#define SPI_CR1_SSI         (1U << 12)

/* SPI CR2 bits */
#define SPI_CR2_TSIZE_Pos   0U

/* SPI CFG2 bits */
#define SPI_CFG2_CPOL       (1U << 25)
#define SPI_CFG2_CPHA       (1U << 24)
#define SPI_CFG2_MASTER     (1U << 22)
#define SPI_CFG2_SSM        (1U << 26)
#define SPI_CFG2_SSOE       (1U << 29)
#define SPI_CFG2_MIDI_Pos   4U

/* SPI SR bits */
#define SPI_SR_RXP          (1U << 0)
#define SPI_SR_TXP          (1U << 1)
#define SPI_SR_EOT          (1U << 3)
#define SPI_SR_RXWNE        (1U << 15)

/* SPI IFCR bits */
#define SPI_IFCR_EOTC       (1U << 3)
#define SPI_IFCR_TXTFC      (1U << 4)

/* CS Pin definitions */
/* MCP3465R CS: PA4 (SPI1), DAC80501 CS: PB0 (SPI2) */
#define MCP3465R_CS_PIN     4U
#define DAC80501_CS_PIN     0U

typedef enum {
    SPI_OK = 0,
    SPI_ERROR,
    SPI_TIMEOUT
} SPI_Status;

typedef enum {
    SPI_DEVICE_MCP3465R = 0,
    SPI_DEVICE_DAC80501
} SPI_Device;

void     SPI_Init(void);
SPI_Status SPI_TransmitReceive(SPI_Device dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
void     SPI_CS_Assert(SPI_Device dev);
void     SPI_CS_Deassert(SPI_Device dev);

/* Simple delay (cycles) */
static inline void delay_us(uint32_t us) {
    /* At 110 MHz, 1 us ≈ 110 cycles */
    volatile uint32_t count = us * 110U;
    while (count--) { __asm__("nop"); }
}

#endif /* SPI_HAL_H */
