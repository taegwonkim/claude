#ifndef STM32L552_FLASH_H
#define STM32L552_FLASH_H

/* Target: STM32L552RCT6
 *   Package : LQFP64
 *   Flash   : 256KB (code "C"), dual-bank (Bank1: p0-63, Bank2: p64-127)
 *   RAM     : 256KB
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Flash memory layout */
#define FLASH_BASE_ADDR         0x08000000UL
#define FLASH_PAGE_SIZE         0x800U          /* 2KB per page */
#define FLASH_TOTAL_PAGES       128U            /* 256KB total, 128 pages */
#define FLASH_BANK_PAGES        64U             /* 64 pages per bank */
#define FLASH_BANK2_START_PAGE  64U
#define FLASH_BANK2_BASE_ADDR   (FLASH_BASE_ADDR + (FLASH_BANK2_START_PAGE * FLASH_PAGE_SIZE))
#define FLASH_END_ADDR          (FLASH_BASE_ADDR + (FLASH_TOTAL_PAGES * FLASH_PAGE_SIZE))

/* User data storage: last 4 pages of Bank2 (pages 124-127, 8KB)
 * Address range: 0x0803E000 - 0x0803FFFF */
#define FLASH_USER_START_PAGE   124U
#define FLASH_USER_START_ADDR   (FLASH_BASE_ADDR + (FLASH_USER_START_PAGE * FLASH_PAGE_SIZE))
#define FLASH_USER_SIZE         (4U * FLASH_PAGE_SIZE)

/* Flash register base */
#define FLASH_REG_BASE          0x40022000UL

typedef struct {
    volatile uint32_t ACR;          /* 0x00 Access control */
    volatile uint32_t PDKEYR;       /* 0x04 Power-down key */
    volatile uint32_t NSKEYR;       /* 0x08 Non-secure key */
    volatile uint32_t SECKEYR;      /* 0x0C Secure key */
    volatile uint32_t OPTKEYR;      /* 0x10 Option key */
    volatile uint32_t RESERVED0;   /* 0x14 */
    volatile uint32_t RESERVED1;   /* 0x18 */
    volatile uint32_t RESERVED2;   /* 0x1C */
    volatile uint32_t NSSR;         /* 0x20 Non-secure status */
    volatile uint32_t SECSR;        /* 0x24 Secure status */
    volatile uint32_t NSCR;         /* 0x28 Non-secure control */
    volatile uint32_t SECCR;        /* 0x2C Secure control */
    volatile uint32_t ECCR;         /* 0x30 ECC register */
} FLASH_Regs_t;

#define FLASH_REGS  ((FLASH_Regs_t *)FLASH_REG_BASE)

/* Unlock keys */
#define FLASH_KEY1              0x45670123UL
#define FLASH_KEY2              0xCDEF89ABUL

/* NSCR bits
 * PNB[6:0] (bits [10:3]): page number within the selected bank (0-63)
 * BKER (bit 11)         : 0 = Bank1, 1 = Bank2
 */
#define FLASH_NSCR_PG           (1U << 0)   /* Programming */
#define FLASH_NSCR_PER          (1U << 1)   /* Page erase */
#define FLASH_NSCR_MER1         (1U << 2)   /* Mass erase bank 1 */
#define FLASH_NSCR_PNB_POS      3U
#define FLASH_NSCR_PNB_MASK     (0x7FU << FLASH_NSCR_PNB_POS)  /* 7-bit page# within bank */
#define FLASH_NSCR_BKER         (1U << 11)  /* Bank select: 0=Bank1, 1=Bank2 */
#define FLASH_NSCR_MER2         (1U << 15)  /* Mass erase bank 2 */
#define FLASH_NSCR_STRT         (1U << 16)  /* Start erase */
#define FLASH_NSCR_OPTSTRT      (1U << 17)  /* Option start */
#define FLASH_NSCR_EOPIE        (1U << 24)  /* End-of-operation interrupt */
#define FLASH_NSCR_ERRIE        (1U << 25)  /* Error interrupt */
#define FLASH_NSCR_OBL_LAUNCH   (1U << 27)  /* Option byte loading */
#define FLASH_NSCR_LOCK         (1U << 31)  /* Lock */

/* NSSR bits */
#define FLASH_NSSR_EOP          (1U << 0)   /* End of operation */
#define FLASH_NSSR_OPERR        (1U << 1)   /* Operation error */
#define FLASH_NSSR_PROGERR      (1U << 3)   /* Programming error */
#define FLASH_NSSR_WRPERR       (1U << 4)   /* Write protection error */
#define FLASH_NSSR_PGAERR       (1U << 5)   /* Programming alignment error */
#define FLASH_NSSR_SIZERR       (1U << 6)   /* Size error */
#define FLASH_NSSR_PGSERR       (1U << 7)   /* Programming sequence error */
#define FLASH_NSSR_BSY          (1U << 16)  /* Busy */
#define FLASH_NSSR_ERRORS       (FLASH_NSSR_OPERR  | FLASH_NSSR_PROGERR | \
                                  FLASH_NSSR_WRPERR | FLASH_NSSR_PGAERR  | \
                                  FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR)

typedef enum {
    FLASH_OK            = 0,
    FLASH_ERR_PARAM     = -1,
    FLASH_ERR_LOCKED    = -2,
    FLASH_ERR_TIMEOUT   = -3,
    FLASH_ERR_WRITE     = -4,
    FLASH_ERR_ERASE     = -5,
    FLASH_ERR_ALIGN     = -6,
    FLASH_ERR_RANGE     = -7,
} flash_status_t;

flash_status_t flash_unlock(void);
flash_status_t flash_lock(void);
flash_status_t flash_erase_page(uint32_t page_num);
flash_status_t flash_write(uint32_t dest_addr, const void *src, size_t len);
flash_status_t flash_read(uint32_t src_addr, void *dest, size_t len);
flash_status_t flash_write_user_data(uint32_t offset, const void *src, size_t len);
flash_status_t flash_read_user_data(uint32_t offset, void *dest, size_t len);

#endif /* STM32L552_FLASH_H */
