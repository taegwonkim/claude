/* MCP3465R BSP for STM32L552RCT6
 *
 * Implements the three HAL hooks declared in mcp3465r.h:
 *   mcp3465r_cs_low / cs_high   → PA4 GPIO
 *   mcp3465r_spi_transfer       → SPI1 (HAL blocking)
 *   mcp3465r_delay_us           → DWT cycle counter
 *
 * SPI1 must be initialised (HAL_SPI_Init) before calling any MCP3465R API.
 */

#include "mcp3465r.h"
#include "main.h"
#include <string.h>

/* Maximum single-transfer size for the dummy-TX buffer used in Rx-only calls */
#define SPI_DUMMY_BUF_SIZE  8U

/* ── Chip-select ─────────────────────────────────────────────────────────── */

void mcp3465r_cs_low(void)
{
    HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_RESET);
}

void mcp3465r_cs_high(void)
{
    HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET);
}

/* ── SPI transfer ────────────────────────────────────────────────────────── */
/*
 * Unified transfer function used by the driver.
 *
 * Rules:
 *  tx != NULL, rx == NULL  → transmit-only
 *  tx == NULL, rx != NULL  → receive-only (dummy 0xFF bytes sent)
 *  tx != NULL, rx != NULL  → full-duplex
 *
 * len is always small (≤ 5 bytes for MCP3465R), so a stack dummy buffer
 * for receive-only is acceptable.
 */
int mcp3465r_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    HAL_StatusTypeDef st;

    if (len == 0)
        return 0;

    if (tx && rx) {
        st = HAL_SPI_TransmitReceive(&hspi1,
                                     (uint8_t *)tx, rx,
                                     (uint16_t)len, 10U);
    } else if (tx) {
        st = HAL_SPI_Transmit(&hspi1, (uint8_t *)tx,
                              (uint16_t)len, 10U);
    } else {
        /* Receive-only: send 0xFF as idle/dummy bytes */
        uint8_t dummy[SPI_DUMMY_BUF_SIZE];
        size_t  remaining = len;
        uint8_t *dst      = rx;
        st = HAL_OK;

        while (remaining > 0 && st == HAL_OK) {
            uint16_t chunk = (uint16_t)((remaining < SPI_DUMMY_BUF_SIZE)
                                        ? remaining : SPI_DUMMY_BUF_SIZE);
            memset(dummy, 0xFFU, chunk);
            st = HAL_SPI_TransmitReceive(&hspi1, dummy, dst,
                                         chunk, 10U);
            dst       += chunk;
            remaining -= chunk;
        }
    }

    return (st == HAL_OK) ? 0 : -1;
}

/* ── Microsecond delay (DWT cycle counter) ────────────────────────────────
 * DWT_Init() in main.c must be called before this is used.
 * Works up to ~38 seconds at 110 MHz before CYCCNT wraps.
 */
void mcp3465r_delay_us(uint32_t us)
{
    uint32_t cycles = us * (HAL_RCC_GetHCLKFreq() / 1000000U);
    uint32_t start  = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles)
        ;
}
