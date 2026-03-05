/**
 ******************************************************************************
 * @file    fx_stm32_sd_driver.h
 * @brief   FileX STM32H5 SDMMC1 미디어 드라이버 헤더
 ******************************************************************************
 */
#ifndef FX_STM32_SD_DRIVER_H
#define FX_STM32_SD_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fx_api.h"
#include "stm32h5xx_hal.h"

extern SD_HandleTypeDef hsd1;

#define FX_SD_SECTOR_SIZE   512U
#define FX_SD_TIMEOUT_MS    5000U

VOID fx_stm32_sd_driver(FX_MEDIA *media_ptr);

#ifdef __cplusplus
}
#endif
#endif /* FX_STM32_SD_DRIVER_H */
