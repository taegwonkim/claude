#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "spi_hal.h"
#include "mcp3465r.h"
#include "dac80501.h"
#include "calibration.h"
#include "uart_log.h"

/* -----------------------------------------------------------------------
 * Minimal system clock setup for STM32L552
 * Target: 110 MHz using PLL (HSI 16 MHz → PLL M=1 N=55/2 → 110 MHz)
 * ----------------------------------------------------------------------- */

#define FLASH_BASE_ADDR     0x40022000UL
#define FLASH_ACR           (*(volatile uint32_t *)(FLASH_BASE_ADDR + 0x00))
#define PWR_BASE_ADDR       0x40007000UL
#define PWR_CR1             (*(volatile uint32_t *)(PWR_BASE_ADDR + 0x00))
#define RCC_CR              (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x00))
#define RCC_PLLCFGR         (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x0C))
#define RCC_CFGR            (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x08))
#define RCC_CFGR_SW_PLL     (0x3U << 0)
#define RCC_CFGR_SWS_PLL    (0x3U << 2)

static void SystemClock_Init(void)
{
    /* Set flash latency to 5 WS for 110 MHz, enable prefetch, icache, dcache */
    FLASH_ACR = (5U | (1U << 8) | (1U << 9) | (1U << 10) | (1U << 11));
    while ((FLASH_ACR & 0x7U) != 5U);

    /* Voltage range 1 (1.2 V) for 110 MHz */
    PWR_CR1 = (PWR_CR1 & ~(3U << 9)) | (1U << 9);

    /* Enable HSI16 */
    RCC_CR |= (1U << 8);
    while (!(RCC_CR & (1U << 10)));

    /* PLL: src=HSI16, M=1, N=55, P=2(not used), Q=2(not used), R=2 → 110 MHz */
    RCC_PLLCFGR = (0x2U << 0)   |   /* PLL src = HSI16 */
                  (0U   << 4)   |   /* M = /1 (0+1) */
                  (55U  << 8)   |   /* N = 55 */
                  (0U   << 17)  |   /* P = /2 */
                  (1U   << 20)  |   /* PLLQEN = on (for USB etc, optional) */
                  (1U   << 21)  |   /* Q = /2 */
                  (1U   << 24);     /* PLLREN = enable R output */

    /* Enable PLL */
    RCC_CR |= (1U << 24);
    while (!(RCC_CR & (1U << 25)));

    /* Switch system clock to PLL */
    RCC_CFGR = (RCC_CFGR & ~0x3U) | RCC_CFGR_SW_PLL;
    while ((RCC_CFGR & (3U << 2)) != RCC_CFGR_SWS_PLL);
}

/* -----------------------------------------------------------------------
 * SysTick-based ms counter (optional, used for coarse waits in main)
 * ----------------------------------------------------------------------- */
#define SYSTICK_BASE_ADDR   0xE000E010UL
#define SYSTICK_CTRL    (*(volatile uint32_t *)(SYSTICK_BASE_ADDR + 0x00))
#define SYSTICK_LOAD    (*(volatile uint32_t *)(SYSTICK_BASE_ADDR + 0x04))
#define SYSTICK_VAL     (*(volatile uint32_t *)(SYSTICK_BASE_ADDR + 0x08))

static volatile uint32_t g_ms_tick = 0;

/* SysTick_Handler must be placed in the vector table; declared weak here */
__attribute__((weak)) void SysTick_Handler(void)
{
    g_ms_tick++;
}

static void SysTick_Init(void)
{
    SYSTICK_LOAD = 110000U - 1U;  /* 1 ms at 110 MHz */
    SYSTICK_VAL  = 0;
    SYSTICK_CTRL = 0x7U;          /* enable, tickint, use processor clock */
}

static void delay_ms(uint32_t ms)
{
    uint32_t target = g_ms_tick + ms;
    while (g_ms_tick < target) { __asm__("nop"); }
}

/* -----------------------------------------------------------------------
 * Calibration report printer
 * ----------------------------------------------------------------------- */

void Calibration_PrintReport(const CalibrationReport *r)
{
    UART_Print("\r\n========== CALIBRATION REPORT ==========\r\n");
    UART_Print("-- Offset step (DAC = 0.5 V) --\r\n");
    UART_PrintFloat("  ADC raw reading  : ", r->adc_offset_raw_mv);
    UART_PrintFloat("  Offset error     : ", r->offset_error_mv);
    UART_PrintInt32("  OFFSETCAL reg    : ", r->offset_reg);
    UART_Print("-- Gain step (DAC = 4.5 V) --\r\n");
    UART_PrintFloat("  ADC raw reading  : ", r->adc_gain_raw_mv);
    UART_PrintFloat("  Gain factor      : ", r->gain_factor);
    UART_PrintHex32("  GAINCAL reg      : ", r->gain_reg);
    UART_Print("-- Verification --\r\n");
    UART_PrintFloat("  Offset point     : ", r->verify_offset_mv);
    UART_PrintFloat("  Gain   point     : ", r->verify_gain_mv);
    UART_Print(r->success ? "  Result          : PASS\r\n"
                          : "  Result          : FAIL\r\n");
    UART_Print("========================================\r\n");
}

/* -----------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------- */

int main(void)
{
    SystemClock_Init();
    SysTick_Init();
    UART_Init();
    SPI_Init();

    UART_Print("\r\nSTM32L552 ADC/DAC Auto-Calibration\r\n");
    UART_Print("MCU: STM32L552  ADC: MCP3465R  DAC: DAC80501\r\n\r\n");

    /* --- Initialise ADC (MCP3465R via SPI1) --- */
    MCP3465R_Handle adc;
    if (!MCP3465R_Init(&adc)) {
        UART_Print("ERROR: MCP3465R init failed\r\n");
        while (1);
    }
    UART_Print("MCP3465R initialised OK\r\n");

    /* --- Initialise DAC (DAC80501 via SPI2) --- */
    DAC80501_Handle dac;
    if (!DAC80501_Init(&dac)) {
        UART_Print("ERROR: DAC80501 init failed\r\n");
        while (1);
    }
    UART_Print("DAC80501  initialised OK\r\n");

    /* Wait for supplies to stabilise */
    delay_ms(100);

    /* --- Run calibration --- */
    CalibrationReport report;
    UART_Print("Starting auto-calibration...\r\n");

    CalResult result = Calibration_Run(&adc, &dac, &report);
    Calibration_PrintReport(&report);

    if (result != CAL_OK) {
        UART_Print("Calibration FAILED with code ");
        char buf[8];
        snprintf(buf, sizeof(buf), "%d\r\n", (int)result);
        UART_Print(buf);
        while (1);  /* halt on failure */
    }

    UART_Print("Calibration complete.  Entering measurement loop.\r\n\r\n");

    /* --- Continuous measurement loop (post-calibration) --- */
    while (1) {
        int32_t raw;
        if (MCP3465R_ReadRaw(&adc, &raw)) {
            float mv = MCP3465R_RawToMillivolts(&adc, raw);
            UART_PrintFloat("Vin = ", mv);
        } else {
            UART_Print("ADC read error\r\n");
        }
        delay_ms(500);
    }

    return 0;  /* unreachable */
}
