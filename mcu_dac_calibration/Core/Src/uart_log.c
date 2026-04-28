#include "uart_log.h"
#include "spi_hal.h"   /* for GPIO/RCC types */
#include <string.h>
#include <stdio.h>

/* RCC APB1ENR1: USART2EN bit 17 */
#define RCC_APB1ENR1_USART2EN   (1U << 17)

void UART_Init(void)
{
    /* Enable USART2 clock */
    *(volatile uint32_t *)(RCC_BASE_ADDR + 0x58) |= RCC_APB1ENR1_USART2EN;

    /* PA2 → AF7 (USART2_TX), PA3 → AF7 (USART2_RX) */
    GPIOA->MODER &= ~((3U << 4) | (3U << 6));
    GPIOA->MODER |=  ((2U << 4) | (2U << 6));
    GPIOA->AFR[0] &= ~((0xFU << 8) | (0xFU << 12));
    GPIOA->AFR[0] |=  ((7U  << 8) | (7U  << 12));

    /* BRR: 110 MHz / 115200 ≈ 955 */
    USART2->BRR = 955U;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

void UART_PutChar(char c)
{
    while (!(USART2->ISR & USART_ISR_TXE));
    USART2->TDR = (uint8_t)c;
}

void UART_Print(const char *s)
{
    while (*s) UART_PutChar(*s++);
}

/* Simple float→string, avoids printf float overhead */
void UART_PrintFloat(const char *label, float val)
{
    char buf[48];
    int32_t whole = (int32_t)val;
    int32_t frac  = (int32_t)((val - (float)whole) * 1000.0f);
    if (frac < 0) frac = -frac;
    snprintf(buf, sizeof(buf), "%s%ld.%03ld mV\r\n", label, (long)whole, (long)frac);
    UART_Print(buf);
}

void UART_PrintHex32(const char *label, uint32_t val)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%s0x%06lX\r\n", label, (unsigned long)val);
    UART_Print(buf);
}

void UART_PrintInt32(const char *label, int32_t val)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%ld\r\n", label, (long)val);
    UART_Print(buf);
}
