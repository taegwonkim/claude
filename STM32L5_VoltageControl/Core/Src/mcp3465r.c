/* =========================================================
 * MCP3465R ADC 드라이버 구현
 * =========================================================*/
#include "mcp3465r.h"
#include "cmsis_os2.h"
#include <string.h>

/* ----- 내부 함수 프로토타입 ----- */
static void     _CS_Assert(void);
static void     _CS_Deassert(void);
static bool     _SPI_Transfer(uint8_t *tx, uint8_t *rx, uint8_t len);

/* =========================================================
 * CS 핀 제어
 * =========================================================*/
static void _CS_Assert(void)
{
    HAL_GPIO_WritePin(MCP3465R_CS_GPIO, MCP3465R_CS_PIN, GPIO_PIN_RESET);
}

static void _CS_Deassert(void)
{
    HAL_GPIO_WritePin(MCP3465R_CS_GPIO, MCP3465R_CS_PIN, GPIO_PIN_SET);
}

/* =========================================================
 * SPI 데이터 송수신
 * =========================================================*/
static bool _SPI_Transfer(uint8_t *tx, uint8_t *rx, uint8_t len)
{
    HAL_StatusTypeDef status;

    if (rx == NULL) {
        /* 쓰기만 */
        status = HAL_SPI_Transmit(MCP3465R_SPI, tx, len, MCP3465R_TIMEOUT_MS);
    } else if (tx == NULL) {
        /* 읽기만 */
        status = HAL_SPI_Receive(MCP3465R_SPI, rx, len, MCP3465R_TIMEOUT_MS);
    } else {
        /* 동시 송수신 */
        status = HAL_SPI_TransmitReceive(MCP3465R_SPI, tx, rx, len, MCP3465R_TIMEOUT_MS);
    }

    return (status == HAL_OK);
}

/* =========================================================
 * 초기화
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hdev)
{
    memset(hdev, 0, sizeof(MCP3465R_Handle_t));

    /* CS 비활성화 */
    _CS_Deassert();

    /* 하드웨어 리셋 */
    if (MCP3465R_Reset(hdev) != HAL_OK) {
        return HAL_ERROR;
    }
    osDelay(10); /* 리셋 후 안정화 대기 */

    /* ----- CONFIG0 설정 -----
     * 외부 VREF 사용
     * 내부 클럭 (AMCLK)
     * 전류원 없음
     * 연속 변환 모드
     */
    hdev->config0 = MCP3465R_CFG0_VREF_EXT
                  | MCP3465R_CFG0_CLK_INT
                  | MCP3465R_CFG0_CS_NONE
                  | MCP3465R_CFG0_MODE_CONV;

    /* ----- CONFIG1 설정 -----
     * 프리스케일러: /1
     * 오버샘플링: 1024 (적당한 속도/노이즈 균형)
     */
    hdev->config1 = MCP3465R_CFG1_PRE_1
                  | MCP3465R_CFG1_OSR_1024;

    /* ----- CONFIG2 설정 -----
     * 부스트: 정상 (1x)
     * 게인: 1 (입력 범위 = VREF)
     * 자동 제로 MUX: 활성화
     */
    hdev->config2 = MCP3465R_CFG2_BOOST_1
                  | MCP3465R_CFG2_GAIN_1
                  | MCP3465R_CFG2_AZ_MUX_EN;

    /* ----- CONFIG3 설정 -----
     * 연속 변환
     * 32비트 결과 (채널 ID + 부호 포함)
     */
    hdev->config3 = MCP3465R_CFG3_CONV_CONT
                  | MCP3465R_CFG3_DATA_32_CHID;

    /* CONFIG 레지스터 일괄 쓰기 (Incremental Write) */
    uint8_t cfg_data[4] = {
        hdev->config0,
        hdev->config1,
        hdev->config2,
        hdev->config3,
    };

    if (MCP3465R_WriteReg(hdev, MCP3465R_REG_CONFIG0, cfg_data, 4) != HAL_OK) {
        return HAL_ERROR;
    }

    /* ----- IRQ 레지스터: IRQ 핀 활성화, DATA_READY IRQ 활성화 ----- */
    uint8_t irq_cfg = 0x07; /* bit2=IRQ핀 활성화, bit1=DATA_READY, bit0=CRC 오류 */
    if (MCP3465R_WriteReg(hdev, MCP3465R_REG_IRQ, &irq_cfg, 1) != HAL_OK) {
        return HAL_ERROR;
    }

    /* ----- SCAN 모드 설정 (CH0~CH7 순차 스캔) ----- */
    /* 전압 측정: CH0~CH3 (단일종단, 기준=AGND)
     * 전류 측정: CH4~CH7 (단일종단, 기준=AGND) */
    uint32_t scan_cfg = MCP3465R_SCAN_CH0
                      | MCP3465R_SCAN_CH1
                      | MCP3465R_SCAN_CH2
                      | MCP3465R_SCAN_CH3
                      | MCP3465R_SCAN_CH4
                      | MCP3465R_SCAN_CH5
                      | MCP3465R_SCAN_CH6
                      | MCP3465R_SCAN_CH7
                      | MCP3465R_SCAN_DLY_8; /* 8 DMCLK 지연 */

    uint8_t scan_data[3] = {
        (scan_cfg >> 16) & 0xFF,
        (scan_cfg >> 8)  & 0xFF,
        (scan_cfg)       & 0xFF,
    };

    if (MCP3465R_WriteReg(hdev, MCP3465R_REG_SCAN, scan_data, 3) != HAL_OK) {
        return HAL_ERROR;
    }

    /* ----- 타이머: 스캔 간 지연 없음 ----- */
    uint8_t timer_data[3] = {0x00, 0x00, 0x00};
    if (MCP3465R_WriteReg(hdev, MCP3465R_REG_TIMER, timer_data, 3) != HAL_OK) {
        return HAL_ERROR;
    }

    /* 변환 시작 */
    if (MCP3465R_StartConversion(hdev) != HAL_OK) {
        return HAL_ERROR;
    }

    /* 초기 채널 설정 */
    hdev->current_channel = 0;

    /* 결과 초기화 */
    for (int i = 0; i < MCP3465R_TOTAL_CH; i++) {
        hdev->results[i].channel  = i;
        hdev->results[i].raw_code = 0;
        hdev->results[i].voltage_mv = 0;
        hdev->results[i].is_valid   = false;
    }

    hdev->initialized = true;
    return HAL_OK;
}

/* =========================================================
 * 소프트웨어 리셋 (Fast Command)
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_Reset(MCP3465R_Handle_t *hdev)
{
    uint8_t cmd = MCP3465R_FCMD_RESET;
    _CS_Assert();
    bool ok = _SPI_Transfer(&cmd, NULL, 1);
    _CS_Deassert();
    return ok ? HAL_OK : HAL_ERROR;
}

/* =========================================================
 * 레지스터 쓰기 (Incremental Write)
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hdev,
                                     uint8_t reg, uint8_t *data, uint8_t len)
{
    /* 커맨드 바이트: DevAddr + RegAddr + Incremental Write (0x02) */
    uint8_t cmd = MCP3465R_CMD(MCP3465R_DEV_ADDR, reg, MCP3465R_CMD_INCR_WR);
    uint8_t tx_buf[1 + 4]; /* 커맨드 + 최대 4바이트 */

    if (len > 4) return HAL_ERROR;

    tx_buf[0] = cmd;
    for (int i = 0; i < len; i++) {
        tx_buf[1 + i] = data[i];
    }

    _CS_Assert();
    bool ok = _SPI_Transfer(tx_buf, NULL, 1 + len);
    _CS_Deassert();

    return ok ? HAL_OK : HAL_ERROR;
}

/* =========================================================
 * 레지스터 읽기 (Static Read)
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *hdev,
                                    uint8_t reg, uint8_t *data, uint8_t len)
{
    /* 커맨드 바이트: DevAddr + RegAddr + Static Read (0x01) */
    uint8_t cmd = MCP3465R_CMD(MCP3465R_DEV_ADDR, reg, MCP3465R_CMD_STATIC_RD);

    _CS_Assert();
    /* 커맨드 전송 */
    _SPI_Transfer(&cmd, NULL, 1);
    /* 데이터 수신 */
    bool ok = _SPI_Transfer(NULL, data, len);
    _CS_Deassert();

    return ok ? HAL_OK : HAL_ERROR;
}

/* =========================================================
 * MUX 채널 설정 (단일 채널 변환 시 사용)
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_SetChannel(MCP3465R_Handle_t *hdev,
                                       uint8_t ch_pos, uint8_t ch_neg)
{
    uint8_t mux = MCP3465R_MUX(ch_pos, ch_neg);
    return MCP3465R_WriteReg(hdev, MCP3465R_REG_MUX, &mux, 1);
}

/* =========================================================
 * 변환 시작 (Fast Command)
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_StartConversion(MCP3465R_Handle_t *hdev)
{
    uint8_t cmd = MCP3465R_FCMD_CONV;
    _CS_Assert();
    bool ok = _SPI_Transfer(&cmd, NULL, 1);
    _CS_Deassert();
    return ok ? HAL_OK : HAL_ERROR;
}

/* =========================================================
 * IRQ 핀으로 데이터 준비 여부 확인
 * =========================================================*/
bool MCP3465R_IsDataReady(MCP3465R_Handle_t *hdev)
{
    /* IRQ 핀: 액티브 LOW → LOW일 때 데이터 준비됨 */
    return (HAL_GPIO_ReadPin(MCP3465R_IRQ_GPIO, MCP3465R_IRQ_PIN) == GPIO_PIN_RESET);
}

/* =========================================================
 * 변환 결과 읽기 (32비트, 채널 ID 포함)
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_ReadConvResult(MCP3465R_Handle_t *hdev,
                                           uint8_t channel,
                                           MCP3465R_Result_t *result)
{
    uint8_t rx_data[4] = {0};
    uint8_t status_byte;
    uint8_t cmd = MCP3465R_CMD(MCP3465R_DEV_ADDR, MCP3465R_REG_ADCDATA, MCP3465R_CMD_STATIC_RD);

    _CS_Assert();

    /* 커맨드 + 상태 바이트 수신 */
    uint8_t tx_cmd[1] = {cmd};
    uint8_t rx_status[1] = {0};
    _SPI_Transfer(tx_cmd, rx_status, 1);
    status_byte = rx_status[0];

    /* 4바이트 데이터 수신 (32비트: [CHID:4][SGN:4][DATA:24]) */
    bool ok = _SPI_Transfer(NULL, rx_data, 4);

    _CS_Deassert();

    if (!ok) return HAL_ERROR;

    /* 채널 ID 추출 (상위 4비트) */
    uint8_t ch_id = (rx_data[0] >> 4) & 0x0F;
    result->channel = ch_id;

    /* 24비트 ADC 코드 추출 (부호 있는 23비트 + 1비트 오버레인지) */
    uint32_t raw_unsigned = ((uint32_t)(rx_data[0] & 0x01) << 23)
                           | ((uint32_t)rx_data[1] << 16)
                           | ((uint32_t)rx_data[2] << 8)
                           |  (uint32_t)rx_data[3];

    /* 부호 확장 (24비트 → 32비트 부호 있는 정수) */
    if (raw_unsigned & 0x800000) {
        result->raw_code = (int32_t)(raw_unsigned | 0xFF000000);
    } else {
        result->raw_code = (int32_t)raw_unsigned;
    }

    /* mV 변환 */
    result->voltage_mv = MCP3465R_CodeToMillivolts(result->raw_code, MCP3465R_VREF_MV);
    result->is_valid   = true;

    /* 전역 결과 저장 */
    if (ch_id < MCP3465R_TOTAL_CH) {
        hdev->results[ch_id] = *result;
    }

    (void)status_byte; /* 필요 시 상태 바이트 파싱 가능 */
    return HAL_OK;
}

/* =========================================================
 * 전체 채널 스캔 읽기
 * SCAN 모드에서 IRQ를 기다리며 8채널 순차 읽기
 * =========================================================*/
HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_Handle_t *hdev)
{
    if (!hdev->initialized) return HAL_ERROR;

    MCP3465R_Result_t result;
    HAL_StatusTypeDef ret = HAL_OK;

    /* SCAN 모드: IRQ마다 채널 순서대로 결과 출력
     * 8채널 모두 읽음 (SCAN이 자동 순환) */
    for (int i = 0; i < MCP3465R_TOTAL_CH; i++) {
        /* IRQ 대기 (최대 200ms) */
        uint32_t tick = HAL_GetTick();
        while (!MCP3465R_IsDataReady(hdev)) {
            if ((HAL_GetTick() - tick) > 200) {
                return HAL_TIMEOUT;
            }
            osDelay(1);
        }

        if (MCP3465R_ReadConvResult(hdev, i, &result) != HAL_OK) {
            ret = HAL_ERROR;
        }
    }

    return ret;
}

/* =========================================================
 * ADC 코드 → 전압 (mV) 변환
 * 공식: V = (raw_code / 2^23) × VREF
 * =========================================================*/
int32_t MCP3465R_CodeToMillivolts(int32_t raw_code, int32_t vref_mv)
{
    /* 오버레인지 클리핑 */
    if (raw_code >  (int32_t)MCP3465R_MAX_CODE)  raw_code =  (int32_t)MCP3465R_MAX_CODE;
    if (raw_code < -(int32_t)MCP3465R_MAX_CODE)  raw_code = -(int32_t)MCP3465R_MAX_CODE;

    /* 정수 연산으로 mV 계산 (64비트 중간 결과로 오버플로우 방지) */
    int64_t mv = ((int64_t)raw_code * vref_mv) / ((int64_t)MCP3465R_MAX_CODE + 1);

    return (int32_t)mv;
}
