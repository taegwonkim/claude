/**
 * mcp3465r.c
 * MCP3465R 24-bit Delta-Sigma ADC SPI driver (polling mode)
 * Target: STM32L552 with STM32CubeIDE / STM32 HAL
 */

#include "mcp3465r.h"

/* ------------------------------------------------------------------ */
/*  내부 헬퍼: CS 제어                                                   */
/* ------------------------------------------------------------------ */
static inline void cs_low(MCP3465R_Handle_t *hdev)
{
    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_RESET);
}

static inline void cs_high(MCP3465R_Handle_t *hdev)
{
    HAL_GPIO_WritePin(hdev->cs_port, hdev->cs_pin, GPIO_PIN_SET);
}

/* ------------------------------------------------------------------ */
/*  내부 헬퍼: Fast Command (데이터 없음)                                  */
/* ------------------------------------------------------------------ */
static HAL_StatusTypeDef send_fast_cmd(MCP3465R_Handle_t *hdev, uint8_t cmd_byte)
{
    HAL_StatusTypeDef ret;
    uint8_t tx = cmd_byte;
    uint8_t rx;

    cs_low(hdev);
    ret = HAL_SPI_TransmitReceive(hdev->hspi, &tx, &rx, 1, MCP3465R_CONV_TIMEOUT_MS);
    cs_high(hdev);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_Init                                                       */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hdev)
{
    HAL_StatusTypeDef ret;

    /* CS 초기 상태: HIGH (비활성) */
    cs_high(hdev);
    HAL_Delay(1);

    /* 하드웨어 리셋 */
    ret = MCP3465R_Reset(hdev);
    if (ret != HAL_OK) return ret;
    HAL_Delay(5);

    /* CONFIG0: External VREF, external clock, standby */
    uint8_t cfg0 = MCP3465R_CONFIG0_VAL;
    ret = MCP3465R_WriteReg(hdev, MCP3465R_REG_CONFIG0, &cfg0, 1);
    if (ret != HAL_OK) return ret;

    /* CONFIG1: 일반 측정용 OSR (나중에 캘리브레이션 시 변경) */
    uint8_t cfg1 = MCP3465R_CONFIG1_OSR_NORMAL;
    ret = MCP3465R_WriteReg(hdev, MCP3465R_REG_CONFIG1, &cfg1, 1);
    if (ret != HAL_OK) return ret;

    /* CONFIG2: BOOST=1x, PGA=1x, AZ_MUX=1, AZ_REF=1 */
    uint8_t cfg2 = MCP3465R_CONFIG2_VAL;
    ret = MCP3465R_WriteReg(hdev, MCP3465R_REG_CONFIG2, &cfg2, 1);
    if (ret != HAL_OK) return ret;

    /* CONFIG3: One-shot mode, 32-bit data format, 보정 비활성화 */
    hdev->config3 = MCP3465R_CONFIG3_BASE;
    ret = MCP3465R_WriteReg(hdev, MCP3465R_REG_CONFIG3, &hdev->config3, 1);
    if (ret != HAL_OK) return ret;

    /* IRQ: 인터럽트 비활성화, Fast Command 허용 */
    uint8_t irq = 0x73U;  /* [6]=IRQ_MODE=1(MDAT), [4]=EN_FASTCMD=1, [2]=EN_STP=1 */
    ret = MCP3465R_WriteReg(hdev, MCP3465R_REG_IRQ, &irq, 1);
    if (ret != HAL_OK) return ret;

    /* MUX 초기값: CH0(+) / AGND(-) */
    ret = MCP3465R_SetMux(hdev, MCP3465R_MUX_CH0, MCP3465R_MUX_AGND);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_Reset                                                      */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_Reset(MCP3465R_Handle_t *hdev)
{
    return send_fast_cmd(hdev, MCP3465R_FASTCMD_RESET);
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_WriteReg                                                   */
/*  Incremental Write: 명령 바이트 → data[0..len-1]                      */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hdev, uint8_t reg,
                                     const uint8_t *data, uint8_t len)
{
    HAL_StatusTypeDef ret;
    uint8_t tx_buf[8];  /* 최대 레지스터 크기: 명령 1 + 데이터 4 */
    uint8_t rx_buf[8];

    if (len > 7U) return HAL_ERROR;

    tx_buf[0] = MCP3465R_MAKE_CMD(hdev->dev_addr, reg, MCP3465R_CMD_INC_WR);
    for (uint8_t i = 0; i < len; i++) {
        tx_buf[1U + i] = data[i];
    }

    cs_low(hdev);
    ret = HAL_SPI_TransmitReceive(hdev->hspi, tx_buf, rx_buf, (uint16_t)(1U + len),
                                   MCP3465R_CONV_TIMEOUT_MS);
    cs_high(hdev);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_ReadReg                                                    */
/*  Static Read: 명령 바이트 → status(수신 중) → data[0..len-1]           */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *hdev, uint8_t reg,
                                    uint8_t *data, uint8_t len)
{
    HAL_StatusTypeDef ret;
    uint8_t tx_buf[8];
    uint8_t rx_buf[8];

    if (len > 7U) return HAL_ERROR;

    tx_buf[0] = MCP3465R_MAKE_CMD(hdev->dev_addr, reg, MCP3465R_CMD_INC_RD);
    for (uint8_t i = 0; i < len; i++) {
        tx_buf[1U + i] = 0xFFU;
    }

    cs_low(hdev);
    ret = HAL_SPI_TransmitReceive(hdev->hspi, tx_buf, rx_buf, (uint16_t)(1U + len),
                                   MCP3465R_CONV_TIMEOUT_MS);
    cs_high(hdev);

    if (ret == HAL_OK) {
        /* rx_buf[0] = status byte (명령 전송 중 수신), rx_buf[1..] = 실제 데이터 */
        for (uint8_t i = 0; i < len; i++) {
            data[i] = rx_buf[1U + i];
        }
    }
    return ret;
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_SetMux                                                     */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_SetMux(MCP3465R_Handle_t *hdev, uint8_t vinp, uint8_t vinn)
{
    uint8_t mux = MCP3465R_MAKE_MUX(vinp, vinn);
    return MCP3465R_WriteReg(hdev, MCP3465R_REG_MUX, &mux, 1);
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_SetConfig3                                                 */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_SetConfig3(MCP3465R_Handle_t *hdev,
                                       uint8_t en_offcal, uint8_t en_gaincal)
{
    hdev->config3 = MCP3465R_CONFIG3_BASE;
    if (en_offcal)  hdev->config3 |= MCP3465R_CONFIG3_EN_OFFCAL;
    if (en_gaincal) hdev->config3 |= MCP3465R_CONFIG3_EN_GAINCAL;
    return MCP3465R_WriteReg(hdev, MCP3465R_REG_CONFIG3, &hdev->config3, 1);
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_SetOSR                                                     */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_SetOSR(MCP3465R_Handle_t *hdev, uint8_t config1_val)
{
    return MCP3465R_WriteReg(hdev, MCP3465R_REG_CONFIG1, &config1_val, 1);
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_StartConversion                                            */
/*  One-shot 변환 시작 (Fast Command)                                    */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_StartConversion(MCP3465R_Handle_t *hdev)
{
    uint8_t fast_cmd = MCP3465R_MAKE_CMD(hdev->dev_addr, 0xAU, MCP3465R_CMD_FAST_WR);
    /* 0xA = Start/Restart fast command address */
    return send_fast_cmd(hdev, fast_cmd);
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_WaitReady                                                  */
/*  IRQ/nDR 폴링: 상태 바이트의 DR 비트([2]) 확인                          */
/*                                                                      */
/*  MCP3465R SPI 응답: 명령 수신 중 MISO로 STATUS 바이트 출력              */
/*  STATUS[2] = DR (Data Ready, active-low)                             */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_WaitReady(MCP3465R_Handle_t *hdev, uint32_t timeout_ms)
{
    uint32_t t_start = HAL_GetTick();
    uint8_t tx[1] = { MCP3465R_MAKE_CMD(hdev->dev_addr, MCP3465R_REG_ADCDATA,
                                         MCP3465R_CMD_STATIC_RD) };
    uint8_t rx[1];

    while ((HAL_GetTick() - t_start) < timeout_ms) {
        cs_low(hdev);
        HAL_SPI_TransmitReceive(hdev->hspi, tx, rx, 1, 10U);
        cs_high(hdev);

        /* STATUS 바이트: bit[2]=DR_STATUS (0=데이터 준비됨) */
        if ((rx[0] & 0x04U) == 0U) {
            return HAL_OK;
        }
        HAL_Delay(1);
    }
    return HAL_TIMEOUT;
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_ReadData                                                   */
/*  변환 시작 → 준비 대기 → 32-bit 데이터 읽기                             */
/*                                                                      */
/*  DATA_FORMAT=11: [31:28]=SGN(4bit) [27:24]=CHID [23:0]=ADC data    */
/*  24-bit 부호 정수로 반환                                               */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_ReadData(MCP3465R_Handle_t *hdev, int32_t *data,
                                     uint32_t timeout_ms)
{
    HAL_StatusTypeDef ret;
    uint8_t raw[4];

    ret = MCP3465R_StartConversion(hdev);
    if (ret != HAL_OK) return ret;

    ret = MCP3465R_WaitReady(hdev, timeout_ms);
    if (ret != HAL_OK) return ret;

    /* ADCDATA 레지스터: 4바이트 (32-bit) 읽기 */
    ret = MCP3465R_ReadReg(hdev, MCP3465R_REG_ADCDATA, raw, 4);
    if (ret != HAL_OK) return ret;

    /*
     * DATA_FORMAT=11: byte[0]=[SGN(3bits)][CHID(4bits)][data_b23]
     *                 byte[1]=data[22:15], byte[2]=data[14:7], byte[3]=data[6:0]+0b0
     *
     * 24-bit 유효 데이터 추출: 하위 24비트 (byte[0]의 하위 1비트 + byte[1..3])
     * 부호 처리: byte[0]의 최상위 비트 = SGN
     */
    int32_t raw32 = ((int32_t)raw[0] << 24) |
                    ((int32_t)raw[1] << 16) |
                    ((int32_t)raw[2] <<  8) |
                    ((int32_t)raw[3]);

    /* CHID 제거 후 24-bit 부호 정수 추출:
     * raw32 상위 8비트: [7:4]=SGN×4, [3:0]=CHID → 부호 비트는 bit31
     * 하위 24비트가 ADC 결과, bit31로 부호 확장 */
    int32_t adc24 = raw32 & 0x00FFFFFFL;  /* 하위 24비트 */
    /* 부호 확장: bit23이 1이면 음수 */
    if (adc24 & 0x00800000L) {
        adc24 |= (int32_t)0xFF000000L;
    }

    *data = adc24;
    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_WriteOffsetCal                                             */
/*  OFFSETCAL: 24-bit 2의 보수, MSB first (3 bytes)                     */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_WriteOffsetCal(MCP3465R_Handle_t *hdev, int32_t offset_val)
{
    /* 24-bit 마스킹 */
    uint32_t v = (uint32_t)offset_val & 0x00FFFFFFU;
    uint8_t reg[3];
    reg[0] = (uint8_t)(v >> 16);
    reg[1] = (uint8_t)(v >>  8);
    reg[2] = (uint8_t)(v);
    return MCP3465R_WriteReg(hdev, MCP3465R_REG_OFFSETCAL, reg, 3);
}

/* ------------------------------------------------------------------ */
/*  MCP3465R_WriteGainCal                                               */
/*  GAINCAL: 24-bit unsigned, 0x800000 = 1.0, MSB first (3 bytes)      */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_WriteGainCal(MCP3465R_Handle_t *hdev, uint32_t gain_val)
{
    uint32_t v = gain_val & 0x00FFFFFFU;
    uint8_t reg[3];
    reg[0] = (uint8_t)(v >> 16);
    reg[1] = (uint8_t)(v >>  8);
    reg[2] = (uint8_t)(v);
    return MCP3465R_WriteReg(hdev, MCP3465R_REG_GAINCAL, reg, 3);
}
