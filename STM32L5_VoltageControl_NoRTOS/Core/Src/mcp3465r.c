/* =========================================================
 * MCP3465R 드라이버 - Non-RTOS (폴링 + HAL_Delay)
 * =========================================================*/
#include "mcp3465r.h"
#include <string.h>

static void _CS_Assert(void)   { HAL_GPIO_WritePin(MCP3465R_CS_GPIO, MCP3465R_CS_PIN, GPIO_PIN_RESET); }
static void _CS_Deassert(void) { HAL_GPIO_WritePin(MCP3465R_CS_GPIO, MCP3465R_CS_PIN, GPIO_PIN_SET);   }

static bool _SPI_Transfer(uint8_t *tx, uint8_t *rx, uint8_t len)
{
    HAL_StatusTypeDef s;
    if      (rx == NULL) s = HAL_SPI_Transmit(MCP3465R_SPI, tx, len, MCP3465R_TIMEOUT_MS);
    else if (tx == NULL) s = HAL_SPI_Receive (MCP3465R_SPI, rx, len, MCP3465R_TIMEOUT_MS);
    else                 s = HAL_SPI_TransmitReceive(MCP3465R_SPI, tx, rx, len, MCP3465R_TIMEOUT_MS);
    return (s == HAL_OK);
}

/* IRQ 핀 상태 확인 */
bool MCP3465R_IsDataReady(void)
{
    return (HAL_GPIO_ReadPin(MCP3465R_IRQ_GPIO, MCP3465R_IRQ_PIN) == GPIO_PIN_RESET);
}

HAL_StatusTypeDef MCP3465R_Reset(MCP3465R_Handle_t *hdev)
{
    uint8_t cmd = MCP3465R_FCMD_RESET;
    _CS_Assert();
    bool ok = _SPI_Transfer(&cmd, NULL, 1);
    _CS_Deassert();
    return ok ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hdev,
                                     uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t tx[5];
    if (len > 4) return HAL_ERROR;
    tx[0] = MCP3465R_CMD(MCP3465R_DEV_ADDR, reg, MCP3465R_CMD_INCR_WR);
    for (int i = 0; i < len; i++) tx[1+i] = data[i];

    _CS_Assert();
    bool ok = _SPI_Transfer(tx, NULL, 1 + len);
    _CS_Deassert();
    return ok ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef MCP3465R_StartConversion(MCP3465R_Handle_t *hdev)
{
    uint8_t cmd = MCP3465R_FCMD_CONV;
    _CS_Assert();
    bool ok = _SPI_Transfer(&cmd, NULL, 1);
    _CS_Deassert();
    return ok ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hdev)
{
    memset(hdev, 0, sizeof(MCP3465R_Handle_t));
    _CS_Deassert();

    if (MCP3465R_Reset(hdev) != HAL_OK) return HAL_ERROR;
    HAL_Delay(10);

    uint8_t cfg[4] = {
        MCP3465R_CFG0_VREF_EXT | MCP3465R_CFG0_CLK_INT |
        MCP3465R_CFG0_CS_NONE  | MCP3465R_CFG0_MODE_CONV,
        MCP3465R_CFG1_PRE_1    | MCP3465R_CFG1_OSR_1024,
        MCP3465R_CFG2_BOOST_1  | MCP3465R_CFG2_GAIN_1 | MCP3465R_CFG2_AZ_MUX_EN,
        MCP3465R_CFG3_CONV_CONT| MCP3465R_CFG3_DATA_32_CHID,
    };
    if (MCP3465R_WriteReg(hdev, MCP3465R_REG_CONFIG0, cfg, 4) != HAL_OK) return HAL_ERROR;

    /* IRQ 핀 활성화 */
    uint8_t irq_cfg = 0x07;
    if (MCP3465R_WriteReg(hdev, MCP3465R_REG_IRQ, &irq_cfg, 1) != HAL_OK) return HAL_ERROR;

    /* SCAN: CH0~CH7 */
    uint32_t scan = MCP3465R_SCAN_CH0|MCP3465R_SCAN_CH1|MCP3465R_SCAN_CH2|MCP3465R_SCAN_CH3
                  | MCP3465R_SCAN_CH4|MCP3465R_SCAN_CH5|MCP3465R_SCAN_CH6|MCP3465R_SCAN_CH7
                  | MCP3465R_SCAN_DLY_8;
    uint8_t scan_d[3] = {(scan>>16)&0xFF, (scan>>8)&0xFF, scan&0xFF};
    if (MCP3465R_WriteReg(hdev, MCP3465R_REG_SCAN, scan_d, 3) != HAL_OK) return HAL_ERROR;

    uint8_t timer_d[3] = {0,0,0};
    MCP3465R_WriteReg(hdev, MCP3465R_REG_TIMER, timer_d, 3);

    if (MCP3465R_StartConversion(hdev) != HAL_OK) return HAL_ERROR;

    hdev->initialized = true;
    return HAL_OK;
}

/* =========================================================
 * 단일 채널 결과 읽기 (32비트, 채널ID 포함)
 * SCAN 모드: IRQ마다 다음 채널 순서로 자동 전환
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_ReadOneResult(MCP3465R_Handle_t *hdev)
{
    uint8_t rx[4] = {0};
    uint8_t cmd   = MCP3465R_CMD(MCP3465R_DEV_ADDR, MCP3465R_REG_ADCDATA, MCP3465R_CMD_STATIC_RD);

    _CS_Assert();
    uint8_t tx1[1] = {cmd};
    uint8_t rx1[1] = {0};
    _SPI_Transfer(tx1, rx1, 1);            /* 커맨드 + 상태 바이트 */
    bool ok = _SPI_Transfer(NULL, rx, 4); /* 32비트 데이터 */
    _CS_Deassert();

    if (!ok) return HAL_ERROR;

    uint8_t ch_id = (rx[0] >> 4) & 0x0F;
    if (ch_id >= MCP3465R_TOTAL_CH) return HAL_ERROR;

    uint32_t raw_u = ((uint32_t)(rx[0]&0x01)<<23)
                   | ((uint32_t)rx[1]<<16)
                   | ((uint32_t)rx[2]<<8)
                   |  (uint32_t)rx[3];

    int32_t raw = (raw_u & 0x800000) ? (int32_t)(raw_u|0xFF000000)
                                     : (int32_t)raw_u;

    hdev->results[ch_id].channel    = ch_id;
    hdev->results[ch_id].raw_code   = raw;
    hdev->results[ch_id].voltage_mv = MCP3465R_CodeToMillivolts(raw, MCP3465R_VREF_MV);
    hdev->results[ch_id].is_valid   = true;

    return HAL_OK;
}

/* =========================================================
 * 8채널 전부 폴링 읽기 (TIM7 트리거 시 사용)
 * 각 채널마다 IRQ LOW 대기 → 읽기
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_ReadAllBlocking(MCP3465R_Handle_t *hdev)
{
    if (!hdev->initialized) return HAL_ERROR;

    for (int i = 0; i < MCP3465R_TOTAL_CH; i++) {
        uint32_t tick = HAL_GetTick();
        while (!MCP3465R_IsDataReady()) {
            if ((HAL_GetTick() - tick) > 200) return HAL_TIMEOUT;
        }
        if (MCP3465R_ReadOneResult(hdev) != HAL_OK) return HAL_ERROR;
    }
    return HAL_OK;
}

int32_t MCP3465R_CodeToMillivolts(int32_t raw, int32_t vref_mv)
{
    if (raw >  (int32_t)MCP3465R_MAX_CODE) raw =  (int32_t)MCP3465R_MAX_CODE;
    if (raw < -(int32_t)MCP3465R_MAX_CODE) raw = -(int32_t)MCP3465R_MAX_CODE;
    return (int32_t)(((int64_t)raw * vref_mv) / ((int64_t)MCP3465R_MAX_CODE + 1));
}
