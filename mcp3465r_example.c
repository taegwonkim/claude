/**
 * MCP3465R usage example — STM32L552 + FreeRTOS
 *
 * Place this code inside your freertos.c / main_task or equivalent.
 * Assumes CubeMX generated hspi1, FreeRTOS kernel, and GPIO for CS pin.
 *
 * Wiring (mandatory):
 *   REFIN+ ──── 3.0 V precision reference (e.g. LM4040-3.0 or REF3030)
 *   REFIN- ──── AGND
 *   VIN pin ─── voltage divider output
 *
 *   Voltage divider for 0–5 V input:
 *              VIN_SYS
 *                 │
 *               2 kΩ  (R1)
 *                 │
 *                 ├──── MCP3465R CH0+ (AINP)
 *                 │
 *               3 kΩ  (R2)
 *                 │
 *               AGND
 *
 *   V_adc = V_sys × 3/(2+3) = V_sys × 0.6
 *   At V_sys = 5 V → V_adc = 3.0 V = VREF  → code = +0x7FFFFF  ✓
 *
 * SPI configuration (CubeMX):
 *   Mode       : Full-Duplex Master
 *   Data size  : 8-bit
 *   CPOL       : Low  (mode 0,0)
 *   CPHA       : 1 Edge
 *   Baud rate  : ≤ 20 MHz  (MCP3465R max: 20 MHz)
 *   NSS        : Software (CS controlled by mcp3465r.c)
 */

#include "mcp3465r.h"

/* -------------------------------------------------------------------------
 * Thread attributes
 * --------------------------------------------------------------------- */
static const osThreadAttr_t attr_init = {
    .name       = "mcp_init",
    .stack_size = 512 * 4,          /* 512 words × 4 bytes */
    .priority   = osPriorityHigh,
};

static const osThreadAttr_t attr_meas = {
    .name       = "mcp_meas",
    .stack_size = 256 * 4,
    .priority   = osPriorityNormal,
};

/* -------------------------------------------------------------------------
 * Application entry point
 * Call once from MX_FREERTOS_Init() or equivalent.
 * --------------------------------------------------------------------- */
void App_MCP3465R_Start(void)
{
    /*
     * Init task runs once: reset → configure → calibrate.
     * It sets g_cal_valid=true when done, then self-terminates.
     * The measure task spins until g_cal_valid is true.
     */
    osThreadNew(Task_MCP3465R_Init,    NULL, &attr_init);
    osThreadNew(Task_MCP3465R_Measure, NULL, &attr_meas);
}

/* -------------------------------------------------------------------------
 * Example: read current voltage from another task
 * --------------------------------------------------------------------- */
void SomeOtherTask(void *arg)
{
    (void)arg;

    for (;;) {
        if (g_cal_valid) {
            /* g_voltage_mv is updated every 100 ms by Task_MCP3465R_Measure */
            float v = g_voltage_mv;
            /* Use v here — e.g., transmit over UART, log to flash, etc. */
            (void)v;
        }
        osDelay(200);
    }
}
