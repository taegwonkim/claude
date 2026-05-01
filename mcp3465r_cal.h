#ifndef MCP3465R_CAL_H
#define MCP3465R_CAL_H

/* MCP3465R Calibration - Flash storage layout
 *
 * Flash user region (0x0803E000, 8KB):
 *   Page 124 (offset 0x0000): calibration record
 *   Pages 125-127            : reserved / future use
 *
 * Calibration procedure:
 *   Offset: short VIN+/VIN- to AGND, average N samples → OFFSETCAL = -mean
 *   Gain  : apply REFIN+/REFIN- as input, average N samples
 *           GAINCAL = 0x800000 * expected_counts / measured_counts
 */

#include <stdint.h>
#include <stdbool.h>
#include "mcp3465r.h"

#define MCP3465R_CAL_MAGIC      0xCA13DA7AUL    /* "calibrata" */
#define MCP3465R_CAL_VERSION    1U
#define MCP3465R_CAL_AVG_COUNT  64U             /* Samples averaged per calibration step */
#define MCP3465R_CAL_FLASH_OFFSET 0U            /* Byte offset within user flash region */

/* Offset within a 2KB page where a second copy is kept (redundancy) */
#define MCP3465R_CAL_COPY2_OFFSET  FLASH_PAGE_SIZE

typedef struct __attribute__((packed)) {
    uint32_t magic;         /* MCP3465R_CAL_MAGIC */
    uint8_t  version;       /* MCP3465R_CAL_VERSION */
    uint8_t  pga_gain;      /* CONFIG2 gain bits used during calibration */
    uint8_t  osr;           /* CONFIG1 OSR bits used during calibration */
    uint8_t  reserved;

    int32_t  offset;        /* OFFSETCAL value written to hardware register */
    uint32_t gain;          /* GAINCAL  value written to hardware register */

    int32_t  raw_offset_mean;   /* Mean raw code at shorted inputs (diagnostic) */
    int32_t  raw_gain_mean;     /* Mean raw code at Vref input (diagnostic) */

    uint32_t crc32;         /* CRC-32 of all fields above */
} MCP3465R_CalData_t;

/* Padded size aligned to 8 bytes for flash double-word writes */
#define MCP3465R_CAL_PADDED_SIZE  \
    (((sizeof(MCP3465R_CalData_t) + 7U) / 8U) * 8U)

typedef enum {
    CAL_OK              =  0,
    CAL_ERR_FLASH       = -1,
    CAL_ERR_ADC         = -2,
    CAL_ERR_CRC         = -3,
    CAL_ERR_MAGIC       = -4,
    CAL_ERR_NO_DATA     = -5,
    CAL_ERR_PARAM       = -6,
} cal_status_t;

/* Run full calibration (offset + gain) and save to flash.
 * pga_gain_cfg: CONFIG2 gain bits (e.g. MCP3465R_CFG2_GAIN_1)
 * osr_cfg     : CONFIG1 OSR bits
 * vref_counts : expected ADC full-scale count for the reference voltage used
 *               e.g. for Vref=2.048V internal, full-scale = 0x7FFFFF */
cal_status_t mcp3465r_calibrate(uint8_t pga_gain_cfg,
                                uint8_t osr_cfg,
                                int32_t vref_counts);

/* Load calibration from flash and apply to MCP3465R hardware registers.
 * Call once at startup before any conversion. */
cal_status_t mcp3465r_cal_load_and_apply(void);

/* Read back currently stored calibration (without applying). */
cal_status_t mcp3465r_cal_read(MCP3465R_CalData_t *out);

/* Apply a CalData record to the hardware registers (OFFSETCAL, GAINCAL). */
cal_status_t mcp3465r_cal_apply(const MCP3465R_CalData_t *cal);

/* Erase stored calibration (forces re-calibration on next boot). */
cal_status_t mcp3465r_cal_erase(void);

/* True if a valid calibration record exists in flash. */
bool mcp3465r_cal_is_valid(void);

#endif /* MCP3465R_CAL_H */
