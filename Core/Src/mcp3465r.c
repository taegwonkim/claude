/**
 * @file    mcp3465r.c
 * @brief   MCP3465R 24-bit Delta-Sigma ADC 드라이버 구현
 *
 * 동작 순서 (One-shot 모드):
 *   1. MCP3465R_Init()       — 레지스터 설정, 내부 Vref + 내부 클럭 활성화
 *   2. MCP3465R_SetMuxCh()   — MUX 레지스터에 채널 선택
 *   3. MCP3465R_StartConv()  — Fast ADC Start 커맨드
 *   4. MCP3465R_WaitReady()  — INT 핀 LOW 될 때까지 대기 (FreeRTOS osDelay)
 *   5. MCP3465R_ReadResult() — ADCDATA 32-bit 읽기, CH_ID + 부호 있는 코드 추출
 *   6. RawToVoltage()        — 전압 변환
 *
 * 전압 변환 공식 (단일 종단, Vref = 2.4V):
 *   V = raw / 2^23 * Vref
 *   (full-scale = Vref, raw 범위 -2^23 ~ +2^23-1)
 */
#include "mcp3465r.h"
#include "cmsis_os2.h"   /* osDelay */

/* ------------------------------------------------------------------ */
/*  SPI 헬퍼                                                            */
/* ------------------------------------------------------------------ */
static inline void cs_assert(MCP3465R_HandleTypeDef *hdev)
{
    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_RESET);
}

static inline void cs_deassert(MCP3465R_HandleTypeDef *hdev)
{
    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_SET);
}

/* ------------------------------------------------------------------ */
/*  초기화                                                               */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_HandleTypeDef *hdev,
                                 SPI_HandleTypeDef       *hspi,
                                 GPIO_TypeDef *cs_port,  uint16_t cs_pin,
                                 GPIO_TypeDef *int_port, uint16_t int_pin)
{
    if (!hdev || !hspi || !cs_port || !int_port) return HAL_ERROR;

    hdev->hspi          = hspi;
    hdev->cs_port       = cs_port;
    hdev->cs_pin        = cs_pin;
    hdev->int_port      = int_port;
    hdev->int_pin       = int_pin;
    hdev->vref          = MCP3465R_VREF_INTERNAL;
    hdev->conv_timeout_ms = 10U;

    for (uint8_t i = 0; i < MCP3465R_NUM_CHANNELS; i++) {
        hdev->raw_data[i]   = 0;
        hdev->data_valid[i] = false;
    }

    /* CS 초기 HIGH */
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    /* 1. 전체 리셋 */
    HAL_StatusTypeDef st = MCP3465R_FastCmd(hdev, MCP3465R_FAST_FULL_RESET);
    if (st != HAL_OK) return st;
    osDelay(2);  /* 리셋 안정화 대기 */

    /*
     * 2. CONFIG0~CONFIG3 + IRQ + MUX 연속 쓰기 (CONFIG0=0x01 부터 증분)
     *
     * CONFIG0 = 0xB2:
     *   [7]  VREF_SEL=1  → 내부 2.4V 기준 전압 활성화
     *   [5:4] CLK_SEL=11 → 내부 클럭, 핀 출력 없음
     *   [3:2] CS_SEL=00  → 전류 소스 없음
     *   [1:0] ADC_MODE=10 → 대기(Standby) 모드 초기화
     *
     * CONFIG1 = 0x14:
     *   [7:6] PRE=00  → AMCLK = MCLK (÷1)
     *   [5:2] OSR=0101 → OSR=1024 ≈ 변환 시간 0.5ms/채널
     *
     * CONFIG2 = 0x8B:
     *   [7:6] BOOST=10 → 전류 x1 (정상)
     *   [5:3] GAIN=001 → 게인 x1
     *   [2]   AZ_MUX=0 → Auto-zero MUX 비활성
     *   [1]   AZ_REF=1 → Auto-zero Vref 활성 (노이즈 감소)
     *
     * CONFIG3 = 0x30:
     *   [7:6] CONV_MODE=00 → One-shot (완료 후 Standby)
     *   [5:4] DATA_FORMAT=11 → 32-bit, CH_ID 포함, 우측 정렬
     *   [3:2] CRC=00 → CRC 비활성
     *
     * IRQ = 0x0A:
     *   [3:2] IRQ_MODE=10 → 비활성=HIGH, 활성(DATA_RDY)=LOW
     *   [1]   EN_FASTCMD=1 → Fast command 허용
     *
     * MUX = 0x08:
     *   VIN+ = CH0, VIN- = AGND (초기값, 채널별로 재설정)
     */
    const uint8_t cfg[] = {
        0xB2U,  /* CONFIG0 */
        0x14U,  /* CONFIG1 */
        0x8BU,  /* CONFIG2 */
        0x30U,  /* CONFIG3 */
        0x0AU,  /* IRQ     */
        0x08U,  /* MUX     (CH0 SE) */
    };
    st = MCP3465R_WriteRegs(hdev, MCP3465R_REG_CONFIG0, cfg, sizeof(cfg));
    return st;
}

/* ------------------------------------------------------------------ */
/*  Fast command                                                        */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_FastCmd(MCP3465R_HandleTypeDef *hdev, uint8_t cmd)
{
    cs_assert(hdev);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(hdev->hspi, &cmd, 1U, HAL_MAX_DELAY);
    cs_deassert(hdev);
    return st;
}

/* ------------------------------------------------------------------ */
/*  레지스터 쓰기                                                         */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_WriteReg8(MCP3465R_HandleTypeDef *hdev,
                                      uint8_t reg, uint8_t value)
{
    return MCP3465R_WriteRegs(hdev, reg, &value, 1U);
}

HAL_StatusTypeDef MCP3465R_WriteRegs(MCP3465R_HandleTypeDef *hdev,
                                      uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t cmd = MCP3465R_CMD_BYTE(reg, MCP3465R_CMD_INCR_WRITE);

    cs_assert(hdev);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(hdev->hspi, &cmd, 1U, HAL_MAX_DELAY);
    if (st == HAL_OK) {
        st = HAL_SPI_Transmit(hdev->hspi, (uint8_t *)data, len, HAL_MAX_DELAY);
    }
    cs_deassert(hdev);
    return st;
}

/* ------------------------------------------------------------------ */
/*  레지스터 읽기                                                         */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_ReadRegs(MCP3465R_HandleTypeDef *hdev,
                                     uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t cmd = MCP3465R_CMD_BYTE(reg, MCP3465R_CMD_INCR_READ);
    uint8_t dummy = 0x00U;

    cs_assert(hdev);
    /* 커맨드 바이트 전송 (상태 바이트 수신, 무시) */
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(hdev->hspi, &cmd, &dummy,
                                                    1U, HAL_MAX_DELAY);
    if (st == HAL_OK) {
        uint8_t tx_buf[16] = {0};
        st = HAL_SPI_TransmitReceive(hdev->hspi, tx_buf, data, len, HAL_MAX_DELAY);
    }
    cs_deassert(hdev);
    return st;
}

/* ------------------------------------------------------------------ */
/*  MUX 채널 선택                                                        */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_SetMuxChannel(MCP3465R_HandleTypeDef *hdev, uint8_t channel)
{
    if (channel >= MCP3465R_NUM_CHANNELS) return HAL_ERROR;
    uint8_t mux_val = MCP3465R_MUX_SE(channel);
    return MCP3465R_WriteReg8(hdev, MCP3465R_REG_MUX, mux_val);
}

/* ------------------------------------------------------------------ */
/*  변환 시작                                                            */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_StartConversion(MCP3465R_HandleTypeDef *hdev)
{
    return MCP3465R_FastCmd(hdev, MCP3465R_FAST_ADCSTART);
}

/* ------------------------------------------------------------------ */
/*  변환 완료 대기 (INT 핀 폴링, FreeRTOS osDelay 사용)                   */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_WaitReady(MCP3465R_HandleTypeDef *hdev, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(hdev->int_port, hdev->int_pin) == GPIO_PIN_SET) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            return HAL_TIMEOUT;
        }
        osDelay(1);  /* 1ms 대기 후 재확인 (다른 태스크에 CPU 양보) */
    }
    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  ADCDATA 읽기                                                         */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_ReadConversionResult(MCP3465R_HandleTypeDef *hdev,
                                                  uint8_t *ch_id, int32_t *raw_data)
{
    if (!raw_data) return HAL_ERROR;

    uint8_t rx[4] = {0};
    HAL_StatusTypeDef st = MCP3465R_ReadRegs(hdev, MCP3465R_REG_ADCDATA, rx, 4U);
    if (st != HAL_OK) return st;

    /*
     * DATA_FORMAT=11 (32-bit, CH_ID 포함, 우측 정렬):
     *   Byte0[7:4] = CH_ID[3:0]
     *   Byte0[3:0] = 부호 확장(SGN_EXT)
     *   Byte1      = DATA[23:16]
     *   Byte2      = DATA[15:8]
     *   Byte3      = DATA[7:0]
     *
     * 부호 있는 24-bit 값 → 32-bit sign extension
     */
    if (ch_id) {
        *ch_id = (rx[0] >> 4) & 0x0FU;
    }

    /* 24-bit signed value (Byte1~3) */
    uint32_t raw_u = ((uint32_t)rx[1] << 16) |
                     ((uint32_t)rx[2] <<  8) |
                      (uint32_t)rx[3];

    /* 부호 확장: bit23이 1이면 상위 8비트를 0xFF로 채움 */
    if (raw_u & 0x800000UL) {
        *raw_data = (int32_t)(raw_u | 0xFF000000UL);
    } else {
        *raw_data = (int32_t)raw_u;
    }
    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  채널 단일 변환 (MUX → 시작 → 대기 → 읽기)                            */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_HandleTypeDef *hdev,
                                        uint8_t channel, float *voltage)
{
    if (channel >= MCP3465R_NUM_CHANNELS || !voltage) return HAL_ERROR;

    HAL_StatusTypeDef st;

    st = MCP3465R_SetMuxChannel(hdev, channel);
    if (st != HAL_OK) return st;

    st = MCP3465R_StartConversion(hdev);
    if (st != HAL_OK) return st;

    st = MCP3465R_WaitReady(hdev, hdev->conv_timeout_ms);
    if (st != HAL_OK) return st;

    int32_t raw = 0;
    st = MCP3465R_ReadConversionResult(hdev, NULL, &raw);
    if (st != HAL_OK) return st;

    hdev->raw_data[channel]   = raw;
    hdev->data_valid[channel] = true;
    *voltage = MCP3465R_RawToVoltage(hdev, raw);
    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  4채널 순차 변환                                                       */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_HandleTypeDef *hdev,
                                            float voltage[MCP3465R_NUM_CHANNELS])
{
    HAL_StatusTypeDef result = HAL_OK;
    for (uint8_t ch = 0; ch < MCP3465R_NUM_CHANNELS; ch++) {
        HAL_StatusTypeDef st = MCP3465R_ReadChannel(hdev, ch, &voltage[ch]);
        if (st != HAL_OK) {
            voltage[ch] = 0.0f;
            hdev->data_valid[ch] = false;
            result = st;  /* 마지막 오류 코드 반환, 다음 채널은 계속 시도 */
        }
    }
    return result;
}

/* ------------------------------------------------------------------ */
/*  전압 변환                                                             */
/* ------------------------------------------------------------------ */
float MCP3465R_RawToVoltage(MCP3465R_HandleTypeDef *hdev, int32_t raw)
{
    /*
     * 단일 종단 측정 (VIN+ vs AGND):
     *   CODE = VIN+ / Vref * 2^23  (양수 범위만 사용)
     *   VIN+ = CODE * Vref / 2^23
     */
    return ((float)raw / (float)MCP3465R_FULLSCALE_CODE) * hdev->vref;
}
