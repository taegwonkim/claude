/**
 * main.h — CubeMX가 생성하는 표준 main.h에 추가되는 사용자 핀 매크로 정리
 * (실제 프로젝트에서는 CubeMX가 GPIO Label을 기반으로 이 매크로들을 자동 생성합니다.
 *  SurgeDetector.ioc의 GPIO_Label 값과 이름을 일치시켜 두었습니다.)
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

void Error_Handler(void);

/* ---- GPIO 핀 매크로 (CubeMX GPIO_Label 기준 자동 생성분과 동일하게 유지) ---- */
#define LED_STATUS_Pin        GPIO_PIN_0
#define LED_STATUS_GPIO_Port  GPIOA

#define ESP32_EN_Pin           GPIO_PIN_0
#define ESP32_EN_GPIO_Port     GPIOB

#define FPGA_TRIGGER_Pin       GPIO_PIN_4
#define FPGA_TRIGGER_GPIO_Port GPIOB
#define FPGA_TRIGGER_EXTI_IRQn EXTI4_IRQn

#define RS485_DE_Pin           GPIO_PIN_12
#define RS485_DE_GPIO_Port     GPIOB

#define FLASH_CS_Pin            GPIO_PIN_6
#define FLASH_CS_GPIO_Port      GPIOC

#define LED_WIFI_Pin           GPIO_PIN_13
#define LED_WIFI_GPIO_Port     GPIOC

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
