/**
 ******************************************************************************
 * @file    dac_ad5641.c
 * @brief   AD5641 14비트 SPI DAC 드라이버 구현
 ******************************************************************************
 */

#include "dac_ad5641.h"
#include "cmsis_os.h"

/* -------------------------------------------------------------------------
 * 채널별 CS 핀 매핑 테이블
 * ------------------------------------------------------------------------- */
static const DacCsPin_t s_cs_pins[NUM_CHANNELS] = {
    { DAC_CS0_PORT, DAC_CS0_PIN },
    { DAC_CS1_PORT, DAC_CS1_PIN },
    { DAC_CS2_PORT, DAC_CS2_PIN },
    { DAC_CS3_PORT, DAC_CS3_PIN },
};

/* -------------------------------------------------------------------------
 * 내부 헬퍼
 * ------------------------------------------------------------------------- */
static inline void _cs_low(uint8_t ch)
{
    HAL_GPIO_WritePin(s_cs_pins[ch].port, s_cs_pins[ch].pin, GPIO_PIN_RESET);
}

static inline void _cs_high(uint8_t ch)
{
    HAL_GPIO_WritePin(s_cs_pins[ch].port, s_cs_pins[ch].pin, GPIO_PIN_SET);
}

/* -------------------------------------------------------------------------
 * 공개 API 구현
 * ------------------------------------------------------------------------- */

void DAC_Init(void)
{
    /* 모든 CS 핀 High (비선택) */
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        _cs_high(i);
    }

    /* 전원 인가 후 모든 채널 0V 출력으로 초기화 */
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        DAC_WriteCode(i, 0, DAC_PD_NORMAL);
    }
}

HAL_StatusTypeDef DAC_WriteCode(uint8_t channel, uint16_t code, DacPowerDown_t pd_mode)
{
    if (channel >= NUM_CHANNELS) {
        return HAL_ERROR;
    }

    /* 코드 범위 클램프 */
    if (code > DAC_MAX_CODE) {
        code = DAC_MAX_CODE;
    }

    /*
     * AD5641 16비트 전송 포맷:
     *   [15:14] PD1, PD0
     *   [13: 0] D13~D0
     */
    uint16_t word = (uint16_t)pd_mode | (code & DAC_MAX_CODE);

    /* SPI1 뮤텍스 획득 */
    if (osMutexAcquire(xMutexSPI1, 10) != osOK) {
        return HAL_BUSY;
    }

    _cs_low(channel);

    HAL_StatusTypeDef ret = HAL_SPI_Transmit(&hspi1,
                                              (uint8_t *)&word,
                                              1,          /* 1 × 16-bit word */
                                              10);        /* 10ms timeout */
    /* SYNC(CS)는 최소 SCLK 16클럭 후 High */
    _cs_high(channel);

    osMutexRelease(xMutexSPI1);

    return ret;
}

HAL_StatusTypeDef DAC_SetVoltage(uint8_t channel, uint16_t voltage_mv)
{
    uint16_t code = DAC_VoltageToCode(voltage_mv);
    return DAC_WriteCode(channel, code, DAC_PD_NORMAL);
}

HAL_StatusTypeDef DAC_PowerDown(uint8_t channel, DacPowerDown_t pd_mode)
{
    if (pd_mode == DAC_PD_NORMAL) {
        return HAL_ERROR;   /* PowerDown 함수로 Normal 설정 방지 */
    }
    return DAC_WriteCode(channel, 0, pd_mode);
}
