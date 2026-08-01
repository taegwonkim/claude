/**
 * main.h
 *
 * Minimal CubeMX-style main header for this project. If you generate the
 * project from STM32CubeMX using the pin assignment in README.md, merge
 * this with (or replace) the generated main.h - the important part is
 * that FPGA_TRIGGER_GPIO_PORT/PIN (app_config.h) match whatever pin
 * label CubeMX assigns to the Cyclone IV trigger input.
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

void Error_Handler(void);

/* Cyclone IV trigger pin - user label, matches app_config.h FPGA_TRIGGER_* */
#define FPGA_TRIGGER_Pin        GPIO_PIN_0
#define FPGA_TRIGGER_GPIO_Port  GPIOB
#define FPGA_TRIGGER_EXTI_IRQn  EXTI0_IRQn

/* W25Q40CL chip-select - user label, matches app_config.h EEPROM_CS_* */
#define EEPROM_CS_Pin           GPIO_PIN_4
#define EEPROM_CS_GPIO_Port     GPIOA

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
