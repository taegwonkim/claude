/**
 * @file    eeprom_manager.c
 * @brief   Application-level EEPROM emulation for STM32L552RCT6
 *
 * Wraps the X-CUBE-EEPROM (eeprom_emul) library with a simple typed API.
 * All flash write operations require the flash to be unlocked and re-locked
 * around the call; this wrapper handles that automatically.
 */

#include "eeprom_manager.h"
#include "eeprom_emul.h"
#include "stm32l5xx_hal.h"
#include <string.h>

/* ─── Private helpers ────────────────────────────────────────────────────── */

static EE_MgrStatus_t map_ee_status(EE_Status_t st)
{
  if (st == EE_OK)            return EE_MGR_OK;
  if (st == EE_NO_DATA)       return EE_MGR_NOT_FOUND;
  return EE_MGR_ERROR;
}

/* Unlock flash, run fn, re-lock. Keeps flash locked when idle. */
#define WITH_FLASH_UNLOCKED(expr)               \
  do {                                          \
    HAL_FLASH_Unlock();                         \
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS); \
    (expr);                                     \
    HAL_FLASH_Lock();                           \
  } while (0)

/* ─── Public API implementation ──────────────────────────────────────────── */

EE_MgrStatus_t EE_Manager_Init(void)
{
  EE_Status_t st;

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  st = EE_Init(EE_FORCED_ERASE);   /* EE_NO_ERASE after first boot */

  HAL_FLASH_Lock();

  /* If pages were blank EE_Init returns EE_OK after formatting them.
     A return of EE_CLEAN_NEEDED means a background page-clean is pending;
     treat as success — the library handles it transparently on next write.  */
  if ((st == EE_OK) || (st == EE_CLEAN_NEEDED))
  {
    return EE_MGR_OK;
  }

  return EE_MGR_ERROR;
}

EE_MgrStatus_t EE_Manager_Write(EE_VirtualAddress_t vAddr, uint32_t data)
{
  EE_Status_t st;

  if (vAddr == 0 || vAddr >= VADDR_MAX)
  {
    return EE_MGR_INVALID_PARAM;
  }

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  st = EE_WriteVariable32bits((uint16_t)vAddr, data);

  /* EE_CLEAN_NEEDED: write succeeded but a cleanup transfer is pending */
  if (st == EE_CLEAN_NEEDED)
  {
    /* Execute the pending page transfer immediately */
    st = EE_CleanUp();
  }

  HAL_FLASH_Lock();

  return map_ee_status(st);
}

EE_MgrStatus_t EE_Manager_Read(EE_VirtualAddress_t vAddr, uint32_t *pData)
{
  if (vAddr == 0 || vAddr >= VADDR_MAX || pData == NULL)
  {
    return EE_MGR_INVALID_PARAM;
  }

  EE_Status_t st = EE_ReadVariable32bits((uint16_t)vAddr, pData);
  return map_ee_status(st);
}

EE_MgrStatus_t EE_Manager_Format(void)
{
  EE_Status_t st;

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  st = EE_Format(EE_FORCED_ERASE);

  HAL_FLASH_Lock();

  return map_ee_status(st);
}

EE_MgrStatus_t EE_Manager_IncrementCounter(EE_VirtualAddress_t vAddr,
                                             uint32_t *pNewValue)
{
  uint32_t current = 0;
  EE_MgrStatus_t st;

  st = EE_Manager_Read(vAddr, &current);

  /* treat "not found" as 0 (first boot) */
  if (st == EE_MGR_NOT_FOUND)
  {
    current = 0;
  }
  else if (st != EE_MGR_OK)
  {
    return st;
  }

  current++;

  st = EE_Manager_Write(vAddr, current);

  if ((st == EE_MGR_OK) && (pNewValue != NULL))
  {
    *pNewValue = current;
  }

  return st;
}

EE_MgrStatus_t EE_Manager_WriteMultiple(const EE_Pair_t *pPairs, uint32_t count)
{
  if (pPairs == NULL || count == 0)
  {
    return EE_MGR_INVALID_PARAM;
  }

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  EE_MgrStatus_t result = EE_MGR_OK;

  for (uint32_t i = 0; i < count; i++)
  {
    if (pPairs[i].addr == 0 || pPairs[i].addr >= VADDR_MAX)
    {
      result = EE_MGR_INVALID_PARAM;
      break;
    }

    EE_Status_t st = EE_WriteVariable32bits((uint16_t)pPairs[i].addr,
                                             pPairs[i].value);
    if (st == EE_CLEAN_NEEDED)
    {
      st = EE_CleanUp();
    }

    if (st != EE_OK)
    {
      result = EE_MGR_ERROR;
      break;
    }
  }

  HAL_FLASH_Lock();
  return result;
}
