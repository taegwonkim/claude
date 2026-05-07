/*
 * mcp3465r_example.c – Usage example for mcp3465r driver on STM32L5
 *
 * Hardware wiring (example):
 *   MCP3465R SCK  → SPI1_SCK  (PA5)
 *   MCP3465R SDI  → SPI1_MOSI (PA7)
 *   MCP3465R SDO  → SPI1_MISO (PA6)
 *   MCP3465R CS   → GPIO      (PA4, active low)
 *   MCP3465R MCLK → leave unconnected if using external crystal, or tie to MCU timer
 *
 * CubeMX SPI1 settings:
 *   Mode: Full-Duplex Master
 *   Data size: 8 bits, MSB first
 *   CPOL: Low (0), CPHA: 1 Edge (Mode 0,0)
 *   Baud rate: ≤ 20 MHz
 */

#include "mcp3465r.h"

/* Declared by CubeMX */
extern SPI_HandleTypeDef hspi1;

/*
 * CH0: 단일 종단(single-ended), 고정밀 (OSR 4096, Gain 1x)
 *   → 느린 신호, 노이즈 최소화 (예: 온도 센서, 로드셀)
 *
 * CH1: 단일 종단(single-ended), 고속 (OSR 256, Gain 4x)
 *   → 빠른 신호, 증폭 필요 (예: 소형 신호 센서, 전류 감지)
 */
static const MCP3465R_ChConfig ch0_cfg = {
    .vin_pos = MCP3465R_MUX_CH0,
    .vin_neg = MCP3465R_MUX_AGND,
    .osr     = MCP3465R_OSR_4096,
    .gain    = MCP3465R_GAIN_1,
};

static const MCP3465R_ChConfig ch1_cfg = {
    .vin_pos = MCP3465R_MUX_CH1,
    .vin_neg = MCP3465R_MUX_AGND,
    .osr     = MCP3465R_OSR_256,
    .gain    = MCP3465R_GAIN_4,
};

/* 차동(differential) 예시: CH2(+) - CH3(-), Gain 8x */
static const MCP3465R_ChConfig ch23_diff_cfg = {
    .vin_pos = MCP3465R_MUX_CH2,
    .vin_neg = MCP3465R_MUX_CH3,
    .osr     = MCP3465R_OSR_1024,
    .gain    = MCP3465R_GAIN_8,
};

void Example_MCP3465R(void)
{
    MCP3465R_Handle adc = {
        .hspi    = &hspi1,
        .cs_port = GPIOA,
        .cs_pin  = GPIO_PIN_4,
    };

    if (MCP3465R_Init(&adc) != HAL_OK) {
        /* 초기화 실패 처리 */
        return;
    }

    int32_t ch0_result, ch1_result, diff_result;

    for (;;) {
        /*
         * CH0 읽기: 변환 전에 OSR=4096, Gain=1x 로 재설정됨
         * 변환 시간 ≈ (OSR × 32 + 오버헤드) / AMCLK
         */
        if (MCP3465R_ReadChannel(&adc, &ch0_cfg, &ch0_result) == HAL_OK) {
            /* ch0_result: -8388608 ~ +8388607 (24-bit signed)
             * 전압 = (ch0_result / 8388608.0f) * VREF  (VREF = 2.4V 내부 기준)
             */
        }

        /*
         * CH1 읽기: 변환 전에 OSR=256, Gain=4x 로 재설정됨
         * CH0와 완전히 독립적인 설정 사용 가능
         */
        if (MCP3465R_ReadChannel(&adc, &ch1_cfg, &ch1_result) == HAL_OK) {
            /* ch1_result: Gain=4x 적용된 결과
             * 실제 입력 전압 = (ch1_result / 8388608.0f) * VREF / 4
             */
        }

        /* 차동 채널 읽기 */
        if (MCP3465R_ReadChannel(&adc, &ch23_diff_cfg, &diff_result) == HAL_OK) {
            /* diff_result = (CH2 - CH3) × 8 기준 */
        }

        HAL_Delay(10);
    }
}
