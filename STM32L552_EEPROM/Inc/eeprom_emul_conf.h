/**
 * @file    eeprom_emul_conf.h
 * @brief   X-CUBE-EEPROM configuration for STM32L552RCT6
 *
 * STM32L552RCT6 Flash layout:
 *   Total: 256KB (0x08000000 ~ 0x0803FFFF)
 *   Bank1: pages  0~63  (0x08000000 ~ 0x0801FFFF, 128KB)
 *   Bank2: pages 64~127 (0x08020000 ~ 0x0803FFFF, 128KB)
 *   Page size: 2KB (0x800)
 *
 * EEPROM emulation area (Bank2 마지막 4 페이지):
 *   Page 0: 0x08038000 (Bank2 page 124)
 *   Page 1: 0x08038800 (Bank2 page 125)
 *   Page 2: 0x08039000 (Bank2 page 126)
 *   Page 3: 0x08039800 (Bank2 page 127)
 */

#ifndef EEPROM_EMUL_CONF_H
#define EEPROM_EMUL_CONF_H

#include "stm32l5xx_hal.h"

/* ─── Flash geometry ────────────────────────────────────────────────────── */
#define FLASH_PAGE_SIZE          0x800UL   /* 2KB per page */
#define FLASH_BANK_SIZE          0x20000UL /* 128KB per bank */

/* ─── EEPROM emulation area ─────────────────────────────────────────────── */
/* 4 pages (8KB) at the end of Bank2. Adjust NB_OF_PAGES ≥ 2. */
#define EEPROM_START_ADDRESS     0x08038000UL
#define NB_OF_PAGES              4U        /* must be even for dual-bank swap */

/* ─── Virtual address / variable count ──────────────────────────────────── */
/* Maximum number of distinct virtual addresses (variables) to store.
   Each 32-bit variable needs one virtual address slot.                       */
#define NB_MAX_WRITTEN_ELEMENTS  100U

/* ─── Data width ─────────────────────────────────────────────────────────── */
/* Choose one: EE_DATA_TYPE_32BITS or EE_DATA_TYPE_16BITS */
#define EE_DATA_TYPE_32BITS

/* ─── Wear levelling ─────────────────────────────────────────────────────── */
/* Number of write operations before triggering a page compaction. */
#define CYCLES_NUMBER            1U

/* ─── CRC verification ───────────────────────────────────────────────────── */
/* Enable CRC check on each read (requires CRC peripheral to be initialised). */
#define EE_ACCESS_32BITS
/* #define EE_CRC_ENABLE */

/* ─── HAL flash timeout ──────────────────────────────────────────────────── */
#define FLASH_TIMEOUT_VALUE      50000U    /* 50 s */

#endif /* EEPROM_EMUL_CONF_H */
