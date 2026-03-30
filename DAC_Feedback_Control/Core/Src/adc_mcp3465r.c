/**
 ******************************************************************************
 * @file    adc_mcp3465r.c
 * @brief   MCP3465R 16비트 델타-시그마 ADC 드라이버 구현
 ******************************************************************************
 */

#include "adc_mcp3465r.h"
#include "cmsis_os.h"

/* -------------------------------------------------------------------------
 * 내부 헬퍼
 * ------------------------------------------------------------------------- */
static inline void _adc_cs_low(void)
{
    HAL_GPIO_WritePin(ADC_CS_PORT, ADC_CS_PIN, GPIO_PIN_RESET);
}

static inline void _adc_cs_high(void)
{
    HAL_GPIO_WritePin(ADC_CS_PORT, ADC_CS_PIN, GPIO_PIN_SET);
}

/**
 * @brief  1바이트 레지스터 쓰기 (CS는 호출자가 관리)
 */
static HAL_StatusTypeDef _write_reg(uint8_t reg_addr, uint8_t value)
{
    uint8_t buf[2];
    buf[0] = MCP3465R_CMD_BYTE(reg_addr, MCP3465R_CMD_WRITE);
    buf[1] = value;
    return HAL_SPI_Transmit(&hspi2, buf, 2, 5);
}

/**
 * @brief  1바이트 레지스터 읽기 (CS는 호출자가 관리)
 */
static HAL_StatusTypeDef _read_reg(uint8_t reg_addr, uint8_t *out)
{
    uint8_t tx[2] = { MCP3465R_CMD_BYTE(reg_addr, MCP3465R_CMD_READ), 0xFF };
    uint8_t rx[2] = { 0 };
    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, 5);
    *out = rx[1];
    return ret;
}

/**
 * @brief  Fast Command 전송
 */
static HAL_StatusTypeDef _fast_cmd(uint8_t fast_cmd_byte)
{
    return HAL_SPI_Transmit(&hspi2, &fast_cmd_byte, 1, 5);
}

/**
 * @brief  ADCDATA 레지스터 읽기 (2바이트 = 16비트 부호형)
 */
static HAL_StatusTypeDef _read_data(int16_t *out)
{
    uint8_t tx[3] = { MCP3465R_CMD_BYTE(MCP3465R_ADCDATA, MCP3465R_CMD_READ), 0xFF, 0xFF };
    uint8_t rx[3] = { 0 };
    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, 10);
    if (ret == HAL_OK) {
        /* 빅엔디안: rx[1]=MSB, rx[2]=LSB */
        *out = (int16_t)((rx[1] << 8) | rx[2]);
    }
    return ret;
}

/* -------------------------------------------------------------------------
 * MUX 채널 매핑 (CH0~CH7 단일-끝)
 * CH0~3: 전압 측정, CH4~7: 전류(션트) 측정
 * ------------------------------------------------------------------------- */
static const uint8_t s_mux_table[8] = {
    MCP3465R_MUX_CH(0, MCP3465R_AGND),   /* CH0 */
    MCP3465R_MUX_CH(1, MCP3465R_AGND),   /* CH1 */
    MCP3465R_MUX_CH(2, MCP3465R_AGND),   /* CH2 */
    MCP3465R_MUX_CH(3, MCP3465R_AGND),   /* CH3 */
    MCP3465R_MUX_CH(4, MCP3465R_AGND),   /* CH4 */
    MCP3465R_MUX_CH(5, MCP3465R_AGND),   /* CH5 */
    MCP3465R_MUX_CH(6, MCP3465R_AGND),   /* CH6 */
    MCP3465R_MUX_CH(7, MCP3465R_AGND),   /* CH7 */
};

/* -------------------------------------------------------------------------
 * 공개 API 구현
 * ------------------------------------------------------------------------- */

HAL_StatusTypeDef ADC_Init(void)
{
    HAL_StatusTypeDef ret;

    /* CS High 초기화 */
    _adc_cs_high();
    osDelay(1);

    if (osMutexAcquire(xMutexSPI2, 50) != osOK) {
        return HAL_BUSY;
    }

    _adc_cs_low();

    /* Full Reset */
    ret = _fast_cmd(MCP3465R_FAST_RESET);
    _adc_cs_high();
    if (ret != HAL_OK) { osMutexRelease(xMutexSPI2); return ret; }
    osDelay(2);

    _adc_cs_low();

    /*
     * CONFIG0:
     *   VREF = External (REFIN 핀 사용, Vref=3.3V)
     *   CLK  = Internal oscillator (no CLK output)
     *   CS   = No bias current source
     *   MODE = Continuous conversion
     */
    ret = _write_reg(MCP3465R_CONFIG0,
                     MCP3465R_CONFIG0_VREF_EXT |
                     MCP3465R_CONFIG0_CLK_INT  |
                     MCP3465R_CONFIG0_CS_NONE  |
                     MCP3465R_CONFIG0_MODE_CONT);
    if (ret != HAL_OK) goto init_exit;

    /*
     * CONFIG1:
     *   OSR = 1024 → 약 7.8 ksps (1ms 주기에 충분)
     *   DITHER = 00 (default)
     */
    ret = _write_reg(MCP3465R_CONFIG1, MCP3465R_OSR_1024);
    if (ret != HAL_OK) goto init_exit;

    /*
     * CONFIG2:
     *   GAIN = 1 (×1)
     *   AZ_MUX = Enable (auto-zeroing)
     *   VREF_EXT = 1 (외부 VREF 사용 확인)
     */
    ret = _write_reg(MCP3465R_CONFIG2,
                     MCP3465R_GAIN_1 | MCP3465R_AZ_MUX_EN | 0x03U);
    if (ret != HAL_OK) goto init_exit;

    /*
     * CONFIG3:
     *   DATA_FORMAT = 11b → 16비트 부호형
     *   CONV_MODE   = 11b → Continuous (CONFIG0와 동기)
     *   CRC_FORMAT  = 00b (CRC 없음)
     *   EN_CRCCOM   = 0
     *   EN_OFFCAL   = 0
     *   EN_GAINCAL  = 0
     */
    ret = _write_reg(MCP3465R_CONFIG3,
                     MCP3465R_FMT_16BIT_SGN | MCP3465R_CONV_CONT | MCP3465R_CRC_OFF);
    if (ret != HAL_OK) goto init_exit;

    /*
     * IRQ 레지스터:
     *   IRQ_MODE = 11b → IRQ핀 High-Z (폴링 모드 사용 시)
     *   EN_FASTCMD = 1
     *   EN_STP = 0
     */
    ret = _write_reg(MCP3465R_IRQ, 0x07U);
    if (ret != HAL_OK) goto init_exit;

    /* 첫 번째 채널 MUX 설정 */
    ret = _write_reg(MCP3465R_MUX, s_mux_table[0]);

init_exit:
    _adc_cs_high();
    osMutexRelease(xMutexSPI2);
    return ret;
}

HAL_StatusTypeDef ADC_ReadChannel(uint8_t mux_ch, int16_t *out)
{
    if (out == NULL) return HAL_ERROR;

    HAL_StatusTypeDef ret;

    if (osMutexAcquire(xMutexSPI2, 10) != osOK) {
        return HAL_BUSY;
    }

    _adc_cs_low();

    /* MUX 변경 */
    ret = _write_reg(MCP3465R_MUX, mux_ch);
    if (ret != HAL_OK) goto read_exit;

    _adc_cs_high();

    /*
     * MUX 변경 후 최소 2회 변환 완료 대기 (안정화)
     * OSR=1024 @ ~16MHz MCLK → 변환 시간 ≈ 128μs
     * 여유 있게 1ms 대기
     */
    osDelay(2);

    /* IRQ 핀 폴링 또는 그냥 읽기
     * MCP3465R_IRQ 핀이 Low 됐을 때 데이터 준비됨
     * 폴링: 핀 상태 무시하고 최신 데이터 읽기 (continuous 모드)
     */
    _adc_cs_low();
    ret = _read_data(out);

read_exit:
    _adc_cs_high();
    osMutexRelease(xMutexSPI2);
    return ret;
}

HAL_StatusTypeDef ADC_ReadAllChannels(AdcMeasurement_t *result)
{
    if (result == NULL) return HAL_ERROR;

    HAL_StatusTypeDef ret = HAL_OK;

    for (uint8_t i = 0; i < 8; i++) {
        ret = ADC_ReadChannel(s_mux_table[i], &result->raw[i]);
        if (ret != HAL_OK) {
            return ret;
        }
    }

    /* 원시값 → 물리량 변환 */
    for (uint8_t i = 0; i < 4; i++) {
        result->voltage_mv[i] = ADC_RawToMillivolts(result->raw[i]);
        result->current_ma[i] = ADC_RawToMilliamps(result->raw[i + 4]);
    }

    result->timestamp = osKernelGetTickCount();

    return HAL_OK;
}
