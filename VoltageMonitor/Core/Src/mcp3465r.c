/**
  ******************************************************************************
  * @file           : mcp3465r.c
  * @brief          : MCP3465R 16-bit Delta-Sigma ADC driver implementation
  *
  * Each MCP3465R has 2 input channels:
  *   CH0 = Voltage feedback measurement
  *   CH1 = Current measurement (via shunt resistor)
  *
  * SPI Mode 0,0 (CPOL=0, CPHA=0), Max 20MHz
  * Device address = 01 (default)
  *
  * Command byte format:
  *   [DEV_ADDR(2) | REG_ADDR(4) | CMD_TYPE(2)]
  ******************************************************************************
  */

#include "mcp3465r.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static MCP3465R_Device_t adc_devices[NUM_CHANNELS];
static osMutexId_t       spi1_mutex = NULL;

/* CS pin configuration table */
static const struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} adc_cs_pins[NUM_CHANNELS] = {
    { ADC_CS1_GPIO_Port, ADC_CS1_Pin },
    { ADC_CS2_GPIO_Port, ADC_CS2_Pin },
    { ADC_CS3_GPIO_Port, ADC_CS3_Pin },
    { ADC_CS4_GPIO_Port, ADC_CS4_Pin },
};

/* Private functions ---------------------------------------------------------*/
static inline void ADC_CS_Low(uint8_t idx)
{
    HAL_GPIO_WritePin(adc_devices[idx].cs_port, adc_devices[idx].cs_pin, GPIO_PIN_RESET);
}

static inline void ADC_CS_High(uint8_t idx)
{
    HAL_GPIO_WritePin(adc_devices[idx].cs_port, adc_devices[idx].cs_pin, GPIO_PIN_SET);
}

/**
  * @brief  Build command byte
  * @param  reg_addr: Register address (0x00~0x0D)
  * @param  cmd_type: Command type (Static Read, Inc Write, Inc Read)
  * @retval Command byte
  */
static uint8_t MCP3465R_BuildCmd(uint8_t reg_addr, uint8_t cmd_type)
{
    return (uint8_t)((MCP3465R_DEV_ADDR << 6) | ((reg_addr & 0x0F) << 2) | (cmd_type & 0x03));
}

/* Public functions ----------------------------------------------------------*/

void MCP3465R_Init(MCP3465R_Device_t *devs, SPI_HandleTypeDef *hspi, osMutexId_t mutex)
{
    spi1_mutex = mutex;

    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        adc_devices[i].hspi    = hspi;
        adc_devices[i].cs_port = adc_cs_pins[i].port;
        adc_devices[i].cs_pin  = adc_cs_pins[i].pin;
        adc_devices[i].raw_voltage = 0;
        adc_devices[i].raw_current = 0;

        /* Ensure CS is high (deselected) */
        ADC_CS_High(i);
    }

    /* Copy back to caller if needed */
    if (devs != NULL) {
        memcpy(devs, adc_devices, sizeof(adc_devices));
    }

    /* Delay for power-up */
    osDelay(10);

    /* Configure each ADC device */
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        MCP3465R_FastCommand(i, MCP3465R_FAST_CMD_FULL_RESET);
        osDelay(5);
        MCP3465R_Configure(i);
    }
}

HAL_StatusTypeDef MCP3465R_Configure(uint8_t dev_idx)
{
    HAL_StatusTypeDef status;
    uint8_t reg_val;

    if (dev_idx >= NUM_CHANNELS) return HAL_ERROR;

    /* CONFIG0: External Vref, Internal clock (no output), No current source, Standby */
    reg_val = MCP3465R_CFG0_VREF_SEL_EXT |
              MCP3465R_CFG0_CLK_SEL_INT  |
              MCP3465R_CFG0_CS_SEL_NONE  |
              MCP3465R_CFG0_ADC_MODE_STANDBY;
    status = MCP3465R_WriteReg(dev_idx, MCP3465R_REG_CONFIG0, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* CONFIG1: Prescaler /1, OSR = 256 (good balance of speed and resolution) */
    reg_val = MCP3465R_CFG1_PRESCALE_1 | MCP3465R_CFG1_OSR_256;
    status = MCP3465R_WriteReg(dev_idx, MCP3465R_REG_CONFIG1, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* CONFIG2: Boost 1x, Gain 1x, Auto-zero MUX enabled */
    reg_val = MCP3465R_CFG2_BOOST_1X | MCP3465R_CFG2_GAIN_1X | MCP3465R_CFG2_AZ_MUX_EN;
    status = MCP3465R_WriteReg(dev_idx, MCP3465R_REG_CONFIG2, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* CONFIG3: One-shot mode, default data format */
    reg_val = MCP3465R_CFG3_CONV_MODE_ONESHOT | MCP3465R_CFG3_DATA_FORMAT_DEF;
    status = MCP3465R_WriteReg(dev_idx, MCP3465R_REG_CONFIG3, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* IRQ: Enable data ready, IRQ mode = logic high when inactive */
    reg_val = MCP3465R_IRQ_MODE_LOGIC_HIGH | MCP3465R_IRQ_EN_FASTCMD;
    status = MCP3465R_WriteReg(dev_idx, MCP3465R_REG_IRQ, &reg_val, 1);
    if (status != HAL_OK) return status;

    /* MUX: Default to CH0 single-ended (voltage measurement) */
    reg_val = MCP3465R_MUX_CH0_SE;
    status = MCP3465R_WriteReg(dev_idx, MCP3465R_REG_MUX, &reg_val, 1);

    return status;
}

HAL_StatusTypeDef MCP3465R_ReadChannel(uint8_t dev_idx, MCP3465R_Channel_t ch, int32_t *raw_value)
{
    HAL_StatusTypeDef status;
    uint8_t mux_val;
    uint8_t config0_val;
    uint8_t adc_data[3];
    uint8_t irq_reg;

    if (dev_idx >= NUM_CHANNELS || raw_value == NULL) return HAL_ERROR;

    /* Select MUX channel */
    if (ch == MCP3465R_CH_VOLTAGE) {
        mux_val = MCP3465R_MUX_CH0_SE;
    } else {
        mux_val = MCP3465R_MUX_CH1_SE;
    }

    status = MCP3465R_WriteReg(dev_idx, MCP3465R_REG_MUX, &mux_val, 1);
    if (status != HAL_OK) return status;

    /* Start one-shot conversion: set ADC_MODE to conversion */
    config0_val = MCP3465R_CFG0_VREF_SEL_EXT |
                  MCP3465R_CFG0_CLK_SEL_INT  |
                  MCP3465R_CFG0_CS_SEL_NONE  |
                  MCP3465R_CFG0_ADC_MODE_ONESHOT;
    status = MCP3465R_WriteReg(dev_idx, MCP3465R_REG_CONFIG0, &config0_val, 1);
    if (status != HAL_OK) return status;

    /* Wait for conversion to complete
     * With OSR=256 and internal clock (~4.9MHz MCLK/4):
     * Conversion time ≈ 256 * DMCLK period ≈ ~0.2ms
     * Poll IRQ register for data ready, with timeout */
    uint32_t timeout = 50;  /* 50ms max timeout */
    uint32_t start_tick = HAL_GetTick();

    while (1) {
        status = MCP3465R_ReadReg(dev_idx, MCP3465R_REG_IRQ, &irq_reg, 1);
        if (status != HAL_OK) return status;

        /* Check if data is ready (DR_STATUS bit, active low in some configs) */
        if (!(irq_reg & MCP3465R_IRQ_DATA_RDY_MASK)) {
            break;  /* Data ready */
        }

        if ((HAL_GetTick() - start_tick) >= timeout) {
            return HAL_TIMEOUT;
        }

        osDelay(1);  /* Yield to other tasks */
    }

    /* Read ADC data register (3 bytes for 16-bit + sign extension) */
    status = MCP3465R_ReadReg(dev_idx, MCP3465R_REG_ADCDATA, adc_data, 3);
    if (status != HAL_OK) return status;

    /* Reconstruct 24-bit signed value, then extract 16-bit result */
    int32_t raw = ((int32_t)adc_data[0] << 16) |
                  ((int32_t)adc_data[1] << 8)  |
                  ((int32_t)adc_data[2]);

    /* Sign extend from 24-bit to 32-bit */
    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }

    /* For 16-bit mode, the effective data is in upper bits
     * The raw value represents the ratio: raw / 2^(resolution-1) * Vref
     * With default format, the value is already proportional */
    *raw_value = raw;

    /* Cache the reading */
    if (ch == MCP3465R_CH_VOLTAGE) {
        adc_devices[dev_idx].raw_voltage = raw;
    } else {
        adc_devices[dev_idx].raw_current = raw;
    }

    return HAL_OK;
}

float MCP3465R_RawToVoltage(int32_t raw_value)
{
    /* For single-ended measurement with external Vref:
     * Voltage = (raw / 2^23) * Vref
     * 2^23 = 8388608 (full-scale positive for 24-bit output) */
    float voltage = ((float)raw_value / 8388608.0f) * VREF_ADC;
    if (voltage < 0.0f) voltage = 0.0f;
    if (voltage > VREF_ADC) voltage = VREF_ADC;
    return voltage;
}

float MCP3465R_RawToCurrent(int32_t raw_value)
{
    /* Current = Vshunt / R_shunt
     * Vshunt is measured on CH1 through the shunt resistor
     * I (A) = V_shunt / R_shunt
     * I (mA) = V_shunt / R_shunt * 1000 */
    float v_shunt = ((float)raw_value / 8388608.0f) * VREF_ADC;
    if (v_shunt < 0.0f) v_shunt = 0.0f;

    float current_ma = (v_shunt / SHUNT_RESISTOR_OHM) * 1000.0f;
    return current_ma;
}

HAL_StatusTypeDef MCP3465R_WriteReg(uint8_t dev_idx, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    HAL_StatusTypeDef status;
    uint8_t cmd_byte;

    if (dev_idx >= NUM_CHANNELS) return HAL_ERROR;

    cmd_byte = MCP3465R_BuildCmd(reg_addr, MCP3465R_CMD_INC_WRITE);

    if (spi1_mutex != NULL) {
        osMutexAcquire(spi1_mutex, osWaitForever);
    }

    ADC_CS_Low(dev_idx);

    /* Send command byte */
    status = HAL_SPI_Transmit(adc_devices[dev_idx].hspi, &cmd_byte, 1, 10);
    if (status == HAL_OK && len > 0 && data != NULL) {
        /* Send data bytes */
        status = HAL_SPI_Transmit(adc_devices[dev_idx].hspi, data, len, 10);
    }

    ADC_CS_High(dev_idx);

    if (spi1_mutex != NULL) {
        osMutexRelease(spi1_mutex);
    }

    return status;
}

HAL_StatusTypeDef MCP3465R_ReadReg(uint8_t dev_idx, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    HAL_StatusTypeDef status;
    uint8_t cmd_byte;

    if (dev_idx >= NUM_CHANNELS || data == NULL) return HAL_ERROR;

    cmd_byte = MCP3465R_BuildCmd(reg_addr, MCP3465R_CMD_STATIC_READ);

    if (spi1_mutex != NULL) {
        osMutexAcquire(spi1_mutex, osWaitForever);
    }

    ADC_CS_Low(dev_idx);

    /* Send command byte */
    status = HAL_SPI_Transmit(adc_devices[dev_idx].hspi, &cmd_byte, 1, 10);
    if (status == HAL_OK) {
        /* Read response bytes */
        status = HAL_SPI_Receive(adc_devices[dev_idx].hspi, data, len, 10);
    }

    ADC_CS_High(dev_idx);

    if (spi1_mutex != NULL) {
        osMutexRelease(spi1_mutex);
    }

    return status;
}

HAL_StatusTypeDef MCP3465R_FastCommand(uint8_t dev_idx, uint8_t cmd)
{
    HAL_StatusTypeDef status;

    if (dev_idx >= NUM_CHANNELS) return HAL_ERROR;

    /* Fast command byte includes device address */
    uint8_t fast_cmd = (MCP3465R_DEV_ADDR << 6) | cmd;

    if (spi1_mutex != NULL) {
        osMutexAcquire(spi1_mutex, osWaitForever);
    }

    ADC_CS_Low(dev_idx);
    status = HAL_SPI_Transmit(adc_devices[dev_idx].hspi, &fast_cmd, 1, 10);
    ADC_CS_High(dev_idx);

    if (spi1_mutex != NULL) {
        osMutexRelease(spi1_mutex);
    }

    return status;
}
