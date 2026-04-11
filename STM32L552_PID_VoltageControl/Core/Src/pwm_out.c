/* pwm_out.c - AD5641 외부 DAC 구현 (SPI1)
 *
 * AD5641 SPI 전송:
 *   1. DAC_CS_LOW(ch)
 *   2. HAL_SPI_Transmit 16비트: {PD1:PD0:D13:...:D0}
 *   3. DAC_CS_HIGH(ch)
 *
 * SPI1 설정 (main.c MX_SPI1_Init):
 *   - Mode: Master, Full-Duplex
 *   - DataSize: 16-bit
 *   - CPOL=HIGH (1), CPHA=1EDGE (0) → Mode 2
 *   - NSS: Software (CS는 GPIO로 수동 제어)
 *   - Baud: PCLK2/8 = 110MHz/8 = 13.75 MHz
 */
#include "pwm_out.h"

/* 현재 DAC 코드 캐시 */
static uint16_t s_dac_code[NUM_CHANNELS] = {0};

/* ==========================================================
 * 내부 헬퍼: SPI1로 AD5641에 16비트 워드 전송
 * ========================================================== */
static void ad5641_write(uint8_t ch, uint16_t word)
{
    /* 빅엔디안 바이트 배열로 변환 (SPI MSB First) */
    uint8_t tx[2] = {
        (uint8_t)(word >> 8),
        (uint8_t)(word & 0xFF)
    };

    DAC_CS_LOW(ch);
    HAL_SPI_Transmit(&hspi1, tx, 2, 10U);  /* 10ms timeout */
    DAC_CS_HIGH(ch);
}

/* ==========================================================
 * PWMOut_Start
 *   초기화: 모든 채널 DAC 코드 0으로 출력
 * ========================================================== */
void PWMOut_Start(void)
{
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        s_dac_code[i] = 0U;
        /* AD5641 정상 동작 모드, 출력=0 */
        ad5641_write(i, AD5641_CMD_NORMAL);
    }
}

/* ==========================================================
 * PWMOut_Stop
 * ========================================================== */
void PWMOut_Stop(void)
{
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        PWMOut_Disable(i);
    }
}

/* ==========================================================
 * PWMOut_SetDuty
 *   duty (0.0~1.0) → DAC 코드 (0~16383) → SPI 전송
 * ========================================================== */
void PWMOut_SetDuty(uint8_t ch, float duty)
{
    if (ch >= NUM_CHANNELS) return;

    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    uint16_t code = (uint16_t)(duty * (float)AD5641_FULL_SCALE);
    PWMOut_SetCode(ch, code);
}

/* ==========================================================
 * PWMOut_SetCode
 *   DAC 코드 직접 설정 및 전송
 *   16비트 프레임: [PD1=0][PD0=0][D13..D0]
 * ========================================================== */
void PWMOut_SetCode(uint8_t ch, uint16_t code)
{
    if (ch >= NUM_CHANNELS) return;
    if (code > AD5641_FULL_SCALE) code = AD5641_FULL_SCALE;

    s_dac_code[ch] = code;

    /* AD5641_CMD_NORMAL(0x0000) | code(14비트) = 정상동작 */
    uint16_t word = AD5641_CMD_NORMAL | (code & 0x3FFFU);
    ad5641_write(ch, word);
}

/* ==========================================================
 * PWMOut_Disable
 * ========================================================== */
void PWMOut_Disable(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return;
    s_dac_code[ch] = 0U;
    ad5641_write(ch, AD5641_CMD_NORMAL);   /* code=0 */
}

/* ==========================================================
 * PWMOut_GetDuty
 * ========================================================== */
float PWMOut_GetDuty(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return 0.0f;
    return (float)s_dac_code[ch] / (float)AD5641_FULL_SCALE;
}
