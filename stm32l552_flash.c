#include "stm32l552_flash.h"
#include <string.h>

#define FLASH_TIMEOUT_CYCLES    100000U

static flash_status_t wait_for_idle(void)
{
    uint32_t timeout = FLASH_TIMEOUT_CYCLES;

    while (FLASH_REGS->NSSR & FLASH_NSSR_BSY) {
        if (--timeout == 0)
            return FLASH_ERR_TIMEOUT;
    }

    if (FLASH_REGS->NSSR & FLASH_NSSR_ERRORS) {
        /* Clear error flags */
        FLASH_REGS->NSSR |= FLASH_NSSR_ERRORS;
        return FLASH_ERR_WRITE;
    }

    return FLASH_OK;
}

flash_status_t flash_unlock(void)
{
    if (!(FLASH_REGS->NSCR & FLASH_NSCR_LOCK))
        return FLASH_OK;

    FLASH_REGS->NSKEYR = FLASH_KEY1;
    FLASH_REGS->NSKEYR = FLASH_KEY2;

    if (FLASH_REGS->NSCR & FLASH_NSCR_LOCK)
        return FLASH_ERR_LOCKED;

    return FLASH_OK;
}

flash_status_t flash_lock(void)
{
    FLASH_REGS->NSCR |= FLASH_NSCR_LOCK;
    return FLASH_OK;
}

flash_status_t flash_erase_page(uint32_t page_num)
{
    flash_status_t status;

    if (page_num >= FLASH_TOTAL_PAGES)
        return FLASH_ERR_RANGE;

    status = wait_for_idle();
    if (status != FLASH_OK)
        return status;

    /* Select page and start erase */
    uint32_t cr = FLASH_REGS->NSCR;
    cr &= ~FLASH_NSCR_PNB_MASK;
    cr |= FLASH_NSCR_PER | ((page_num << FLASH_NSCR_PNB_POS) & FLASH_NSCR_PNB_MASK);
    FLASH_REGS->NSCR = cr;
    FLASH_REGS->NSCR |= FLASH_NSCR_STRT;

    status = wait_for_idle();

    FLASH_REGS->NSCR &= ~(FLASH_NSCR_PER | FLASH_NSCR_PNB_MASK);

    if (status != FLASH_OK)
        return FLASH_ERR_ERASE;

    /* Verify page is erased (all 0xFF) */
    uint32_t addr = FLASH_BASE_ADDR + (page_num * FLASH_PAGE_SIZE);
    for (uint32_t i = 0; i < FLASH_PAGE_SIZE / 4; i++) {
        if (((volatile uint32_t *)addr)[i] != 0xFFFFFFFFU)
            return FLASH_ERR_ERASE;
    }

    return FLASH_OK;
}

/*
 * STM32L552 flash must be programmed in 64-bit (double-word) units.
 * Both words of the double-word must be written consecutively without
 * any other flash access in between.
 */
flash_status_t flash_write(uint32_t dest_addr, const void *src, size_t len)
{
    flash_status_t status;
    const uint8_t *data = (const uint8_t *)src;

    if (!src || len == 0)
        return FLASH_ERR_PARAM;

    if (dest_addr < FLASH_BASE_ADDR || (dest_addr + len) > FLASH_END_ADDR)
        return FLASH_ERR_RANGE;

    /* Destination must be 8-byte aligned */
    if (dest_addr & 0x7U)
        return FLASH_ERR_ALIGN;

    /* Length must be a multiple of 8 */
    if (len & 0x7U)
        return FLASH_ERR_ALIGN;

    status = wait_for_idle();
    if (status != FLASH_OK)
        return status;

    FLASH_REGS->NSCR |= FLASH_NSCR_PG;

    for (size_t i = 0; i < len; i += 8) {
        volatile uint32_t *dest32 = (volatile uint32_t *)(dest_addr + i);
        uint32_t word0, word1;

        memcpy(&word0, data + i,     4);
        memcpy(&word1, data + i + 4, 4);

        /* Write both words of the double-word */
        dest32[0] = word0;
        dest32[1] = word1;

        /* Data synchronization barrier before polling */
        __asm volatile ("dsb" ::: "memory");

        status = wait_for_idle();
        if (status != FLASH_OK)
            break;

        /* Verify written data */
        if (dest32[0] != word0 || dest32[1] != word1) {
            status = FLASH_ERR_WRITE;
            break;
        }
    }

    FLASH_REGS->NSCR &= ~FLASH_NSCR_PG;
    return status;
}

flash_status_t flash_read(uint32_t src_addr, void *dest, size_t len)
{
    if (!dest || len == 0)
        return FLASH_ERR_PARAM;

    if (src_addr < FLASH_BASE_ADDR || (src_addr + len) > FLASH_END_ADDR)
        return FLASH_ERR_RANGE;

    /* Wait until flash is not busy before reading */
    flash_status_t status = wait_for_idle();
    if (status != FLASH_OK)
        return status;

    memcpy(dest, (const void *)src_addr, len);
    return FLASH_OK;
}

/* Erase all user data pages, then write src into the user flash region at offset. */
flash_status_t flash_write_user_data(uint32_t offset, const void *src, size_t len)
{
    flash_status_t status;

    if (!src || len == 0)
        return FLASH_ERR_PARAM;

    if ((offset + len) > FLASH_USER_SIZE)
        return FLASH_ERR_RANGE;

    /* offset and len must satisfy double-word alignment */
    if ((FLASH_USER_START_ADDR + offset) & 0x7U)
        return FLASH_ERR_ALIGN;

    if (len & 0x7U)
        return FLASH_ERR_ALIGN;

    status = flash_unlock();
    if (status != FLASH_OK)
        return status;

    /* Determine which pages are affected and erase them */
    uint32_t first_page = FLASH_USER_START_PAGE + (offset / FLASH_PAGE_SIZE);
    uint32_t last_page  = FLASH_USER_START_PAGE + ((offset + len - 1) / FLASH_PAGE_SIZE);

    for (uint32_t page = first_page; page <= last_page; page++) {
        status = flash_erase_page(page);
        if (status != FLASH_OK) {
            flash_lock();
            return status;
        }
    }

    status = flash_write(FLASH_USER_START_ADDR + offset, src, len);

    flash_lock();
    return status;
}

flash_status_t flash_read_user_data(uint32_t offset, void *dest, size_t len)
{
    if (!dest || len == 0)
        return FLASH_ERR_PARAM;

    if ((offset + len) > FLASH_USER_SIZE)
        return FLASH_ERR_RANGE;

    return flash_read(FLASH_USER_START_ADDR + offset, dest, len);
}
