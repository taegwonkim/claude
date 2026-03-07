#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"

/* -----------------------------------------------------------------------
 * 외부 핸들 선언 (main.c에서 정의, 다른 모듈에서 extern으로 사용)
 * ----------------------------------------------------------------------- */
extern SD_HandleTypeDef  hsd1;
extern DMA_HandleTypeDef hdma_sdmmc1_rx;
extern DMA_HandleTypeDef hdma_sdmmc1_tx;
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_tx;

/* -----------------------------------------------------------------------
 * GPIO 핀 정의
 * ----------------------------------------------------------------------- */
/* SD 카드 감지 핀 (Active Low — 카드 삽입 시 LOW) */
#define SD_DETECT_Pin       GPIO_PIN_2
#define SD_DETECT_GPIO_Port GPIOG

/* 사용자 LED (LD1 Green, NUCLEO-H563ZI) */
#define LD1_Pin             GPIO_PIN_0
#define LD1_GPIO_Port       GPIOB

/* -----------------------------------------------------------------------
 * 에러 핸들러
 * ----------------------------------------------------------------------- */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
