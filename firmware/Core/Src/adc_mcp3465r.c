#include "adc_mcp3465r.h"

/* Build command byte: [DevAddr(2) | RegAddr(4) | CmdType(2)] */
static uint8_t MCP3465R_BuildCmd(uint8_t reg, uint8_t cmd_type)
{
    return (uint8_t)((MCP3465R_DEVICE_ADDR << 6) | (reg << 2) | cmd_type);
}

HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hadc, uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t cmd = MCP3465R_BuildCmd(reg, MCP3465R_CMD_INCREMENTAL_WRITE);

    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef status = HAL_SPI_Transmit(hadc->hspi, &cmd, 1, 10);
    if (status == HAL_OK && len > 0) {
        status = HAL_SPI_Transmit(hadc->hspi, data, len, 10);
    }

    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_SET);

    return status;
}

HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *hadc, uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t cmd = MCP3465R_BuildCmd(reg, MCP3465R_CMD_STATIC_READ);

    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef status = HAL_SPI_Transmit(hadc->hspi, &cmd, 1, 10);
    if (status == HAL_OK && len > 0) {
        /* First byte after command is status byte, then data */
        uint8_t dummy_tx[4] = {0};
        uint8_t rx_buf[4] = {0};
        status = HAL_SPI_TransmitReceive(hadc->hspi, dummy_tx, rx_buf, len, 10);
        if (status == HAL_OK) {
            for (uint8_t i = 0; i < len; i++) {
                data[i] = rx_buf[i];
            }
        }
    }

    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_SET);

    return status;
}

HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hadc)
{
    HAL_StatusTypeDef status;
    uint8_t reg_val;

    /* Deselect */
    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_SET);
    HAL_Delay(10);

    /* CONFIG0: External VREF, internal clock, no current source, one-shot mode initially */
    reg_val = MCP3465R_CFG0_VREF_SEL_EXT
            | MCP3465R_CFG0_CLK_SEL_INT
            | MCP3465R_CFG0_CS_SEL_NONE
            | MCP3465R_CFG0_ADC_MODE_STBY;
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG0, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* CONFIG1: Prescaler /1, OSR = 1024 (good balance of speed vs resolution) */
    reg_val = MCP3465R_CFG1_PRE_1 | MCP3465R_CFG1_OSR_1024;
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG1, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* CONFIG2: Boost 1x, Gain 1x, Auto-zero MUX enabled */
    reg_val = MCP3465R_CFG2_BOOST_1X | MCP3465R_CFG2_GAIN_1X | MCP3465R_CFG2_AZ_MUX_EN;
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG2, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* CONFIG3: One-shot conversion, default data format (24-bit), no CRC */
    reg_val = MCP3465R_CFG3_CONV_MODE_ONESHOT | MCP3465R_CFG3_DATA_FORMAT_DEF | MCP3465R_CFG3_CRC_NONE;
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG3, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* IRQ: Default settings, data ready flag enabled */
    reg_val = 0x73;  /* Default: EN_STP=1, EN_FASTCMD=1, IRQ mode push-pull */
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_IRQ, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* MUX: Default to CH0 vs AGND */
    reg_val = MCP3465R_MUX_CH(MCP3465R_MUX_VIN0, MCP3465R_MUX_AGND);
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_MUX, &reg_val, 1);

    return status;
}

HAL_StatusTypeDef MCP3465R_SetChannel(MCP3465R_Handle_t *hadc, uint8_t vin_pos)
{
    uint8_t mux_val = MCP3465R_MUX_CH(vin_pos, MCP3465R_MUX_AGND);
    return MCP3465R_WriteReg(hadc, MCP3465R_REG_MUX, &mux_val, 1);
}

HAL_StatusTypeDef MCP3465R_ReadConversion(MCP3465R_Handle_t *hadc, int32_t *raw_value)
{
    HAL_StatusTypeDef status;
    uint8_t cfg0;

    /* Start one-shot conversion: write CONFIG0 with ADC_MODE = one-shot */
    cfg0 = MCP3465R_CFG0_VREF_SEL_EXT
         | MCP3465R_CFG0_CLK_SEL_INT
         | MCP3465R_CFG0_CS_SEL_NONE
         | MCP3465R_CFG0_ADC_MODE_ONESHOT;
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG0, &cfg0, 1);
    if (status != HAL_OK) return status;

    /* Wait for conversion to complete.
     * At OSR=1024 with internal clock (~4.9152 MHz DMCLK),
     * conversion time ~ 1024 / (DMCLK/4) ~ 1ms.
     * We poll the IRQ register for data ready.
     */
    uint8_t irq_reg;
    uint32_t timeout = 50;  /* Max 50ms wait */
    uint32_t start = HAL_GetTick();

    do {
        status = MCP3465R_ReadReg(hadc, MCP3465R_REG_IRQ, &irq_reg, 1);
        if (status != HAL_OK) return status;

        /* Bit 6 (DR_STATUS): 0 = data ready, 1 = not ready */
        if ((irq_reg & 0x40) == 0) break;

    } while ((HAL_GetTick() - start) < timeout);

    if ((irq_reg & 0x40) != 0) {
        return HAL_TIMEOUT;
    }

    /* Read ADCDATA register (3 bytes in default 24-bit format) */
    uint8_t adc_data[3] = {0};
    status = MCP3465R_ReadReg(hadc, MCP3465R_REG_ADCDATA, adc_data, 3);
    if (status != HAL_OK) return status;

    /* Assemble 24-bit signed value */
    int32_t val = ((int32_t)adc_data[0] << 16) | ((int32_t)adc_data[1] << 8) | adc_data[2];

    /* Sign extend from 24-bit to 32-bit */
    if (val & 0x800000) {
        val |= 0xFF000000;
    }

    *raw_value = val;
    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_ReadChannelVoltage(MCP3465R_Handle_t *hadc, uint8_t ch, float *voltage)
{
    HAL_StatusTypeDef status;
    int32_t raw;

    /* Set MUX to the desired channel */
    status = MCP3465R_SetChannel(hadc, ch);
    if (status != HAL_OK) return status;

    /* Small delay for MUX settling */
    HAL_Delay(1);

    /* Perform conversion */
    status = MCP3465R_ReadConversion(hadc, &raw);
    if (status != HAL_OK) return status;

    /*
     * Convert raw ADC value to voltage:
     * - 24-bit signed value, but for single-ended (positive only), range is 0 to 0x7FFFFF
     * - V_adc = (raw / 2^23) * VREF  (for gain=1)
     * - Actual voltage at DAC output = V_adc / VOLTAGE_DIVIDER_RATIO
     *   (because we have a resistor divider scaling 0-5V to 0-3.3V)
     */
    float v_adc = ((float)raw / (float)(1 << 23)) * MCP3465R_VREF;

    /* Compensate for voltage divider: V_actual = V_adc * (5.0/3.3) */
    *voltage = v_adc / MCP3465R_VOLTAGE_DIVIDER_RATIO;

    /* Clamp to valid range */
    if (*voltage < 0.0f) *voltage = 0.0f;
    if (*voltage > 5.5f) *voltage = 5.5f;

    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_ReadChannelCurrent(MCP3465R_Handle_t *hadc, uint8_t ch,
                                               float *current_mA, float shunt_ohm)
{
    HAL_StatusTypeDef status;
    int32_t raw;

    /* Current sense channels are on CH4~CH7 (mapped from load CH0~CH3) */
    uint8_t adc_ch = ch + MCP3465R_NUM_VOLT_CH;
    if (adc_ch > 7) return HAL_ERROR;

    status = MCP3465R_SetChannel(hadc, adc_ch);
    if (status != HAL_OK) return status;

    HAL_Delay(1);

    status = MCP3465R_ReadConversion(hadc, &raw);
    if (status != HAL_OK) return status;

    /* Voltage across shunt (measured by ADC after current sense amplifier)
     * Assuming INA180 with gain = 50V/V:
     *   V_adc = I_load * R_shunt * Gain_INA
     *   I_load = V_adc / (R_shunt * Gain_INA)
     *
     * V_adc = (raw / 2^23) * VREF
     */
    float v_adc = ((float)raw / (float)(1 << 23)) * MCP3465R_VREF;

    float gain_ina = 50.0f;  /* INA180A3 gain = 50 V/V */
    float current_A = v_adc / (shunt_ohm * gain_ina);

    *current_mA = current_A * 1000.0f;

    if (*current_mA < 0.0f) *current_mA = 0.0f;

    return HAL_OK;
}
