#ifndef MCP3465R_H
#define MCP3465R_H

#include <stdint.h>
#include <stdbool.h>
#include "spi_hal.h"

/* -----------------------------------------------------------------------
 * MCP3465R – 16-bit Delta-Sigma ADC
 * Device address (A1:A0 = 01 → address 0x01)
 * ----------------------------------------------------------------------- */

#define MCP3465R_ADDR           0x01U   /* A1=0, A0=1 (default) */

/* Command byte: [ADDR1:ADDR0 | REG3:REG0 | CMD1:CMD0] */
#define MCP3465R_CMD(addr, reg, cmd)  (((addr) << 6) | ((reg) << 2) | (cmd))

/* CMD bits */
#define MCP3465R_CMD_CONVERSION 0x00U  /* Start/Restart ADC Conversion */
#define MCP3465R_CMD_STANDBY    0x04U  /* Standby mode */
#define MCP3465R_CMD_SHUTDOWN   0x08U  /* Shutdown */
#define MCP3465R_CMD_FULL_RESET 0x0CU  /* Full Reset */
#define MCP3465R_CMD_STAT_READ  0x01U  /* Static Read */
#define MCP3465R_CMD_INC_WRITE  0x02U  /* Incremental Write */
#define MCP3465R_CMD_INC_READ   0x03U  /* Incremental Read */

/* Register addresses */
#define MCP3465R_REG_ADCDATA    0x00U
#define MCP3465R_REG_CONFIG0    0x01U
#define MCP3465R_REG_CONFIG1    0x02U
#define MCP3465R_REG_CONFIG2    0x03U
#define MCP3465R_REG_CONFIG3    0x04U
#define MCP3465R_REG_IRQ        0x05U
#define MCP3465R_REG_MUX        0x06U
#define MCP3465R_REG_SCAN       0x07U
#define MCP3465R_REG_TIMER      0x08U
#define MCP3465R_REG_OFFSETCAL  0x09U
#define MCP3465R_REG_GAINCAL    0x0AU
#define MCP3465R_REG_LOCK       0x0DU
#define MCP3465R_REG_CRCCFG     0x0FU

/* CONFIG0 bits */
#define MCP3465R_CONFIG0_VREF_INT       (0x00U << 6)  /* Internal VREF (2.4V) */
#define MCP3465R_CONFIG0_VREF_EXT       (0x02U << 6)  /* External VREF */
#define MCP3465R_CONFIG0_CLK_INT        (0x02U << 4)  /* Internal clock, no output */
#define MCP3465R_CONFIG0_CS_NONE        (0x00U << 2)
#define MCP3465R_CONFIG0_MODE_ONESHOT   (0x02U << 0)
#define MCP3465R_CONFIG0_MODE_CONT      (0x03U << 0)

/* CONFIG1 bits: OSR (Over-sampling ratio) */
#define MCP3465R_CONFIG1_OSR_256        (0x03U << 2)  /* 256× OSR */
#define MCP3465R_CONFIG1_OSR_1024       (0x05U << 2)
#define MCP3465R_CONFIG1_OSR_4096       (0x07U << 2)
#define MCP3465R_CONFIG1_AMCLK_DIV1     (0x00U << 0)

/* CONFIG2 bits: PGA */
#define MCP3465R_CONFIG2_BOOST_X1       (0x02U << 6)
#define MCP3465R_CONFIG2_GAIN_X1        (0x03U << 3)
#define MCP3465R_CONFIG2_AZ_MUX_EN      (0x01U << 2)

/* CONFIG3 bits */
#define MCP3465R_CONFIG3_CONV_MODE_CONT (0x03U << 6)
#define MCP3465R_CONFIG3_DATA_FORMAT_32 (0x03U << 4)  /* 32-bit sign-extended */
#define MCP3465R_CONFIG3_CRC_FORMAT_32  (0x01U << 3)
#define MCP3465R_CONFIG3_EN_CRCCOM      (0x01U << 2)
#define MCP3465R_CONFIG3_EN_OFFCAL      (0x01U << 1)  /* Enable offset calibration reg */
#define MCP3465R_CONFIG3_EN_GAINCAL     (0x01U << 0)  /* Enable gain calibration reg */

/* MUX channel selection (CONFIG3 format) */
#define MCP3465R_MUX_VIN_POS(ch)    ((ch) << 4)
#define MCP3465R_MUX_VIN_NEG_AGND  0x08U

/* VREF for ADC (external reference, e.g. 5.0V) */
#define MCP3465R_VREF_MV            5000U  /* 5.000 V in mV */

/* Full-scale for 16-bit signed ADC */
#define MCP3465R_FULL_SCALE         32768U

typedef struct {
    int32_t  offset_cal;   /* offset calibration register value */
    uint32_t gain_cal;     /* gain calibration register value */
} MCP3465R_CalData;

typedef struct {
    float    offset_mv;    /* measured offset error in mV */
    float    gain_factor;  /* measured gain factor */
    bool     calibrated;
    MCP3465R_CalData hw_cal;
} MCP3465R_Handle;

/* Public API */
bool    MCP3465R_Init(MCP3465R_Handle *h);
bool    MCP3465R_SetChannel(MCP3465R_Handle *h, uint8_t pos_ch, uint8_t neg_ch);
bool    MCP3465R_ReadRaw(MCP3465R_Handle *h, int32_t *raw_out);
float   MCP3465R_RawToMillivolts(const MCP3465R_Handle *h, int32_t raw);
bool    MCP3465R_WriteOffsetCal(MCP3465R_Handle *h, int32_t offset_val);
bool    MCP3465R_WriteGainCal(MCP3465R_Handle *h, uint32_t gain_val);
int32_t MCP3465R_CalcOffsetRegVal(float offset_mv);
uint32_t MCP3465R_CalcGainRegVal(float measured_full_mv, float expected_full_mv);

#endif /* MCP3465R_H */
