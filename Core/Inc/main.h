/**
 * @file    main.h
 * @brief   핀 정의, 외부 심볼 선언
 *
 * ============================================================
 *  STM32CubeIDE / CubeMX 설정 요약
 * ============================================================
 *
 *  [SPI1] — AD5641 DAC (전송 전용 마스터)
 *    PA5  : SPI1_SCK
 *    PA7  : SPI1_MOSI
 *    Mode : Transmit Only Master, CPOL=0, CPHA=0, 8-bit, 8 MHz
 *
 *  [SPI2] — MCP3465R ADC (전이중 마스터)
 *    PB13 : SPI2_SCK
 *    PB14 : SPI2_MISO
 *    PB15 : SPI2_MOSI
 *    Mode : Full-Duplex Master, CPOL=0, CPHA=0, 8-bit, 8 MHz
 *
 *  [GPIO Output — CS 핀, Active-LOW]
 *    PB0  : CS_DAC_CH0
 *    PB1  : CS_DAC_CH1
 *    PB2  : CS_DAC_CH2
 *    PB10 : CS_DAC_CH3
 *    PC7  : CS_ADC
 *    초기값 모두 HIGH (비활성)
 *
 *  [GPIO Input — ADC 인터럽트 / EXTI]
 *    PC0  : ADC_INT  (Pull-Up, EXTI Line0, 하강 에지)
 *    NVIC : EXTI Line0 인터럽트 활성화 (우선순위 5 이하 권장)
 *
 *  [USART1] — 디버그 출력 (9600/115200 baud)
 *    PA9  : USART1_TX
 *    PA10 : USART1_RX
 *
 *  [FreeRTOS] — CMSIS-RTOS v2
 *    Heap 크기 : 최소 10 kB 권장
 *    Tick Rate : 1000 Hz (1 ms 해상도)
 * ============================================================
 */
#ifndef __MAIN_H
#define __MAIN_H

#include "stm32l5xx_hal.h"

/* ------------------------------------------------------------------ */
/*  CS 핀 — AD5641 DAC 채널                                             */
/* ------------------------------------------------------------------ */
#define CS_DAC_CH0_PORT     GPIOB
#define CS_DAC_CH0_PIN      GPIO_PIN_0
#define CS_DAC_CH1_PORT     GPIOB
#define CS_DAC_CH1_PIN      GPIO_PIN_1
#define CS_DAC_CH2_PORT     GPIOB
#define CS_DAC_CH2_PIN      GPIO_PIN_2
#define CS_DAC_CH3_PORT     GPIOB
#define CS_DAC_CH3_PIN      GPIO_PIN_10

/* ------------------------------------------------------------------ */
/*  CS 핀 — MCP3465R ADC                                               */
/* ------------------------------------------------------------------ */
#define CS_ADC_PORT         GPIOC
#define CS_ADC_PIN          GPIO_PIN_7

/* ------------------------------------------------------------------ */
/*  INT 핀 — MCP3465R ADC 변환 완료 (Active-LOW)                        */
/* ------------------------------------------------------------------ */
#define ADC_INT_PORT        GPIOC
#define ADC_INT_PIN         GPIO_PIN_0

/* ------------------------------------------------------------------ */
/*  외부 HAL 핸들 (main.c에서 정의, freertos.c 등에서 extern 사용)         */
/* ------------------------------------------------------------------ */
extern SPI_HandleTypeDef  hspi1;  /**< AD5641 DAC용 SPI1 */
extern SPI_HandleTypeDef  hspi2;  /**< MCP3465R ADC용 SPI2 */
extern UART_HandleTypeDef huart1; /**< 디버그 USART1 */

/* ------------------------------------------------------------------ */
/*  오류 핸들러                                                          */
/* ------------------------------------------------------------------ */
void Error_Handler(void);

#endif /* __MAIN_H */
