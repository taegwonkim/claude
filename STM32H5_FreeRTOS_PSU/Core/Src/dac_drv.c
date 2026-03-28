/**
 * @file    dac_drv.c
 * @brief   DAC 드라이버 구현
 *
 * CH1/CH2: 내부 DAC1 (STM32H5 HAL)
 * CH3/CH4: 외부 MCP4922 (SPI2, 12-bit 듀얼 DAC)
 */

#include "dac_drv.h"
#include <string.h>

/* =========================================================
 * 내부 헬퍼
 * ========================================================= */

/**
 * @brief MCP4922에 SPI 16-bit 프레임 전송
 * @param ch_b   0=ChannelA(CH3), 1=ChannelB(CH4)
 * @param value  12-bit DAC 코드
 */
static void MCP4922_Write(uint8_t ch_b, uint16_t value)
{
    uint16_t frame = 0;

    /* 채널 선택 */
    if (ch_b) {
        frame |= MCP4922_SEL_CHB;
    } else {
        frame |= MCP4922_SEL_CHA;
    }

    /* 설정 비트: BUF=0(Unbuffered VREF), GAIN=1x, SHDN=Active */
    frame |= MCP4922_BUF_OFF;
    frame |= MCP4922_GAIN_1X;
    frame |= MCP4922_SHDN_ON;

    /* 12-bit 데이터 (비트[11:0]) */
    frame |= (value & 0x0FFFU);

    /* SPI 전송 (MSB first, CS 수동 제어) */
    HAL_GPIO_WritePin(MCP4922_CS_GPIO, MCP4922_CS_PIN, GPIO_PIN_RESET);

    uint8_t tx[2];
    tx[0] = (uint8_t)((frame >> 8) & 0xFF);
    tx[1] = (uint8_t)(frame & 0xFF);
    HAL_SPI_Transmit(&hspi2, tx, 2U, DAC_SPI_TIMEOUT_MS);

    HAL_GPIO_WritePin(MCP4922_CS_GPIO, MCP4922_CS_PIN, GPIO_PIN_SET);
}

/* =========================================================
 * 공개 함수 구현
 * ========================================================= */

void DAC_Drv_Init(void)
{
    /* 내부 DAC1 CH1 시작 */
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0U);

    /* 내부 DAC1 CH2 시작 */
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 0U);

    /* MCP4922 초기화: 두 채널 출력 0 */
    HAL_GPIO_WritePin(MCP4922_CS_GPIO, MCP4922_CS_PIN, GPIO_PIN_SET);
    MCP4922_Write(0U, 0U);   /* CH3 = 0 */
    MCP4922_Write(1U, 0U);   /* CH4 = 0 */
}

void DAC_Drv_SetChannel(uint8_t ch_idx, uint32_t value_12b)
{
    /* 범위 클리핑 */
    if (value_12b > DAC_MAX_CODE) {
        value_12b = DAC_MAX_CODE;
    }

    switch (ch_idx) {
        case 0U:
            /* CH1: 내부 DAC1_OUT1 */
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1,
                             DAC_ALIGN_12B_R, value_12b);
            break;

        case 1U:
            /* CH2: 내부 DAC1_OUT2 */
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2,
                             DAC_ALIGN_12B_R, value_12b);
            break;

        case 2U:
            /* CH3: MCP4922 Channel A */
            MCP4922_Write(0U, (uint16_t)value_12b);
            break;

        case 3U:
            /* CH4: MCP4922 Channel B */
            MCP4922_Write(1U, (uint16_t)value_12b);
            break;

        default:
            /* 범위 초과 무시 */
            break;
    }
}

uint32_t DAC_Drv_VoltageMvToCode(float v_mv)
{
    if (v_mv <= 0.0f) {
        return 0U;
    }
    if (v_mv >= VDAC_MAX_MV) {
        return DAC_MAX_CODE;
    }
    /* code = v_mv / VDAC_MAX_MV * (DAC_RESOLUTION - 1) */
    float code_f = (v_mv / VDAC_MAX_MV) * (DAC_RESOLUTION - 1.0f);
    return (uint32_t)(code_f + 0.5f);  /* 반올림 */
}

void DAC_Drv_DisableAll(void)
{
    for (uint8_t i = 0U; i < NUM_CHANNELS; i++) {
        DAC_Drv_SetChannel(i, 0U);
    }
}
