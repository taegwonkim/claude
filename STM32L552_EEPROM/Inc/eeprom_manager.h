/**
 * @file    eeprom_manager.h
 * @brief   Application-level EEPROM emulation API for STM32L552RCT6
 */

#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "eeprom_emul.h"

/* ─── Virtual address map ────────────────────────────────────────────────── */
/* Add application-specific virtual addresses here (must start from 1).
   0x0000 is reserved by the library.                                         */
typedef enum
{
  VADDR_DEVICE_ID       = 0x0001,
  VADDR_FIRMWARE_VER    = 0x0002,
  VADDR_SENSOR_CALIB    = 0x0003,
  VADDR_CONFIG_FLAGS    = 0x0004,
  VADDR_BOOT_COUNT      = 0x0005,
  VADDR_LAST_ERROR      = 0x0006,
  /* Add more virtual addresses up to NB_MAX_WRITTEN_ELEMENTS */
  VADDR_MAX
} EE_VirtualAddress_t;

/* ─── Result codes ───────────────────────────────────────────────────────── */
typedef enum
{
  EE_MGR_OK              = 0,
  EE_MGR_ERROR           = -1,
  EE_MGR_NOT_FOUND       = -2,
  EE_MGR_INVALID_PARAM   = -3,
} EE_MgrStatus_t;

/* ─── Public API ─────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise EEPROM emulation (call once after HAL_Init).
 * @retval EE_MGR_OK on success
 */
EE_MgrStatus_t EE_Manager_Init(void);

/**
 * @brief  Write a 32-bit value to a virtual address.
 * @param  vAddr   Virtual address (EE_VirtualAddress_t)
 * @param  data    Value to store
 * @retval EE_MGR_OK on success
 */
EE_MgrStatus_t EE_Manager_Write(EE_VirtualAddress_t vAddr, uint32_t data);

/**
 * @brief  Read a 32-bit value from a virtual address.
 * @param  vAddr   Virtual address
 * @param  pData   Pointer to receive the value
 * @retval EE_MGR_OK       – found and read successfully
 *         EE_MGR_NOT_FOUND – address has never been written
 */
EE_MgrStatus_t EE_Manager_Read(EE_VirtualAddress_t vAddr, uint32_t *pData);

/**
 * @brief  Erase all EEPROM emulation pages (factory reset).
 * @retval EE_MGR_OK on success
 */
EE_MgrStatus_t EE_Manager_Format(void);

/**
 * @brief  Increment a counter stored at vAddr (read-modify-write).
 * @param  vAddr        Virtual address
 * @param  pNewValue    Receives the updated counter value (may be NULL)
 * @retval EE_MGR_OK on success
 */
EE_MgrStatus_t EE_Manager_IncrementCounter(EE_VirtualAddress_t vAddr,
                                            uint32_t *pNewValue);

/**
 * @brief  Write multiple variables in one shot.
 *         Array elements: { virtualAddress, value } pairs.
 * @param  pPairs   Pointer to pairs array
 * @param  count    Number of pairs
 * @retval EE_MGR_OK if all writes succeeded
 */
typedef struct { EE_VirtualAddress_t addr; uint32_t value; } EE_Pair_t;
EE_MgrStatus_t EE_Manager_WriteMultiple(const EE_Pair_t *pPairs, uint32_t count);

#endif /* EEPROM_MANAGER_H */
