/**
 ******************************************************************************
 * @file    timer_config.h
 * @brief   STM32L552 Timer Interrupt Configuration Header
 *          - TIM6: Basic Timer (1ms 주기 인터럽트)
 *          - TIM2: General Purpose Timer (다양한 주기 설정 가능)
 *          - TIM1: Advanced Timer (PWM + Update 인터럽트)
 ******************************************************************************
 */

#ifndef __TIMER_CONFIG_H
#define __TIMER_CONFIG_H

#include "stm32l5xx_hal.h"

/* ======================== Timer Handles ======================== */
extern TIM_HandleTypeDef htim6;   /* Basic Timer */
extern TIM_HandleTypeDef htim2;   /* General Purpose Timer */
extern TIM_HandleTypeDef htim1;   /* Advanced Timer */

/* ======================== Clock Info ========================== */
/*
 * STM32L552 기본 클럭 설정 (110MHz):
 *
 *   SYSCLK = 110 MHz
 *   AHB Prescaler  = 1  -> HCLK  = 110 MHz
 *   APB1 Prescaler = 1  -> PCLK1 = 110 MHz -> APB1 Timer CLK = 110 MHz
 *   APB2 Prescaler = 1  -> PCLK2 = 110 MHz -> APB2 Timer CLK = 110 MHz
 *
 *   Timer Update Frequency = Timer CLK / ((Prescaler + 1) * (Period + 1))
 */

/* ======================== Function Prototypes ================== */

/* 초기화 함수 */
void MX_TIM6_Init(void);   /* 1ms 주기 Basic Timer */
void MX_TIM2_Init(void);   /* 500ms 주기 General Purpose Timer */
void MX_TIM1_Init(void);   /* 1s 주기 Advanced Timer */

/* 사용자 콜백 (main.c에서 구현) */
void Timer6_1ms_Callback(void);
void Timer2_500ms_Callback(void);
void Timer1_1s_Callback(void);

#endif /* __TIMER_CONFIG_H */
