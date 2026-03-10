/**
 ******************************************************************************
 * @file    main.h
 * @brief   메인 헤더 파일
 ******************************************************************************
 */

#ifndef MAIN_H
#define MAIN_H

#include "stm32h5xx_hal.h"

/* 외부 핸들 선언 */
extern UART_HandleTypeDef huart3;

/* 함수 선언 */
void Error_Handler(void);

#endif /* MAIN_H */
