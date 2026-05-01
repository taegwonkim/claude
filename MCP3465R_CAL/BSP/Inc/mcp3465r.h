#ifndef MCP3465R_H
#define MCP3465R_H

/* MCP3465R - 24-bit Delta-Sigma ADC
 *
 * SPI command byte: [DEV_ADDR 1:0][REG_ADDR 5:0][CMD_TYPE 1:0]
 *   DEV_ADDR  : 0x01 (hardcoded on MCP3465R)
 *   CMD_TYPE  : 0x0=FastCmd, 0x1=StaticRead, 0x2=IncWrite, 0x3=IncRead
 *
 * OFFSETCAL [23:0] : two's-complement offset (subtracted from raw output)
 * GAINCAL   [23:0] : gain factor; default 0x800000 = 1.0x
 *                    output = raw * GAINCAL / 0x800000
 */

#include <stdint.h>
#include <stddef.h>

/* Device address (hardwired on MCP3465R) */
#define MCP3465R_DEV_ADDR       0x01U

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

/* Command type */
#define MCP3465R_CMD_FASTCMD    0x00U
#define MCP3465R_CMD_SREAD      0x01U
#define MCP3465R_CMD_IWRITE     0x02U
#define MCP3465R_CMD_IREAD      0x03U

/* Fast commands */
#define MCP3465R_FASTCMD_START  0x28U   /* Start/restart conversion */
#define MCP3465R_FASTCMD_STBY   0x2CU   /* Standby */
#define MCP3465R_FASTCMD_RESET  0x38U   /* Full reset */

/* CONFIG0 */
#define MCP3465R_CFG0_VREF_INT  (0x1U << 7)    /* Use internal Vref */
#define MCP3465R_CFG0_CLK_INT   (0x2U << 4)    /* Internal clock */
#define MCP3465R_CFG0_CS_NONE   (0x0U << 2)    /* No current source */
#define MCP3465R_CFG0_MODE_CONV (0x3U << 0)    /* Continuous conversion */
#define MCP3465R_CFG0_MODE_STBY (0x2U << 0)    /* Standby */

/* CONFIG2: PGA gain */
#define MCP3465R_CFG2_BOOST_X2  (0x3U << 6)    /* ADC bias x2 */
#define MCP3465R_CFG2_GAIN_1_3  (0x0U << 3)
#define MCP3465R_CFG2_GAIN_1    (0x1U << 3)
#define MCP3465R_CFG2_GAIN_2    (0x2U << 3)
#define MCP3465R_CFG2_GAIN_4    (0x3U << 3)
#define MCP3465R_CFG2_GAIN_8    (0x4U << 3)
#define MCP3465R_CFG2_GAIN_16   (0x5U << 3)
#define MCP3465R_CFG2_GAIN_32   (0x6U << 3)
#define MCP3465R_CFG2_GAIN_64   (0x7U << 3)
#define MCP3465R_CFG2_AZ_MUX    (0x1U << 2)    /* Auto-zero MUX */

/* CONFIG3: data format */
#define MCP3465R_CFG3_FMT_32B   (0x3U << 6)    /* 32-bit with channel ID */
#define MCP3465R_CFG3_FMT_32B_S (0x2U << 6)    /* 32-bit sign-extended */
#define MCP3465R_CFG3_CONV_CONT (0x2U << 4)    /* Continuous conversion */
#define MCP3465R_CFG3_CRCCOM    (0x1U << 0)    /* CRC on communication */

/* MUX channel selection (upper nibble = VIN+, lower = VIN-) */
#define MCP3465R_MUX_CH(pos, neg)  (((pos) << 4) | (neg))
#define MCP3465R_MUX_CH0P   0x0U
#define MCP3465R_MUX_CH1P   0x1U
#define MCP3465R_MUX_CH2P   0x2U
#define MCP3465R_MUX_CH3P   0x3U
#define MCP3465R_MUX_CH4P   0x4U
#define MCP3465R_MUX_CH5P   0x5U
#define MCP3465R_MUX_CH6P   0x6U
#define MCP3465R_MUX_CH7P   0x7U
#define MCP3465R_MUX_AGND   0x8U   /* AGND (for offset cal) */
#define MCP3465R_MUX_AVDD   0x9U
#define MCP3465R_MUX_REFIN_P 0xBU  /* REFIN+ (for gain cal) */
#define MCP3465R_MUX_REFIN_N 0xCU  /* REFIN- */

#define MCP3465R_GAINCAL_UNITY  0x800000UL  /* 1.0 gain */
#define MCP3465R_ADC_MAX_CODE   0x7FFFFFL   /* Full-scale positive, 24-bit */

typedef enum {
    MCP3465R_OK         =  0,
    MCP3465R_ERR_SPI    = -1,
    MCP3465R_ERR_PARAM  = -2,
    MCP3465R_ERR_TIMEOUT= -3,
} mcp3465r_status_t;

/* ---------- SPI HAL (implement in your BSP) ---------- */

/* Assert/deassert chip-select */
void mcp3465r_cs_low(void);
void mcp3465r_cs_high(void);

/* Blocking SPI transfer: send tx_len bytes, receive rx_len bytes.
 * tx and rx may overlap; caller manages buffer size.
 * Returns 0 on success. */
int mcp3465r_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len);

/* Microsecond busy-wait */
void mcp3465r_delay_us(uint32_t us);

/* ---------- Low-level register access ---------- */

static inline uint8_t mcp3465r_cmd(uint8_t reg, uint8_t type)
{
    return (uint8_t)((MCP3465R_DEV_ADDR << 6) | (reg << 2) | type);
}

mcp3465r_status_t mcp3465r_write_reg(uint8_t reg, const uint8_t *data, size_t len);
mcp3465r_status_t mcp3465r_read_reg(uint8_t reg, uint8_t *data, size_t len);
mcp3465r_status_t mcp3465r_fast_cmd(uint8_t cmd);
mcp3465r_status_t mcp3465r_read_adc(int32_t *out);

/* Write 24-bit calibration register (OFFSETCAL or GAINCAL) */
mcp3465r_status_t mcp3465r_write_cal_reg(uint8_t reg, int32_t value);
mcp3465r_status_t mcp3465r_read_cal_reg(uint8_t reg, int32_t *value);

/* Apply stored offset/gain to the hardware registers */
mcp3465r_status_t mcp3465r_apply_offset(int32_t offset);
mcp3465r_status_t mcp3465r_apply_gain(uint32_t gain);

#endif /* MCP3465R_H */
