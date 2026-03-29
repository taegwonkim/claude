/**
 ******************************************************************************
 * @file    mcp3465r.c
 * @brief   MCP3465R 4-Channel 16-bit Delta-Sigma ADC Driver Implementation
 ******************************************************************************
 */
#include "mcp3465r.h"

/* ── Private Helpers ──────────────────────────────────────────────────── */

static inline void CS_Low(MCP3465R_Handle_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static inline void CS_High(MCP3465R_Handle_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

static uint8_t BuildCmd(uint8_t reg_addr, uint8_t cmd_type)
{
    return MCP3465R_DEV_ADDR | reg_addr | cmd_type;
}

/* ── Register Access ──────────────────────────────────────────────────── */

HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *dev,
                                    uint8_t reg_addr, uint8_t data)
{
    uint8_t tx[2];
    tx[0] = BuildCmd(reg_addr, MCP3465R_CMD_INC_WRITE);
    tx[1] = data;

    CS_Low(dev);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(dev->hspi, tx, 2, 100);
    CS_High(dev);
    return status;
}

HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *dev,
                                   uint8_t reg_addr, uint8_t *data)
{
    uint8_t tx[2] = {0};
    uint8_t rx[2] = {0};
    tx[0] = BuildCmd(reg_addr, MCP3465R_CMD_STATIC_READ);

    CS_Low(dev);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 2, 100);
    CS_High(dev);

    *data = rx[1];  /* First byte is status, second is data */
    return status;
}

HAL_StatusTypeDef MCP3465R_FastCommand(MCP3465R_Handle_t *dev, uint8_t cmd)
{
    uint8_t tx = MCP3465R_DEV_ADDR | cmd;
    CS_Low(dev);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(dev->hspi, &tx, 1, 100);
    CS_High(dev);
    return status;
}

/* ── ADC Data Read ────────────────────────────────────────────────────── */

HAL_StatusTypeDef MCP3465R_ReadAdcData(MCP3465R_Handle_t *dev, int32_t *raw_data)
{
    uint8_t tx[5] = {0};
    uint8_t rx[5] = {0};

    /* Command to read ADCDATA register (32-bit signed format = 4 data bytes) */
    tx[0] = BuildCmd(MCP3465R_REG_ADCDATA, MCP3465R_CMD_STATIC_READ);

    CS_Low(dev);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 5, 100);
    CS_High(dev);

    if (status == HAL_OK) {
        /* rx[0] = status byte, rx[1..4] = 32-bit ADC data (MSB first) */
        *raw_data = ((int32_t)rx[1] << 24) |
                    ((int32_t)rx[2] << 16) |
                    ((int32_t)rx[3] << 8)  |
                    ((int32_t)rx[4]);
    }

    return status;
}

/* ── Initialization ───────────────────────────────────────────────────── */

HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *dev)
{
    HAL_StatusTypeDef status;

    /* Ensure CS is high */
    CS_High(dev);
    HAL_Delay(1);

    /* Reset the device */
    status = MCP3465R_FastCommand(dev, MCP3465R_FAST_CMD_RESET);
    if (status != HAL_OK) return status;
    HAL_Delay(5);

    /* CONFIG0: External VREF, internal clock, conversion mode */
    status = MCP3465R_WriteReg(dev, MCP3465R_REG_CONFIG0,
        MCP3465R_CFG0_VREF_SEL_EXT |
        MCP3465R_CFG0_CLK_SEL_INT  |
        MCP3465R_CFG0_CS_SEL_NONE  |
        MCP3465R_CFG0_ADC_MODE_STANDBY);
    if (status != HAL_OK) return status;

    /* CONFIG1: OSR = 256 (balanced speed vs resolution), prescale /1 */
    status = MCP3465R_WriteReg(dev, MCP3465R_REG_CONFIG1,
        MCP3465R_CFG1_OSR_256 |
        MCP3465R_CFG1_PRESCALE_1);
    if (status != HAL_OK) return status;

    /* CONFIG2: 1x boost, 1x gain, auto-zero MUX enabled */
    status = MCP3465R_WriteReg(dev, MCP3465R_REG_CONFIG2,
        MCP3465R_CFG2_BOOST_1X  |
        MCP3465R_CFG2_GAIN_1X   |
        MCP3465R_CFG2_AZ_MUX_EN);
    if (status != HAL_OK) return status;

    /* CONFIG3: One-shot conversion, 32-bit signed data format */
    status = MCP3465R_WriteReg(dev, MCP3465R_REG_CONFIG3,
        MCP3465R_CFG3_CONV_MODE_ONE_SHOT    |
        MCP3465R_CFG3_DATA_FORMAT_32BIT_SGN |
        MCP3465R_CFG3_CRC_DIS);
    if (status != HAL_OK) return status;

    /* MUX: Default to CH0 vs AGND */
    status = MCP3465R_WriteReg(dev, MCP3465R_REG_MUX,
        MCP3465R_MUX_VIN_POS(0) | MCP3465R_MUX_VIN_NEG_AGND);
    if (status != HAL_OK) return status;

    return HAL_OK;
}

/* ── Channel Reading ──────────────────────────────────────────────────── */

HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_Handle_t *dev,
                                       uint8_t channel,
                                       uint16_t *voltage_mv)
{
    HAL_StatusTypeDef status;
    int32_t raw_data = 0;

    if (channel > 3) return HAL_ERROR;

    /* Set MUX to desired channel vs AGND */
    status = MCP3465R_WriteReg(dev, MCP3465R_REG_MUX,
        MCP3465R_MUX_VIN_POS(channel) | MCP3465R_MUX_VIN_NEG_AGND);
    if (status != HAL_OK) return status;

    /* Trigger one-shot conversion */
    status = MCP3465R_FastCommand(dev, MCP3465R_FAST_CMD_CONVERSION);
    if (status != HAL_OK) return status;

    /* Wait for conversion to complete
     * At OSR=256, internal clock ~6.144MHz → conversion ≈ 256/6144000 ≈ 42μs
     * Add margin for settling */
    HAL_Delay(1);

    /* Read ADC data */
    status = MCP3465R_ReadAdcData(dev, &raw_data);
    if (status != HAL_OK) return status;

    /* Convert to millivolts:
     * In 32-bit signed format with DATA_FORMAT = 11:
     *   Bit 24 = sign (repeated in bits 31:25)
     *   Bits 23:8 = 16-bit ADC data
     *   Bits 7:0 = zero
     * Extract 16-bit value and convert */
    int16_t adc_16bit = (int16_t)((raw_data >> 8) & 0xFFFF);

    /* Clamp negative values to zero (single-ended measurement) */
    if (adc_16bit < 0) adc_16bit = 0;

    /* Convert: voltage_mv = (adc_value / 2^15) * VREF_mV
     * Using 2^15 because MSB is sign bit in signed 16-bit */
    uint32_t mv = ((uint32_t)adc_16bit * (uint32_t)dev->vref_mv) / 32768U;
    if (mv > dev->vref_mv) mv = dev->vref_mv;

    *voltage_mv = (uint16_t)mv;
    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_Handle_t *dev,
                                           uint16_t voltages_mv[4])
{
    for (uint8_t ch = 0; ch < 4; ch++) {
        HAL_StatusTypeDef status = MCP3465R_ReadChannel(dev, ch, &voltages_mv[ch]);
        if (status != HAL_OK) return status;
    }
    return HAL_OK;
}
