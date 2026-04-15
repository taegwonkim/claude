/**
 * @file    mcp3465r.c
 * @brief   MCP3465R 16-bit Delta-Sigma ADC 드라이버 구현
 *
 * 동작 흐름
 * 1. Init: Full Reset → CONFIG0~3 설정 → 연속 변환 시작
 * 2. ReadChannel: MUX 레지스터 변경 → DR 핀(또는 폴링) 대기 → ADCDATA 읽기
 *
 * ※ DR(Data Ready) 인터럽트 핀 미사용. HAL_Delay로 변환 완료를 기다립니다.
 *    OSR=256, 내부 4.915MHz AMCLK 기준 변환 시간 ≒ 0.25ms → 1ms 대기로 충분.
 */
#include "mcp3465r.h"

/* ------------------------------------------------------------------ */
/*  내부 헬퍼                                                          */
/* ------------------------------------------------------------------ */

/** 커맨드 바이트 생성 매크로 */
#define CMD_BYTE(reg, cmd_type) \
    (uint8_t)(((MCP3465R_DEV_ADDR) << 6) | ((reg) << 2) | (cmd_type))

/**
 * @brief  1바이트 레지스터 쓰기 (Incremental Write)
 */
static HAL_StatusTypeDef write_reg8(MCP3465R_HandleTypeDef *hdev,
                                     uint8_t reg,
                                     uint8_t value)
{
    uint8_t buf[2] = { CMD_BYTE(reg, MCP3465R_CMD_INC_W), value };

    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef ret = HAL_SPI_Transmit(hdev->hspi, buf, 2U,
                                              MCP3465R_TIMEOUT_MS);
    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_SET);
    return ret;
}

/**
 * @brief  Fast 커맨드 전송 (1바이트)
 */
static HAL_StatusTypeDef fast_cmd(MCP3465R_HandleTypeDef *hdev, uint8_t cmd)
{
    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef ret = HAL_SPI_Transmit(hdev->hspi, &cmd, 1U,
                                              MCP3465R_TIMEOUT_MS);
    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_SET);
    return ret;
}

/**
 * @brief  ADCDATA 레지스터 32비트 읽기 (Static Read)
 *
 * DATA_FORMAT=11 (CONFIG3[5:4]) 설정 시:
 *   Byte0 : Status  [CHID3:CHID0 | DR | ~DR | OR | ~OR]
 *   Byte1 : SGN확장 [B31:B24]
 *   Byte2 : B23:B16
 *   Byte3 : B15:B8
 *   Byte4 : B7:B0
 *   → 총 5바이트 (커맨드 1 + 데이터 4 = 5)
 *
 * 실제로는 커맨드 바이트 이후 4바이트 데이터 수신.
 */
static HAL_StatusTypeDef read_adcdata(MCP3465R_HandleTypeDef *hdev,
                                       int32_t *raw_out,
                                       uint8_t *ch_id_out)
{
    uint8_t tx[5] = { CMD_BYTE(MCP3465R_REG_ADCDATA, MCP3465R_CMD_STATIC_R),
                      0x00U, 0x00U, 0x00U, 0x00U };
    uint8_t rx[5] = { 0 };

    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(hdev->hspi, tx, rx, 5U,
                                                    MCP3465R_TIMEOUT_MS);
    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_SET);

    if (ret != HAL_OK) return ret;

    /*
     * rx[0] = 커맨드에 대한 응답(STATUS byte)
     *   bit[7:4] = CHID
     *   bit[3]   = DR (데이터 준비 플래그, 1=준비)
     * rx[1..4] = 32-bit signed ADC 데이터 (MSB first)
     */
    if (ch_id_out)  *ch_id_out = (rx[0] >> 4) & 0x0FU;

    int32_t raw = ((int32_t)rx[1] << 24) |
                  ((int32_t)rx[2] << 16) |
                  ((int32_t)rx[3] <<  8) |
                   (int32_t)rx[4];
    *raw_out = raw;
    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  공개 API                                                           */
/* ------------------------------------------------------------------ */

HAL_StatusTypeDef MCP3465R_Init(MCP3465R_HandleTypeDef *hdev)
{
    HAL_StatusTypeDef ret;

    /* CS 비활성 */
    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_SET);

    /* 1. Full Reset */
    ret = fast_cmd(hdev, MCP3465R_FAST_RESET);
    if (ret != HAL_OK) return ret;
    HAL_Delay(1U);   /* 리셋 안정화 */

    /* 2. CONFIG0: 내부CLK, 외부Vref, 연속변환 */
    ret = write_reg8(hdev, MCP3465R_REG_CONFIG0, MCP3465R_CONFIG0_INT_CLK_CONT);
    if (ret != HAL_OK) return ret;

    /* 3. CONFIG1: PRE=1, OSR=256 */
    ret = write_reg8(hdev, MCP3465R_REG_CONFIG1, MCP3465R_CONFIG1_PRE1_OSR256);
    if (ret != HAL_OK) return ret;

    /* 4. CONFIG2: BOOST=1×, GAIN=1× */
    ret = write_reg8(hdev, MCP3465R_REG_CONFIG2, MCP3465R_CONFIG2_BOOST1_GAIN1);
    if (ret != HAL_OK) return ret;

    /* 5. CONFIG3: 연속변환, 32bit + CHID 포맷 */
    ret = write_reg8(hdev, MCP3465R_REG_CONFIG3, MCP3465R_CONFIG3_CONT_32BIT_CHID);
    if (ret != HAL_OK) return ret;

    /* 6. MUX: CH0 단일단으로 초기 설정 */
    ret = write_reg8(hdev, MCP3465R_REG_MUX, MCP3465R_MUX_SE(0U));
    if (ret != HAL_OK) return ret;

    /* 7. 변환 시작 */
    ret = fast_cmd(hdev, MCP3465R_FAST_ADC_START);
    if (ret != HAL_OK) return ret;

    HAL_Delay(2U);   /* 첫 변환 안정화 */
    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_HandleTypeDef *hdev,
                                        uint8_t  channel,
                                        float   *voltage)
{
    if (channel >= MCP3465R_NUM_CH) return HAL_ERROR;
    if (!voltage)                   return HAL_ERROR;

    HAL_StatusTypeDef ret;

    /* 1. MUX 채널 전환 */
    ret = write_reg8(hdev, MCP3465R_REG_MUX, MCP3465R_MUX_SE(channel));
    if (ret != HAL_OK) return ret;

    /* 2. 변환 완료 대기
     *    OSR=256 기준 변환 시간 ≒ 0.2ms → 넉넉히 2ms 대기
     *    (DR 핀 폴링을 사용하면 HAL_Delay를 제거하고 정확하게 대기 가능) */
    HAL_Delay(2U);

    /* 3. ADC 데이터 읽기 */
    int32_t raw   = 0;
    uint8_t ch_id = 0;
    ret = read_adcdata(hdev, &raw, &ch_id);
    if (ret != HAL_OK) return ret;

    /*
     * 단극성 단일단 입력(0V ~ Vref):
     *   raw 범위: 0x000000 ~ 0x7FFF00 (상위 16비트가 유효 ADC 데이터)
     *   유효 16비트 = raw >> 16  (DATA_FORMAT=11에서 부호확장된 32비트)
     *   Vin = (int16_t)(raw >> 16) / 32768.0 * Vref
     *
     * ※ 음수 코드는 0으로 클램프(오프셋 오차, 노이즈 대응)
     */
    int16_t adc16 = (int16_t)(raw >> 16);
    if (adc16 < 0) adc16 = 0;

    *voltage = ((float)adc16 / (float)MCP3465R_FULL_SCALE) * MCP3465R_VREF;
    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_HandleTypeDef *hdev,
                                            float voltages[MCP3465R_NUM_CH])
{
    HAL_StatusTypeDef overall = HAL_OK;

    for (uint8_t ch = 0; ch < MCP3465R_NUM_CH; ch++)
    {
        HAL_StatusTypeDef r = MCP3465R_ReadChannel(hdev, ch, &voltages[ch]);
        if (r != HAL_OK) overall = r;
    }
    return overall;
}
