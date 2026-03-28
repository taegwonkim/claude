/**
 * @file    main.h
 * @brief   STM32H5 DAC/ADC 피드백 전압 제어 - 전역 핸들 및 공유 정의
 *
 * 타겟 MCU : STM32H563 / STM32H573 (STM32CubeH5 HAL)
 * RTOS     : FreeRTOS v10 (CMSIS-RTOS v2 래퍼 없이 native 사용)
 */

#ifndef MAIN_H
#define MAIN_H

/* ---- STM32H5 HAL ---- */
#include "stm32h5xx_hal.h"

/* ---- FreeRTOS ---- */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "stream_buffer.h"

/* ======================================================================== */
/*  주변장치 핸들 (main.c에서 정의, 다른 파일에서 extern으로 참조)           */
/* ======================================================================== */

extern DAC_HandleTypeDef  hdac1;    /**< DAC1 (CH1=PA4, CH2=PA5) */
extern ADC_HandleTypeDef  hadc1;    /**< ADC1 (4채널 DMA 스캔) */
extern TIM_HandleTypeDef  htim3;    /**< TIM3 PWM (CH1=PA6, CH2=PA7) */
extern UART_HandleTypeDef huart3;   /**< UART3 디버그 출력 */

/* ======================================================================== */
/*  FreeRTOS 공유 객체                                                        */
/* ======================================================================== */

extern SemaphoreHandle_t  xADCDoneSem;  /**< ADC DMA 완료 신호 세마포어 */
extern SemaphoreHandle_t  xCtrlMutex;   /**< 채널 구조체 보호 뮤텍스 */
extern StreamBufferHandle_t xUARTRxStream; /**< UART RX ISR → vUARTRxTask 스트림 버퍼 */

/* ======================================================================== */
/*  태스크 우선순위                                                            */
/* ======================================================================== */

#define TASK_PRIO_ADC       (configMAX_PRIORITIES - 1)  /**< ADC: 최고 우선순위 */
#define TASK_PRIO_CONTROL   (configMAX_PRIORITIES - 2)  /**< PID 제어 */
#define TASK_PRIO_UART_RX   (configMAX_PRIORITIES - 3)  /**< UART RX 명령 처리 */
#define TASK_PRIO_MONITOR   (tskIDLE_PRIORITY + 1)      /**< UART 모니터 */

/* ======================================================================== */
/*  태스크 스택 크기 [words]                                                  */
/* ======================================================================== */

#define STACK_ADC           256U
#define STACK_CONTROL       512U
#define STACK_UART_RX       512U
#define STACK_MONITOR       512U

/* ======================================================================== */
/*  UART RX DMA 버퍼 크기                                                    */
/* ======================================================================== */

#define UART_RX_DMA_SIZE    128U   /**< DMA 원형 수신 버퍼 크기 */
#define UART_RX_STREAM_SIZE 256U   /**< StreamBuffer 크기 */

/* ======================================================================== */
/*  오류 핸들러                                                               */
/* ======================================================================== */

void Error_Handler(void);

#endif /* MAIN_H */
