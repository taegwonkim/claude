#ifndef STM32L5XX_HAL_CONF_H
#define STM32L5XX_HAL_CONF_H

/* ── Module enable ────────────────────────────────────────────────────────── */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
/* HAL_FLASH_MODULE_ENABLED is intentionally omitted:
 * flash operations are handled by the custom stm32l552_flash driver
 * which accesses registers directly. */

/* ── Oscillator values ────────────────────────────────────────────────────── */
#if !defined(HSE_VALUE)
#define HSE_VALUE           8000000U
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT 100U
#endif
#if !defined(MSI_VALUE)
#define MSI_VALUE           4000000U
#endif
#if !defined(HSI_VALUE)
#define HSI_VALUE           16000000U
#endif
#if !defined(LSI_VALUE)
#define LSI_VALUE           32000U
#endif
#if !defined(LSE_VALUE)
#define LSE_VALUE           32768U
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT 5000U
#endif
#if !defined(EXTERNAL_SAI1_CLOCK_VALUE)
#define EXTERNAL_SAI1_CLOCK_VALUE 2097000U
#endif
#if !defined(EXTERNAL_SAI2_CLOCK_VALUE)
#define EXTERNAL_SAI2_CLOCK_VALUE 2097000U
#endif

/* ── SysTick ────────────────────────────────────────────────────────────── */
#define  TICK_INT_PRIORITY   15U

/* ── Ethernet (unused) ───────────────────────────────────────────────────── */
#define  USE_RTOS            0U
#define  PREFETCH_ENABLE     1U

/* ── Assert (enable for debug builds) ────────────────────────────────────── */
/* #define USE_FULL_ASSERT */

#ifdef USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

#include "stm32l5xx_hal_def.h"
#include "stm32l5xx_hal_rcc.h"
#include "stm32l5xx_hal_rcc_ex.h"
#include "stm32l5xx_hal_gpio.h"
#include "stm32l5xx_hal_dma.h"
#include "stm32l5xx_hal_dma_ex.h"
#include "stm32l5xx_hal_exti.h"
#include "stm32l5xx_hal_cortex.h"
#include "stm32l5xx_hal_pwr.h"
#include "stm32l5xx_hal_pwr_ex.h"
#include "stm32l5xx_hal_spi.h"
#include "stm32l5xx_hal_uart.h"
#include "stm32l5xx_hal_uart_ex.h"

#endif /* STM32L5XX_HAL_CONF_H */
