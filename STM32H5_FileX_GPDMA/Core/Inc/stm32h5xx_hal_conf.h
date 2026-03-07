/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h5xx_hal_conf.h
  * @brief   HAL 드라이버 설정 파일
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __STM32H5xx_HAL_CONF_H
#define __STM32H5xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================= 사용할 HAL 모듈 선택 =========================== */
#define HAL_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED       /* GPDMA 사용 (필수) */
#define HAL_SD_MODULE_ENABLED        /* SDMMC SD 카드 */
#define HAL_UART_MODULE_ENABLED      /* USART1 UART */
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_ICACHE_MODULE_ENABLED    /* STM32H5 명령어 캐시 */
/* #define HAL_DCACHE_MODULE_ENABLED */  /* 데이터 캐시 (필요시 활성화) */

/* ========================= 클록 설정 ====================================== */
#if !defined(HSE_VALUE)
  #define HSE_VALUE              25000000UL  /* 외부 크리스탈 25MHz (보드별 조정) */
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    100UL       /* ms */
#endif

#if !defined(HSI_VALUE)
  #define HSI_VALUE              64000000UL  /* 내부 HSI 64MHz */
#endif

#if !defined(CSI_VALUE)
  #define CSI_VALUE              4000000UL
#endif

#if !defined(LSI_VALUE)
  #define LSI_VALUE              32000UL
#endif

#if !defined(LSE_VALUE)
  #define LSE_VALUE              32768UL
#endif

/* ========================= SysTick / 타이머 ================================ */
#define TICK_INT_PRIORITY          15U  /* SysTick 우선순위: 가장 낮게 */
#define USE_RTOS                    0U
#define USE_SD_TRANSCEIVER          0U  /* SD 트랜시버 미사용 */

/* ========================= HAL 드라이버 포함 =============================== */
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32h5xx_hal_rcc.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32h5xx_hal_gpio.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32h5xx_hal_dma.h"
  #include "stm32h5xx_hal_dma_ex.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32h5xx_hal_cortex.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32h5xx_hal_pwr.h"
  #include "stm32h5xx_hal_pwr_ex.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32h5xx_hal_flash.h"
  #include "stm32h5xx_hal_flash_ex.h"
#endif

#ifdef HAL_ICACHE_MODULE_ENABLED
  #include "stm32h5xx_hal_icache.h"
#endif

#ifdef HAL_SD_MODULE_ENABLED
  #include "stm32h5xx_hal_sd.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32h5xx_hal_uart.h"
  #include "stm32h5xx_hal_uart_ex.h"
#endif

/* ========================= assert_param ================================== */
#define assert_param(expr) ((void)0U)

#ifdef __cplusplus
}
#endif

#endif /* __STM32H5xx_HAL_CONF_H */
