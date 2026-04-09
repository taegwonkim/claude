#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

/* ----- HAL 핸들 (main.c에서 정의) ----- */
extern SPI_HandleTypeDef  hspi1;
extern TIM_HandleTypeDef  htim2;
extern TIM_HandleTypeDef  htim6;   /* 10ms 제어 타이머 */
extern TIM_HandleTypeDef  htim7;   /* 20ms ADC 타이머  */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

/* ----- 핀 정의 ----- */
#define ADC_CS_Pin          GPIO_PIN_6
#define ADC_CS_GPIO_Port    GPIOB
#define ADC_IRQ_Pin         GPIO_PIN_7
#define ADC_IRQ_GPIO_Port   GPIOB
#define LED_STATUS_Pin      GPIO_PIN_7
#define LED_STATUS_GPIO_Port GPIOC

/* ----- 전역 이벤트 플래그 (ISR → 메인루프 통신) ----- */
/* volatile: 컴파일러 최적화로 인한 캐시 문제 방지 */
typedef struct {
    volatile uint8_t ctrl_tick;      /* TIM6: 10ms 제어 주기 도래 */
    volatile uint8_t adc_tick;       /* TIM7: ADC 읽기 트리거      */
    volatile uint8_t adc_ready;      /* MCP3465R IRQ: 변환 완료    */
    volatile uint8_t uart_rx_ready;  /* UART: 새 데이터 수신       */
    volatile uint8_t uart_tx_done;   /* UART TX DMA 완료           */
    volatile uint8_t spi_done;       /* SPI 전송 완료              */
} AppFlags_t;

extern AppFlags_t g_flags;

/* ----- 에러 핸들러 ----- */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* MAIN_H */
