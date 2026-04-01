/**
  ******************************************************************************
  * @file           : mcp3465r.h
  * @brief          : MCP3465R 16-bit Delta-Sigma ADC driver header (SPI)
  *                   4 devices on SPI1, each with CH0(voltage) and CH1(current)
  ******************************************************************************
  */
#ifndef __MCP3465R_H
#define __MCP3465R_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os2.h"

/* ---- MCP3465R Register Addresses ---- */
#define MCP3465R_REG_ADCDATA    0x00
#define MCP3465R_REG_CONFIG0    0x01
#define MCP3465R_REG_CONFIG1    0x02
#define MCP3465R_REG_CONFIG2    0x03
#define MCP3465R_REG_CONFIG3    0x04
#define MCP3465R_REG_IRQ        0x05
#define MCP3465R_REG_MUX        0x06
#define MCP3465R_REG_SCAN       0x07
#define MCP3465R_REG_TIMER      0x08
#define MCP3465R_REG_OFFSETCAL  0x09
#define MCP3465R_REG_GAINCAL    0x0A
#define MCP3465R_REG_LOCK       0x0D

/* ---- Command Byte Format ---- */
/* [ADDR1:ADDR0 | REG3:REG2:REG1:REG0 | CMD1:CMD0] */
#define MCP3465R_DEV_ADDR       0x01    /* Default device address (01) */
#define MCP3465R_CMD_STATIC_READ    0x01
#define MCP3465R_CMD_INC_WRITE      0x02
#define MCP3465R_CMD_INC_READ       0x03

/* Fast Commands */
#define MCP3465R_FAST_CMD_CONV_START    0x28  /* Start conversion */
#define MCP3465R_FAST_CMD_STANDBY       0x2C
#define MCP3465R_FAST_CMD_SHUTDOWN      0x30
#define MCP3465R_FAST_CMD_FULL_SHUTDOWN  0x34
#define MCP3465R_FAST_CMD_FULL_RESET    0x38

/* ---- CONFIG0 Register ---- */
#define MCP3465R_CFG0_VREF_SEL_EXT      0x00  /* External Vref */
#define MCP3465R_CFG0_VREF_SEL_INT      0x80  /* Internal 2.4V Vref */
#define MCP3465R_CFG0_CLK_SEL_INT       0x30  /* Internal clock, no output */
#define MCP3465R_CFG0_CS_SEL_NONE       0x00  /* No current source */
#define MCP3465R_CFG0_ADC_MODE_CONV     0x03  /* Conversion mode (continuous) */
#define MCP3465R_CFG0_ADC_MODE_ONESHOT  0x02  /* One-shot conversion */
#define MCP3465R_CFG0_ADC_MODE_STANDBY  0x00  /* Standby */

/* ---- CONFIG1 Register ---- */
#define MCP3465R_CFG1_PRESCALE_1        0x00
#define MCP3465R_CFG1_OSR_256           0x0C  /* OSR = 256 */
#define MCP3465R_CFG1_OSR_1024          0x10  /* OSR = 1024 */
#define MCP3465R_CFG1_OSR_4096          0x14  /* OSR = 4096 */

/* ---- CONFIG2 Register ---- */
#define MCP3465R_CFG2_BOOST_1X          0x40  /* 1x boost */
#define MCP3465R_CFG2_GAIN_1X           0x02  /* Gain = 1x */
#define MCP3465R_CFG2_AZ_MUX_EN        0x04  /* Auto-zero MUX enable */
#define MCP3465R_CFG2_AZ_REF_EN        0x02  /* Auto-zero Vref enable (if supported) */

/* ---- CONFIG3 Register ---- */
#define MCP3465R_CFG3_CONV_MODE_CONT    0xC0  /* Continuous conversion */
#define MCP3465R_CFG3_CONV_MODE_ONESHOT 0x80
#define MCP3465R_CFG3_DATA_FORMAT_32    0x30  /* 32-bit with sign extension + ch id */
#define MCP3465R_CFG3_DATA_FORMAT_DEF   0x00  /* Default: 24/32 bit depending on res */
#define MCP3465R_CFG3_CRC_FORMAT_16     0x00
#define MCP3465R_CFG3_EN_CRCCOM         0x08  /* Enable CRC on read communications */

/* ---- MUX Register ---- */
#define MCP3465R_MUX_VIN_CH0            0x00  /* CH0 for VIN+ */
#define MCP3465R_MUX_VIN_CH1            0x10  /* CH1 for VIN+ */
#define MCP3465R_MUX_VIN_AGND           0x08  /* AGND for VIN- (single-ended) */

#define MCP3465R_MUX_CH0_SE             (MCP3465R_MUX_VIN_CH0 | MCP3465R_MUX_VIN_AGND)  /* CH0 single-ended: 0x08 */
#define MCP3465R_MUX_CH1_SE             (MCP3465R_MUX_VIN_CH1 | MCP3465R_MUX_VIN_AGND)  /* CH1 single-ended: 0x18 */

/* ---- IRQ Register ---- */
#define MCP3465R_IRQ_EN_STP             0x40
#define MCP3465R_IRQ_EN_FASTCMD         0x02
#define MCP3465R_IRQ_MODE_LOGIC_HIGH    0x04  /* IRQ pin inactive = logic high */
#define MCP3465R_IRQ_DATA_RDY_MASK      0x40

/* ADC input channels */
typedef enum {
    MCP3465R_CH_VOLTAGE = 0,    /* Channel 0: Voltage feedback */
    MCP3465R_CH_CURRENT = 1     /* Channel 1: Current measurement */
} MCP3465R_Channel_t;

/* Device instance */
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    int32_t            raw_voltage;  /* Last raw ADC reading - voltage */
    int32_t            raw_current;  /* Last raw ADC reading - current */
} MCP3465R_Device_t;

/* Initialize all MCP3465R devices */
void MCP3465R_Init(MCP3465R_Device_t *devs, SPI_HandleTypeDef *hspi, osMutexId_t spi_mutex);

/* Configure a specific ADC device */
HAL_StatusTypeDef MCP3465R_Configure(uint8_t dev_idx);

/* Read a specific channel from a specific device (blocking) */
HAL_StatusTypeDef MCP3465R_ReadChannel(uint8_t dev_idx, MCP3465R_Channel_t ch, int32_t *raw_value);

/* Convert raw ADC value to voltage (V) */
float MCP3465R_RawToVoltage(int32_t raw_value);

/* Convert raw ADC value to current (mA) based on shunt resistor */
float MCP3465R_RawToCurrent(int32_t raw_value);

/* Write register */
HAL_StatusTypeDef MCP3465R_WriteReg(uint8_t dev_idx, uint8_t reg_addr, uint8_t *data, uint8_t len);

/* Read register */
HAL_StatusTypeDef MCP3465R_ReadReg(uint8_t dev_idx, uint8_t reg_addr, uint8_t *data, uint8_t len);

/* Send fast command */
HAL_StatusTypeDef MCP3465R_FastCommand(uint8_t dev_idx, uint8_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* __MCP3465R_H */
