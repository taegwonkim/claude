/*
 * mcp3465r.c – STM32L5 HAL driver for MCP3465R (24-bit delta-sigma ADC)
 *
 * Per-channel configuration strategy:
 *   MCP3465R has no hardware per-channel registers; OSR, Gain, and MUX are
 *   global. MCP3465R_ReadChannel() writes CONFIG1 (OSR), CONFIG2 (Gain), and
 *   MUX before each one-shot conversion to achieve per-channel behaviour.
 */

#include "mcp3465r.h"

/* ---- Register addresses ------------------------------------------------- */
#define REG_ADCDATA   0x00u
#define REG_CONFIG0   0x01u
#define REG_CONFIG1   0x02u
#define REG_CONFIG2   0x03u
#define REG_CONFIG3   0x04u
#define REG_IRQ       0x05u
#define REG_MUX       0x06u

/* ---- Command byte builder ------------------------------------------------
 * Format: [A1:A0=01][REG[3:0]][CMD_TYPE[1:0]]
 * A1:A0 = 0b01 is the default device address for MCP3465R
 * ------------------------------------------------------------------------- */
#define DEV_ADDR      0x01u
#define CMD_IWRITE    0x01u   /* Incremental write */
#define CMD_SREAD     0x03u   /* Static read       */
#define CMD_BYTE(reg, type)  ((uint8_t)((DEV_ADDR << 6) | ((reg) << 2) | (type)))

/* ---- Fast commands ------------------------------------------------------- */
#define FAST_START    0x68u   /* Start/Restart conversion */
#define FAST_RESET    0x78u   /* Full device reset        */

/* ---- Default register values -------------------------------------------- */
/*
 * CONFIG0: VREF_SEL=11 (internal 2.4 V, no output pin),
 *          CLK_SEL=00 (external MCLK), CS_SEL=00, ADC_MODE=10 (standby)
 */
#define CONFIG0_VAL   0xC2u

/*
 * CONFIG2: BOOST=10 (×1 operating current) – GAIN bits are set per-channel
 */
#define CONFIG2_BOOST_MASK  0x80u  /* bits [7:6] = 0b10 */

/*
 * CONFIG3: CONV_MODE=10 (one-shot → standby after conversion),
 *          DATA_FORMAT=01 (32-bit sign-extended, no channel ID),
 *          CRC_FORMAT=00, EN_STOP=0, EN_FASTCMD=1
 */
#define CONFIG3_VAL   0x91u

/* ---- Data-ready timeout -------------------------------------------------- */
#define DR_TIMEOUT_MS  500u

/* ---- IRQ register DR bit -------------------------------------------------
 * Bit 0 of the STATUS byte (= IRQ register echoed on MISO at CS assertion):
 *   0 = new conversion data is available
 *   1 = no new data
 * ------------------------------------------------------------------------- */
#define IRQ_DR_BIT    0x01u

/* ========================================================================= */
/* Static helpers                                                             */
/* ========================================================================= */

static void cs_assert(MCP3465R_Handle *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static void cs_deassert(MCP3465R_Handle *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

/* Single-register write (incremental write, 1 data byte) */
static HAL_StatusTypeDef write_reg(MCP3465R_Handle *dev, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { CMD_BYTE(reg, CMD_IWRITE), val };
    cs_assert(dev);
    HAL_StatusTypeDef ret = HAL_SPI_Transmit(dev->hspi, tx, 2, 10);
    cs_deassert(dev);
    return ret;
}

/* Send a 1-byte fast command */
static HAL_StatusTypeDef fast_cmd(MCP3465R_Handle *dev, uint8_t cmd)
{
    cs_assert(dev);
    HAL_StatusTypeDef ret = HAL_SPI_Transmit(dev->hspi, &cmd, 1, 10);
    cs_deassert(dev);
    return ret;
}

/*
 * Poll the STATUS byte until DR (bit 0) clears, indicating a completed
 * conversion.  The STATUS byte is the first MISO byte of any SPI transaction
 * (MCP3465R echoes the IRQ register at the start of every CS assertion).
 */
static HAL_StatusTypeDef wait_data_ready(MCP3465R_Handle *dev)
{
    uint8_t tx[2] = { CMD_BYTE(REG_IRQ, CMD_SREAD), 0x00u };
    uint8_t rx[2];
    uint32_t t0 = HAL_GetTick();

    do {
        HAL_StatusTypeDef ret = HAL_OK;
        cs_assert(dev);
        ret = HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 2, 10);
        cs_deassert(dev);
        if (ret != HAL_OK)
            return ret;
        /* rx[0] = STATUS byte sent during CMD phase (IRQ register content) */
        if ((rx[0] & IRQ_DR_BIT) == 0u)
            return HAL_OK;
    } while ((HAL_GetTick() - t0) < DR_TIMEOUT_MS);

    return HAL_TIMEOUT;
}

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

/**
 * @brief  Reset the MCP3465R and apply base (channel-independent) settings.
 *         Call once after power-up before any MCP3465R_ReadChannel() calls.
 */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle *dev)
{
    HAL_StatusTypeDef ret;

    cs_deassert(dev);
    HAL_Delay(1);

    ret = fast_cmd(dev, FAST_RESET);
    if (ret != HAL_OK) return ret;
    HAL_Delay(1);   /* tRESET: 1 ms is conservative */

    ret = write_reg(dev, REG_CONFIG0, CONFIG0_VAL);
    if (ret != HAL_OK) return ret;

    /* CONFIG3: one-shot standby, 32-bit sign-extended output, fast cmds on */
    ret = write_reg(dev, REG_CONFIG3, CONFIG3_VAL);
    if (ret != HAL_OK) return ret;

    return HAL_OK;
}

/**
 * @brief  Reconfigure the ADC for the given channel settings, trigger a
 *         one-shot conversion, wait for completion, and return the result.
 *
 * @param  dev     Driver handle (SPI + CS pin).
 * @param  cfg     Per-channel settings: MUX inputs, OSR, and PGA gain.
 * @param  result  Output: 24-bit signed ADC value in int32_t.
 *                 Voltage = ((float)result / 8388608.0f) * VREF / gain_factor
 */
HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_Handle *dev,
                                        const MCP3465R_ChConfig *cfg,
                                        int32_t *result)
{
    HAL_StatusTypeDef ret;

    /* --- CONFIG1: set OSR for this channel (PRE=00, reserved=00) ---------- */
    uint8_t config1 = (uint8_t)((uint8_t)cfg->osr << 2);
    ret = write_reg(dev, REG_CONFIG1, config1);
    if (ret != HAL_OK) return ret;

    /* --- CONFIG2: set Gain for this channel (BOOST=×1 fixed) -------------- */
    uint8_t config2 = (uint8_t)(CONFIG2_BOOST_MASK | ((uint8_t)cfg->gain << 3));
    ret = write_reg(dev, REG_CONFIG2, config2);
    if (ret != HAL_OK) return ret;

    /* --- MUX: select positive and negative inputs for this channel --------- */
    uint8_t mux = (uint8_t)(((uint8_t)cfg->vin_pos << 4) | (uint8_t)cfg->vin_neg);
    ret = write_reg(dev, REG_MUX, mux);
    if (ret != HAL_OK) return ret;

    /* --- Trigger one-shot conversion --------------------------------------- */
    ret = fast_cmd(dev, FAST_START);
    if (ret != HAL_OK) return ret;

    /* --- Wait for data ready (DR bit = 0) ---------------------------------- */
    ret = wait_data_ready(dev);
    if (ret != HAL_OK) return ret;

    /* --- Read 4-byte ADC result (DATA_FORMAT=01: 32-bit sign-extended) ----- */
    uint8_t tx[5] = { CMD_BYTE(REG_ADCDATA, CMD_SREAD), 0, 0, 0, 0 };
    uint8_t rx[5] = { 0 };
    cs_assert(dev);
    ret = HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 5, 10);
    cs_deassert(dev);
    if (ret != HAL_OK) return ret;

    /*
     * rx[0] = STATUS byte (discarded here)
     * rx[1..4] = ADC data, MSB first
     * DATA_FORMAT=01: sign bit of 24-bit result is extended to fill bits[31:24].
     * Casting the constructed uint32_t to int32_t gives the correct signed value.
     */
    uint32_t raw = ((uint32_t)rx[1] << 24) |
                   ((uint32_t)rx[2] << 16) |
                   ((uint32_t)rx[3] <<  8) |
                    (uint32_t)rx[4];
    *result = (int32_t)raw;

    return HAL_OK;
}
