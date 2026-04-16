/**
 * @file    mcp3465r.c
 * @brief   MCP3465R 16-bit Delta-Sigma ADC 드라이버 구현
 *
 * 동작 흐름
 * 1. Init: Full Reset → CONFIG0~3 설정 → 연속 변환 시작
 * 2. ReadChannel: MUX 레지스터 변경 → DR 핀(또는 폴링) 대기 → ADCDATA 읽기
 *
 * ※ DR 인터럽트 핀 미사용. STATUS byte의 DR bit 폴링 + HAL_Delay 대기.
 *    OSR=256, 내부 4.915MHz AMCLK 기준 변환 시간 ≒ 0.25ms.
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
                                       int16_t *adc_out,
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
     * SPI 수신 데이터 레이아웃 (DATA_FORMAT=11, 16-bit ADC):
     *
     * rx[0] = STATUS byte (커맨드 바이트 전송 중 슬레이브 출력)
     *   bit[7:4] = CHID (최근 변환된 채널 ID)
     *   bit[3]   = DR   (1=Data Ready)
     *
     * rx[1..4] = ADCDATA 레지스터 (32-bit, DATA_FORMAT=11):
     *   rx[1] bit[7:4] = CHID[3:0]    ← 채널 ID
     *   rx[1] bit[3:0] = SGN 확장
     *   rx[2]          = SGN 확장
     *   rx[3]          = D15:D8       ← ADC 데이터 상위
     *   rx[4]          = D7:D0        ← ADC 데이터 하위
     *
     * 16-bit ADC 유효 데이터 = rx[3]:rx[4]  (하위 16비트)
     * CHID = rx[1] >> 4
     */
    /* STATUS byte의 DR bit(bit[2])로 데이터 준비 확인
     * DR=0 → 새 데이터 있음 (active-low), DR=1 → 미준비 */
    if (rx[0] & 0x04U)
    {
        /* Data not ready — 이전 데이터가 아직 유효하지 않음.
         * 호출부에서 재시도 또는 대기 시간 증가 필요 */
        return HAL_BUSY;
    }

    if (ch_id_out)  *ch_id_out = (rx[1] >> 4) & 0x0FU;

    *adc_out = (int16_t)(((uint16_t)rx[3] << 8) | (uint16_t)rx[4]);
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

    /* 6. IRQ: IRQ핀 비활성 (Hi-Z), Fast command 활성 */
    ret = write_reg8(hdev, MCP3465R_REG_IRQ, MCP3465R_IRQ_DEFAULT);
    if (ret != HAL_OK) return ret;

    /* 7. MUX: CH0 단일단으로 초기 설정 */
    ret = write_reg8(hdev, MCP3465R_REG_MUX, MCP3465R_MUX_SE(0U));
    if (ret != HAL_OK) return ret;

    /* 8. 변환 시작 */
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

    /* 2. 변환 완료 대기 후 ADC 데이터 읽기
     *    OSR=256 기준 변환 시간 ≒ 0.2ms → 1ms 대기 후 시도
     *    STATUS DR 비트로 준비 여부를 확인하며 최대 3회 재시도 */
    int16_t adc16 = 0;
    uint8_t ch_id = 0;
    uint8_t retries = 3U;

    HAL_Delay(1U);
    do {
        ret = read_adcdata(hdev, &adc16, &ch_id);
        if (ret == HAL_OK) break;
        if (ret == HAL_BUSY)
        {
            HAL_Delay(1U);   /* DR not ready → 1ms 추가 대기 */
            retries--;
        }
        else
        {
            return ret;      /* SPI 통신 에러 */
        }
    } while (retries > 0U);

    if (ret != HAL_OK) return HAL_TIMEOUT;

    /*
     * 단극성 단일단 입력(0V ~ Vref):
     *   adc16 범위: 0x0000(0V) ~ 0x7FFF(Vref)
     *   Vin = adc16 / 32768.0 * Vref
     *
     * ※ 음수 코드는 0으로 클램프(오프셋 오차, 노이즈 대응)
     */
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
