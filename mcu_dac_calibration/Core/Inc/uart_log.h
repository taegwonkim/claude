#ifndef UART_LOG_H
#define UART_LOG_H

#include <stdint.h>

/* Minimal USART2 driver for debug output (PA2=TX, PA3=RX, 115200 baud @ 110 MHz) */

#define USART2_BASE_ADDR    0x40004400UL

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t BRR;
    volatile uint32_t GTPR;
    volatile uint32_t RTOR;
    volatile uint32_t RQR;
    volatile uint32_t ISR;
    volatile uint32_t ICR;
    volatile uint32_t RDR;
    volatile uint32_t TDR;
    volatile uint32_t PRESC;
} USART_TypeDef;

#define USART2  ((USART_TypeDef *)USART2_BASE_ADDR)

#define USART_CR1_UE    (1U << 0)
#define USART_CR1_TE    (1U << 3)
#define USART_ISR_TXE   (1U << 7)
#define USART_ISR_TC    (1U << 6)

void UART_Init(void);
void UART_PutChar(char c);
void UART_Print(const char *s);
void UART_PrintFloat(const char *label, float val);
void UART_PrintHex32(const char *label, uint32_t val);
void UART_PrintInt32(const char *label, int32_t val);

#endif /* UART_LOG_H */
