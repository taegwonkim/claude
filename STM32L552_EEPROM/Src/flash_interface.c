/**
 * @file    flash_interface.c
 * @brief   Low-level flash HAL interface required by X-CUBE-EEPROM
 *          on STM32L552RCT6 (dual-bank, 2KB pages).
 *
 * The eeprom_emul library calls these functions internally.
 * You do NOT call them directly from application code.
 */

#include "flash_interface.h"
#include "eeprom_emul_conf.h"
#include "stm32l5xx_hal.h"

/* ─── Address → bank/page conversion ────────────────────────────────────── */

/**
 * Returns the HAL bank identifier for a given flash address.
 * STM32L552: Bank1 = 0x08000000–0x0801FFFF, Bank2 = 0x08020000–0x0803FFFF
 */
static uint32_t get_bank(uint32_t address)
{
  if (address < (FLASH_BASE + FLASH_BANK_SIZE))
  {
    return FLASH_BANK_1;
  }
  return FLASH_BANK_2;
}

/**
 * Returns the absolute page number (0-based across both banks) for an address.
 */
static uint32_t get_page(uint32_t address)
{
  return (address - FLASH_BASE) / FLASH_PAGE_SIZE;
}

/* ─── eeprom_emul interface ──────────────────────────────────────────────── */

/**
 * @brief  Erase one flash page.
 * @param  pageAddress   Start address of the page to erase.
 * @retval HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef FI_ErasePage(uint32_t pageAddress)
{
  FLASH_EraseInitTypeDef eraseInit = {0};
  uint32_t pageError = 0;

  eraseInit.TypeErase   = FLASH_TYPEERASE_PAGES;
  eraseInit.Banks       = get_bank(pageAddress);
  eraseInit.Page        = get_page(pageAddress);
  eraseInit.NbPages     = 1;

  if (HAL_FLASHEx_Erase(&eraseInit, &pageError) != HAL_OK)
  {
    return HAL_ERROR;
  }
  return (pageError == 0xFFFFFFFFU) ? HAL_OK : HAL_ERROR;
}

/**
 * @brief  Write a 64-bit (double-word) value to flash.
 *         STM32L5 flash minimum write granularity is 8 bytes.
 * @param  address   Destination address (must be 8-byte aligned).
 * @param  data64    64-bit value to write.
 * @retval HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef FI_WriteDoubleWord(uint32_t address, uint64_t data64)
{
  return HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data64);
}

/**
 * @brief  Read a 32-bit value directly from flash (no HAL needed).
 * @param  address   Source address.
 * @retval Value at address.
 */
uint32_t FI_ReadWord(uint32_t address)
{
  return *(__IO uint32_t *)address;
}

/**
 * @brief  Read a 64-bit value directly from flash.
 * @param  address   Source address (8-byte aligned).
 * @retval 64-bit value at address.
 */
uint64_t FI_ReadDoubleWord(uint32_t address)
{
  return *(__IO uint64_t *)address;
}

/**
 * @brief  Copy a full flash page (used internally by X-CUBE-EEPROM during
 *         page transfer / wear-levelling).
 * @param  srcAddress   Source page start address.
 * @param  dstAddress   Destination page start address (already erased).
 * @retval HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef FI_CopyPage(uint32_t srcAddress, uint32_t dstAddress)
{
  for (uint32_t offset = 0; offset < FLASH_PAGE_SIZE; offset += 8U)
  {
    uint64_t data = FI_ReadDoubleWord(srcAddress + offset);

    /* Skip blank double-words to minimise write cycles */
    if (data == 0xFFFFFFFFFFFFFFFFULL)
    {
      continue;
    }

    if (FI_WriteDoubleWord(dstAddress + offset, data) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }
  return HAL_OK;
}
