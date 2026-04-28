/**
 * DAC80501 DAC driver implementation.
 * See dac80501.h for full documentation.
 */
#include "dac80501.h"

/* ── CS helpers ─────────────────────────────────────────────────────────── */
static inline void cs_low(void)
{
    HAL_GPIO_WritePin(DAC80501_CS_GPIO, DAC80501_CS_PIN, GPIO_PIN_RESET);
}

static inline void cs_high(void)
{
    HAL_GPIO_WritePin(DAC80501_CS_GPIO, DAC80501_CS_PIN, GPIO_PIN_SET);
}

/* ── Low-level register access ──────────────────────────────────────────── */

/**
 * DAC80501 SPI frame: 24 bits total.
 *   Byte 0: [7:4]=reg_addr [3:0]=0x0 (reserved)
 *   Byte 1: data[15:8]
 *   Byte 2: data[7:0]
 */
static HAL_StatusTypeDef write_reg(uint8_t reg, uint16_t data)
{
    uint8_t tx[3] = {
        (uint8_t)((reg & 0x0FU) << 4),  /* command byte: addr in bits[7:4] */
        (uint8_t)(data >> 8),
        (uint8_t)(data & 0xFF)
    };
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(&DAC80501_SPI, tx, 3,
                                             DAC80501_SPI_TIMEOUT_MS);
    cs_high();
    return st;
}

static HAL_StatusTypeDef read_reg(uint8_t reg, uint16_t *data)
{
    /* Read command: set bit 7 of first byte */
    uint8_t tx[3] = { (uint8_t)(0x80U | ((reg & 0x0FU) << 4)), 0xFF, 0xFF };
    uint8_t rx[3] = {0};
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&DAC80501_SPI, tx, rx, 3,
                                                    DAC80501_SPI_TIMEOUT_MS);
    cs_high();
    if (st == HAL_OK)
        *data = ((uint16_t)rx[1] << 8) | rx[2];
    return st;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

HAL_StatusTypeDef DAC80501_Reset(void)
{
    /* Set RESET bit (bit 1) in TRIGGER register */
    HAL_StatusTypeDef st = write_reg(DAC80501_REG_TRIGGER, 0x000AU);
    HAL_Delay(1);
    return st;
}

HAL_StatusTypeDef DAC80501_Init(void)
{
    cs_high();
    HAL_Delay(2); /* power-on settling */

    HAL_StatusTypeDef st;

    /* Power up DAC and internal reference */
    st = write_reg(DAC80501_REG_CONFIG,
                   DAC80501_CFG_DAC_ON | DAC80501_CFG_REF_ON);
    if (st != HAL_OK) return st;

    /* Output range: VREF=2.5V, REF_DIV=0 (no divide), BUFF_GAIN=×2 → 0~5V */
    st = write_reg(DAC80501_REG_GAIN,
                   DAC80501_GAIN_REF_DIV0 | DAC80501_GAIN_BUF_2X);
    if (st != HAL_OK) return st;

    /* Set DAC output to 0V initially */
    st = write_reg(DAC80501_REG_DAC, 0x0000U);
    if (st != HAL_OK) return st;

    HAL_Delay(5);
    return HAL_OK;
}

HAL_StatusTypeDef DAC80501_SetCode(uint16_t code)
{
    return write_reg(DAC80501_REG_DAC, code);
}

/**
 * Set DAC output to the requested voltage.
 *
 * With BUFF_GAIN=×2 and VREF=2.5V:
 *   V_out = code / 65535 × 5.0V
 *   code  = V_out / 5.0V × 65535
 *
 * @param voltage_v  desired output, 0.0 to 5.0 V
 */
HAL_StatusTypeDef DAC80501_SetVoltage(float voltage_v)
{
    if (voltage_v < 0.0f) voltage_v = 0.0f;
    if (voltage_v > DAC80501_VOUT_MAX) voltage_v = DAC80501_VOUT_MAX;

    uint16_t code = (uint16_t)(voltage_v / DAC80501_VOUT_MAX
                                * (float)DAC80501_CODE_MAX + 0.5f);
    return DAC80501_SetCode(code);
}
