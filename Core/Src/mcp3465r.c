/**
 ******************************************************************************
 * @file    mcp3465r.c
 * @brief   MCP3465R 4채널 16-bit Delta-Sigma ADC 드라이버 구현
 *
 * SPI 통신 프로토콜:
 *   Command byte: [ADDR1:ADDR0][REG3:REG0][CMD1:CMD0]
 *
 *   레지스터 쓰기:
 *     CS Low → CMD(INC_WRITE) → Data byte(s) → CS High
 *
 *   레지스터 읽기:
 *     CS Low → CMD(STATIC_READ) → Read byte(s) → CS High
 *
 *   ADC 데이터 읽기:
 *     1. MUX 레지스터로 채널 선택
 *     2. Fast Command로 변환 시작
 *     3. IRQ 핀 또는 폴링으로 변환 완료 대기
 *     4. ADCDATA 레지스터 읽기
 *
 * 전압 계산:
 *   V_adc = (raw_code / 2^15) × Vref / Gain
 *   V_actual = V_adc × divider_scale
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "mcp3465r.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define CMD_BYTE(addr, reg, cmd) \
    (uint8_t)(((addr) << 6) | ((reg) << 2) | (cmd))

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  게인 열거형을 실제 게인 값으로 변환
 */
static float MCP3465R_GetGainValue(MCP3465R_Gain_t gain)
{
    static const float gain_table[] = {
        0.333f,  /* 1/3x */
        1.0f,    /* 1x */
        2.0f,    /* 2x */
        4.0f,    /* 4x */
        8.0f,    /* 8x */
        16.0f,   /* 16x */
        32.0f,   /* 32x */
        64.0f    /* 64x */
    };

    if (gain > MCP3465R_GAIN_64X) {
        return 1.0f;
    }
    return gain_table[gain];
}

/**
 * @brief  변환 완료 대기 (폴링 방식)
 */
static HAL_StatusTypeDef MCP3465R_WaitConversion(MCP3465R_Handle_t *hadc)
{
    uint32_t timeout = HAL_GetTick() + MCP3465R_CONV_TIMEOUT;

    /* IRQ 핀이 설정된 경우 핀 폴링 */
    if (hadc->irq_port != NULL) {
        while (HAL_GPIO_ReadPin(hadc->irq_port, hadc->irq_pin) != GPIO_PIN_RESET) {
            if (HAL_GetTick() > timeout) {
                return HAL_TIMEOUT;
            }
        }
        return HAL_OK;
    }

    /* IRQ 핀이 없으면 IRQ 레지스터 폴링 */
    uint8_t irq_reg;
    while (1) {
        if (MCP3465R_ReadReg(hadc, MCP3465R_REG_IRQ, &irq_reg) != HAL_OK) {
            return HAL_ERROR;
        }

        /* Data Ready 비트 확인 (active-low) */
        if ((irq_reg & MCP3465R_IRQ_DR_STATUS) == 0) {
            return HAL_OK;
        }

        if (HAL_GetTick() > timeout) {
            return HAL_TIMEOUT;
        }
    }
}

/* Exported functions --------------------------------------------------------*/

HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hadc,
                                 SPI_HandleTypeDef *hspi,
                                 GPIO_TypeDef *cs_port, uint16_t cs_pin,
                                 float vref, float divider_scale)
{
    HAL_StatusTypeDef status;

    if (hadc == NULL || hspi == NULL || cs_port == NULL) {
        return HAL_ERROR;
    }

    hadc->hspi = hspi;
    hadc->cs_port = cs_port;
    hadc->cs_pin = cs_pin;
    hadc->irq_port = NULL;
    hadc->irq_pin = 0;
    hadc->vref = vref;
    hadc->gain = MCP3465R_GAIN_033X;
    hadc->divider_scale = divider_scale;
    memset(hadc->raw_data, 0, sizeof(hadc->raw_data));

    /* CS를 High로 초기화 */
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    /* 소프트 리셋 */
    status = MCP3465R_Reset(hadc);
    if (status != HAL_OK) return status;

    HAL_Delay(5);  /* 리셋 후 안정화 대기 */

    /*
     * CONFIG0: 내부 Vref, 내부 클럭(출력 없음), 전류 소스 없음, Standby
     */
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG0,
        MCP3465R_CFG0_VREF_INT | MCP3465R_CFG0_CLK_INT |
        MCP3465R_CFG0_CS_NONE | MCP3465R_CFG0_ADC_STANDBY);
    if (status != HAL_OK) return status;

    /*
     * CONFIG1: 프리스케일러 /1, OSR = 256
     * OSR 256은 약 600 SPS (내부 클럭 기준), 4채널 순차 읽기에 적합
     */
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG1,
        MCP3465R_CFG1_PRE_1 | MCP3465R_CFG1_OSR_256);
    if (status != HAL_OK) return status;

    /*
     * CONFIG2: 부스트 1x, 게인 1/3x, Auto-zero MUX 활성
     * 게인 1/3x + 내부 Vref 2.4V → 입력 범위 ≈ 0 ~ 7.2V (공급 전압으로 제한)
     * 전압 분배기(1:1)를 사용하므로 실제 측정 범위: 0 ~ 약 5V
     */
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG2,
        MCP3465R_CFG2_BOOST_1X | MCP3465R_CFG2_GAIN_033X |
        MCP3465R_CFG2_AZ_MUX_EN);
    if (status != HAL_OK) return status;

    /*
     * CONFIG3: One-shot 모드, 16-bit 데이터 포맷, CRC 비활성
     */
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG3,
        MCP3465R_CFG3_CONV_ONESHOT | MCP3465R_CFG3_DATA_16BIT |
        MCP3465R_CFG3_CRC_OFF);
    if (status != HAL_OK) return status;

    /*
     * IRQ: Fast command 활성, STP 활성
     */
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_IRQ,
        MCP3465R_IRQ_EN_FASTCMD | MCP3465R_IRQ_EN_STP);
    if (status != HAL_OK) return status;

    /*
     * MUX: 기본값 CH0 vs AGND (싱글 엔디드)
     */
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_MUX,
        (MCP3465R_MUX_VIN_CH0 << 4) | MCP3465R_MUX_VIN_AGND);
    if (status != HAL_OK) return status;

    return HAL_OK;
}

void MCP3465R_SetIRQPin(MCP3465R_Handle_t *hadc,
                         GPIO_TypeDef *irq_port, uint16_t irq_pin)
{
    if (hadc != NULL) {
        hadc->irq_port = irq_port;
        hadc->irq_pin = irq_pin;
    }
}

HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hadc, uint8_t reg,
                                     uint8_t data)
{
    HAL_StatusTypeDef status;
    uint8_t tx_buf[2];

    tx_buf[0] = CMD_BYTE(MCP3465R_DEV_ADDR, reg, MCP3465R_CMD_INC_WRITE);
    tx_buf[1] = data;

    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(hadc->hspi, tx_buf, 2, MCP3465R_SPI_TIMEOUT);
    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_SET);

    return status;
}

HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *hadc, uint8_t reg,
                                    uint8_t *data)
{
    HAL_StatusTypeDef status;
    uint8_t tx_buf[2] = {0};
    uint8_t rx_buf[2] = {0};

    tx_buf[0] = CMD_BYTE(MCP3465R_DEV_ADDR, reg, MCP3465R_CMD_STATIC_READ);

    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(hadc->hspi, tx_buf, rx_buf, 2,
                                      MCP3465R_SPI_TIMEOUT);
    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_SET);

    if (status == HAL_OK) {
        *data = rx_buf[1];
    }

    return status;
}

HAL_StatusTypeDef MCP3465R_SetGain(MCP3465R_Handle_t *hadc,
                                    MCP3465R_Gain_t gain)
{
    if (hadc == NULL) {
        return HAL_ERROR;
    }

    uint8_t cfg2 = MCP3465R_CFG2_BOOST_1X | ((uint8_t)gain << 3) |
                    MCP3465R_CFG2_AZ_MUX_EN;

    HAL_StatusTypeDef status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG2,
                                                  cfg2);
    if (status == HAL_OK) {
        hadc->gain = gain;
    }

    return status;
}

HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_Handle_t *hadc,
                                        MCP3465R_Channel_t channel,
                                        int16_t *raw_value)
{
    HAL_StatusTypeDef status;

    if (hadc == NULL || raw_value == NULL || channel >= MCP3465R_NUM_CHANNELS) {
        return HAL_ERROR;
    }

    /* 1. MUX 설정: 선택 채널 vs AGND (싱글 엔디드) */
    uint8_t mux_val = ((uint8_t)channel << 4) | MCP3465R_MUX_VIN_AGND;
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_MUX, mux_val);
    if (status != HAL_OK) return status;

    /* 2. CONFIG0에서 ADC 모드를 Conversion으로 변경 */
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG0,
        MCP3465R_CFG0_VREF_INT | MCP3465R_CFG0_CLK_INT |
        MCP3465R_CFG0_CS_NONE | MCP3465R_CFG0_ADC_CONV);
    if (status != HAL_OK) return status;

    /* 3. 변환 완료 대기 */
    status = MCP3465R_WaitConversion(hadc);
    if (status != HAL_OK) return status;

    /* 4. ADCDATA 읽기 (16-bit 포맷) */
    uint8_t tx_buf[3] = {0};
    uint8_t rx_buf[3] = {0};

    tx_buf[0] = CMD_BYTE(MCP3465R_DEV_ADDR, MCP3465R_REG_ADCDATA,
                          MCP3465R_CMD_STATIC_READ);

    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(hadc->hspi, tx_buf, rx_buf, 3,
                                      MCP3465R_SPI_TIMEOUT);
    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_SET);

    if (status != HAL_OK) return status;

    /* 16-bit signed 데이터 조합 (MSB first) */
    *raw_value = (int16_t)((rx_buf[1] << 8) | rx_buf[2]);

    /* 내부 캐시 업데이트 */
    hadc->raw_data[channel] = *raw_value;

    /* 5. ADC를 다시 Standby 모드로 */
    status = MCP3465R_WriteReg(hadc, MCP3465R_REG_CONFIG0,
        MCP3465R_CFG0_VREF_INT | MCP3465R_CFG0_CLK_INT |
        MCP3465R_CFG0_CS_NONE | MCP3465R_CFG0_ADC_STANDBY);

    return status;
}

HAL_StatusTypeDef MCP3465R_ReadVoltage(MCP3465R_Handle_t *hadc,
                                        MCP3465R_Channel_t channel,
                                        float *voltage)
{
    int16_t raw;
    HAL_StatusTypeDef status;

    if (hadc == NULL || voltage == NULL) {
        return HAL_ERROR;
    }

    status = MCP3465R_ReadChannel(hadc, channel, &raw);
    if (status != HAL_OK) return status;

    /*
     * 전압 변환:
     *   V_adc = (raw / 32768) × (Vref / Gain)
     *   V_actual = V_adc × divider_scale
     *
     * 16-bit signed: -32768 ~ +32767
     * 싱글 엔디드에서는 양수 값만 유효 (0 ~ 32767)
     */
    float gain = MCP3465R_GetGainValue(hadc->gain);
    float v_adc = ((float)raw / 32768.0f) * (hadc->vref / gain);

    /* 음수 클램핑 (싱글 엔디드에서 노이즈로 인한 소량의 음수 가능) */
    if (v_adc < 0.0f) {
        v_adc = 0.0f;
    }

    /* 전압 분배기 보정 적용 */
    *voltage = v_adc * hadc->divider_scale;

    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_Handle_t *hadc,
                                            float voltages[MCP3465R_NUM_CHANNELS])
{
    HAL_StatusTypeDef status;

    if (hadc == NULL || voltages == NULL) {
        return HAL_ERROR;
    }

    for (int i = 0; i < MCP3465R_NUM_CHANNELS; i++) {
        status = MCP3465R_ReadVoltage(hadc, (MCP3465R_Channel_t)i,
                                       &voltages[i]);
        if (status != HAL_OK) {
            /* 실패한 채널은 0으로 설정하고 계속 진행 */
            voltages[i] = 0.0f;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_FastCommand(MCP3465R_Handle_t *hadc, uint8_t cmd)
{
    HAL_StatusTypeDef status;
    uint8_t tx_buf;

    if (hadc == NULL) {
        return HAL_ERROR;
    }

    /* Fast command byte: [ADDR1:0][CMD5:2][11] */
    tx_buf = (MCP3465R_DEV_ADDR << 6) | (cmd << 2) | 0x03;

    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(hadc->hspi, &tx_buf, 1, MCP3465R_SPI_TIMEOUT);
    HAL_GPIO_WritePin(hadc->cs_port, hadc->cs_pin, GPIO_PIN_SET);

    return status;
}

HAL_StatusTypeDef MCP3465R_Reset(MCP3465R_Handle_t *hadc)
{
    return MCP3465R_FastCommand(hadc, MCP3465R_FAST_CMD_RESET);
}
