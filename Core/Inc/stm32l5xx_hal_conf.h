/**
 ******************************************************************************
 * @file    stm32l5xx_hal_conf.h
 * @brief   STM32L5xx HAL 라이브러리 설정
 *
 * 사용하는 HAL 모듈만 활성화하여 빌드 시간 및 코드 크기 최적화
 ******************************************************************************
 */

#ifndef __STM32L5XX_HAL_CONF_H
#define __STM32L5XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Module Selection ############################## */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_PWR_EX_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_RCC_EX_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ########################## Oscillator Values adaptation ################## */
#if !defined (HSE_VALUE)
  #define HSE_VALUE             8000000UL   /*!< HSE crystal: 8MHz */
#endif

#if !defined (HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT   100UL       /*!< HSE startup timeout (ms) */
#endif

#if !defined (MSI_VALUE)
  #define MSI_VALUE             4000000UL   /*!< MSI default: 4MHz */
#endif

#if !defined (HSI_VALUE)
  #define HSI_VALUE             16000000UL  /*!< HSI: 16MHz */
#endif

#if !defined (HSI48_VALUE)
  #define HSI48_VALUE           48000000UL  /*!< HSI48: 48MHz */
#endif

#if !defined (LSI_VALUE)
  #define LSI_VALUE             32000UL     /*!< LSI: 32kHz */
#endif

#if !defined (LSE_VALUE)
  #define LSE_VALUE             32768UL     /*!< LSE: 32.768kHz */
#endif

#if !defined (LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT   5000UL      /*!< LSE startup timeout (ms) */
#endif

#if !defined (EXTERNAL_SAI1_CLOCK_VALUE)
  #define EXTERNAL_SAI1_CLOCK_VALUE  48000UL
#endif

#if !defined (EXTERNAL_SAI2_CLOCK_VALUE)
  #define EXTERNAL_SAI2_CLOCK_VALUE  48000UL
#endif

/* ########################### System Configuration ######################### */
#define VDD_VALUE                   3300UL  /*!< VDD in mV */
#define TICK_INT_PRIORITY           15UL    /*!< tick interrupt priority (lowest) */
#define USE_RTOS                    1U
#define PREFETCH_ENABLE             1U
#define INSTRUCTION_CACHE_ENABLE    1U
#define DATA_CACHE_ENABLE           1U

/* ########################## Assert Selection ############################## */
/* #define USE_FULL_ASSERT    1U */

/* ################## SPI peripheral configuration ########################## */
#define USE_SPI_CRC                 0U

/* Includes ------------------------------------------------------------------*/
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32l5xx_hal_rcc.h"
  #include "stm32l5xx_hal_rcc_ex.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32l5xx_hal_gpio.h"
  #include "stm32l5xx_hal_gpio_ex.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32l5xx_hal_dma.h"
  #include "stm32l5xx_hal_dma_ex.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32l5xx_hal_cortex.h"
#endif

#ifdef HAL_EXTI_MODULE_ENABLED
  #include "stm32l5xx_hal_exti.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32l5xx_hal_flash.h"
  #include "stm32l5xx_hal_flash_ex.h"
  #include "stm32l5xx_hal_flash_ramfunc.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32l5xx_hal_pwr.h"
  #include "stm32l5xx_hal_pwr_ex.h"
#endif

#ifdef HAL_SPI_MODULE_ENABLED
  #include "stm32l5xx_hal_spi.h"
  #include "stm32l5xx_hal_spi_ex.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32l5xx_hal_uart.h"
  #include "stm32l5xx_hal_uart_ex.h"
#endif

/* Assert macro */
#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STM32L5XX_HAL_CONF_H */
