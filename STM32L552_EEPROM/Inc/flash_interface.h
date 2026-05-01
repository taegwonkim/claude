/**
 * @file    flash_interface.h
 * @brief   Low-level flash interface declarations for X-CUBE-EEPROM
 */

#ifndef FLASH_INTERFACE_H
#define FLASH_INTERFACE_H

#include <stdint.h>
#include "stm32l5xx_hal.h"

HAL_StatusTypeDef FI_ErasePage(uint32_t pageAddress);
HAL_StatusTypeDef FI_WriteDoubleWord(uint32_t address, uint64_t data64);
HAL_StatusTypeDef FI_CopyPage(uint32_t srcAddress, uint32_t dstAddress);
uint32_t          FI_ReadWord(uint32_t address);
uint64_t          FI_ReadDoubleWord(uint32_t address);

#endif /* FLASH_INTERFACE_H */
