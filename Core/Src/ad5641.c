/**
 ******************************************************************************
 * @file    ad5641.c
 * @brief   AD5641 14-bit DAC 드라이버 구현
 *
 * AD5641 SPI 통신 프로토콜:
 *   1. SYNC(CS) Low
 *   2. 16-bit 데이터 전송 (MSB first)
 *      [PD1:PD0][D13:D0]
 *   3. SYNC(CS) High → 데이터가 DAC 레지스터에 래치
 *
 * 전압 출력:
 *   Vout = Vref × D / 2^14
 *   Vref = 5.0V 기준:
 *     0.5V → D = 1638
 *     4.5V → D = 14746
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "ad5641.h"

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  SPI를 통해 16-bit 데이터를 AD5641에 전송
 */
static HAL_StatusTypeDef AD5641_Write16(AD5641_Handle_t *hdac, uint16_t data)
{
    HAL_StatusTypeDef status;
    uint8_t tx_buf[2];

    /* MSB first */
    tx_buf[0] = (uint8_t)(data >> 8);
    tx_buf[1] = (uint8_t)(data & 0xFF);

    /* CS(SYNC) Low */
    HAL_GPIO_WritePin(hdac->cs_port, hdac->cs_pin, GPIO_PIN_RESET);

    /* SPI 전송 */
    status = HAL_SPI_Transmit(hdac->hspi, tx_buf, 2, AD5641_SPI_TIMEOUT);

    /* CS(SYNC) High → 데이터 래치 */
    HAL_GPIO_WritePin(hdac->cs_port, hdac->cs_pin, GPIO_PIN_SET);

    return status;
}

/* Exported functions --------------------------------------------------------*/

HAL_StatusTypeDef AD5641_Init(AD5641_Handle_t *hdac, SPI_HandleTypeDef *hspi,
                               GPIO_TypeDef *cs_port, uint16_t cs_pin,
                               float vref)
{
    if (hdac == NULL || hspi == NULL || cs_port == NULL) {
        return HAL_ERROR;
    }

    hdac->hspi = hspi;
    hdac->cs_port = cs_port;
    hdac->cs_pin = cs_pin;
    hdac->vref = vref;
    hdac->current_code = 0;

    /* CS를 High로 초기화 (비활성) */
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    /* DAC 출력을 0V로 초기화 */
    return AD5641_SetRaw(hdac, 0);
}

HAL_StatusTypeDef AD5641_SetRaw(AD5641_Handle_t *hdac, uint16_t code)
{
    if (hdac == NULL) {
        return HAL_ERROR;
    }

    /* 14-bit 범위 제한 */
    if (code > AD5641_MAX_CODE) {
        code = AD5641_MAX_CODE;
    }

    /* 데이터 포맷: [00][D13:D0] (Normal operation mode) */
    uint16_t data = AD5641_MODE_NORMAL | (code & AD5641_MAX_CODE);

    HAL_StatusTypeDef status = AD5641_Write16(hdac, data);
    if (status == HAL_OK) {
        hdac->current_code = code;
    }

    return status;
}

HAL_StatusTypeDef AD5641_SetVoltage(AD5641_Handle_t *hdac, float voltage)
{
    if (hdac == NULL) {
        return HAL_ERROR;
    }

    /* 전압 범위 제한 */
    if (voltage < 0.0f) {
        voltage = 0.0f;
    }
    if (voltage > hdac->vref) {
        voltage = hdac->vref;
    }

    uint16_t code = AD5641_VoltageToCod(hdac, voltage);
    return AD5641_SetRaw(hdac, code);
}

HAL_StatusTypeDef AD5641_PowerDown(AD5641_Handle_t *hdac, uint16_t mode)
{
    if (hdac == NULL) {
        return HAL_ERROR;
    }

    uint16_t data = mode & 0xC000;  /* 상위 2비트만 사용 */
    return AD5641_Write16(hdac, data);
}

uint16_t AD5641_VoltageToCod(AD5641_Handle_t *hdac, float voltage)
{
    if (hdac == NULL || hdac->vref <= 0.0f) {
        return 0;
    }

    if (voltage <= 0.0f) {
        return 0;
    }
    if (voltage >= hdac->vref) {
        return AD5641_MAX_CODE;
    }

    /* D = (V / Vref) × 2^14 */
    float code_f = (voltage / hdac->vref) * (float)(AD5641_MAX_CODE + 1);
    uint16_t code = (uint16_t)code_f;

    if (code > AD5641_MAX_CODE) {
        code = AD5641_MAX_CODE;
    }

    return code;
}

float AD5641_CodeToVoltage(AD5641_Handle_t *hdac, uint16_t code)
{
    if (hdac == NULL) {
        return 0.0f;
    }

    if (code > AD5641_MAX_CODE) {
        code = AD5641_MAX_CODE;
    }

    /* V = Vref × D / 2^14 */
    return hdac->vref * (float)code / (float)(AD5641_MAX_CODE + 1);
}
