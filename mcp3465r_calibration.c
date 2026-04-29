#include "mcp3465r_calibration.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * 내부 헬퍼: CS 제어
 * ----------------------------------------------------------------------- */
static inline void cs_low(MCP3465R_Handle_t *dev) {
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static inline void cs_high(MCP3465R_Handle_t *dev) {
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

/* -----------------------------------------------------------------------
 * 24-bit 레지스터 쓰기 (Incremental Write)
 *
 * SPI 프레임:
 *   [CMD byte] [Byte2(MSB)] [Byte1] [Byte0(LSB)]
 * ----------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3465R_WriteReg24(MCP3465R_Handle_t *dev, uint8_t reg, uint32_t val)
{
    uint8_t tx[4];
    tx[0] = MCP3465R_CMD_BYTE(dev->dev_addr, reg, MCP3465R_CMD_INC_WRITE);
    tx[1] = (val >> 16) & 0xFF;
    tx[2] = (val >>  8) & 0xFF;
    tx[3] = (val >>  0) & 0xFF;

    cs_low(dev);
    HAL_StatusTypeDef ret = HAL_SPI_Transmit(dev->hspi, tx, 4, HAL_MAX_DELAY);
    cs_high(dev);
    return ret;
}

/* -----------------------------------------------------------------------
 * 24-bit 레지스터 읽기 (Incremental Read)
 *
 * SPI 프레임:
 *   TX: [CMD byte] [0x00] [0x00] [0x00]
 *   RX: [STATUS  ] [B2  ] [B1  ] [B0  ]
 * ----------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3465R_ReadReg24(MCP3465R_Handle_t *dev, uint8_t reg, uint32_t *val)
{
    uint8_t tx[4] = { MCP3465R_CMD_BYTE(dev->dev_addr, reg, MCP3465R_CMD_INC_READ),
                      0x00, 0x00, 0x00 };
    uint8_t rx[4] = {0};

    cs_low(dev);
    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 4, HAL_MAX_DELAY);
    cs_high(dev);

    if (ret == HAL_OK) {
        /* rx[0]은 STATUS byte, rx[1..3]이 실제 데이터 */
        *val = ((uint32_t)rx[1] << 16) |
               ((uint32_t)rx[2] <<  8) |
               ((uint32_t)rx[3] <<  0);
    }
    return ret;
}

/* -----------------------------------------------------------------------
 * ADC 단일 변환 결과 읽기 (24-bit, 2's complement 부호 확장)
 *
 * CONFIG3의 DATA_FORMAT = 00b (24-bit, signed) 가정
 * ADCDATA 레지스터는 Static Read로 3바이트 읽음
 * ----------------------------------------------------------------------- */
int32_t MCP3465R_ReadADC_Single(MCP3465R_Handle_t *dev)
{
    uint32_t raw = 0;

    /* ADCDATA Static Read: 변환 완료까지 IRQ/nDRDY 폴링 필요 (여기선 간략화) */
    MCP3465R_ReadReg24(dev, MCP3465R_REG_ADCDATA, &raw);

    /* 24-bit → int32_t 부호 확장 */
    if (raw & 0x800000U) {
        return (int32_t)(raw | 0xFF000000U);
    }
    return (int32_t)raw;
}

/* -----------------------------------------------------------------------
 * n회 평균 ADC 읽기 (노이즈 제거)
 * ----------------------------------------------------------------------- */
int32_t MCP3465R_ReadADC_Average(MCP3465R_Handle_t *dev, uint16_t n)
{
    int64_t sum = 0;
    for (uint16_t i = 0; i < n; i++) {
        sum += MCP3465R_ReadADC_Single(dev);
        HAL_Delay(1);
    }
    return (int32_t)(sum / n);
}

/* -----------------------------------------------------------------------
 * STEP 1: Offset Calibration
 *
 * 외부에서 V_OFFSET(0.5V)를 ADC 입력에 인가한 상태에서 호출.
 *
 * 원리:
 *   ideal_code = (V_OFFSET / VREF) × 2^23
 *   offset_error = measured_code - ideal_code
 *   OFFSETCAL = -offset_error  (부호 반전하여 오차를 상쇄)
 *
 * OFFSETCAL 쓰기 전, 이전 교정값을 0으로 초기화해야 정확한 오차 측정 가능.
 * ----------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3465R_RunOffsetCal(MCP3465R_Handle_t *dev,
                                         MCP3465R_CalResult_t *cal)
{
    /* 기존 교정값 초기화 */
    MCP3465R_WriteReg24(dev, MCP3465R_REG_OFFSETCAL, 0x000000);
    MCP3465R_WriteReg24(dev, MCP3465R_REG_GAINCAL,   GAINCAL_UNITY);
    HAL_Delay(10);

    /* 평균 ADC 코드 측정 */
    int32_t measured = MCP3465R_ReadADC_Average(dev, CAL_SAMPLE_COUNT);

    /* 이상적인 ADC 코드 계산 */
    int32_t ideal = (int32_t)((CAL_V_OFFSET / CAL_VREF) * ADC_FULL_SCALE);

    /* 오프셋 오차 */
    int32_t offset_error = measured - ideal;

    /* OFFSETCAL = -offset_error (24-bit 2's complement) */
    cal->offset_code    = -offset_error;
    cal->offset_voltage = ((float)offset_error / ADC_FULL_SCALE) * CAL_VREF;

    return HAL_OK;
}

/* -----------------------------------------------------------------------
 * STEP 2: Gain Calibration
 *
 * 외부에서 V_GAIN(4.5V)를 ADC 입력에 인가한 상태에서 호출.
 * Offset 교정이 먼저 완료(적용)된 후 수행해야 정확함.
 *
 * 원리:
 *   ideal_span  = (V_GAIN - V_OFFSET) / VREF × 2^23
 *   actual_span = gain_measured - offset_measured  (교정 후 측정값)
 *   gain_ratio  = actual_span / ideal_span
 *   GAINCAL     = 0x800000 / gain_ratio
 *               = 0x800000 × ideal_span / actual_span
 * ----------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3465R_RunGainCal(MCP3465R_Handle_t *dev,
                                       MCP3465R_CalResult_t *cal)
{
    /* Offset 교정값 먼저 적용 */
    uint32_t offsetcal_reg = (uint32_t)(cal->offset_code & 0x00FFFFFFU);
    MCP3465R_WriteReg24(dev, MCP3465R_REG_OFFSETCAL, offsetcal_reg);
    MCP3465R_WriteReg24(dev, MCP3465R_REG_GAINCAL,   GAINCAL_UNITY);
    HAL_Delay(10);

    /* Gain 포인트 측정 */
    int32_t gain_measured = MCP3465R_ReadADC_Average(dev, CAL_SAMPLE_COUNT);

    /* 이상적인 gain 포인트 코드 */
    int32_t gain_ideal = (int32_t)((CAL_V_GAIN / CAL_VREF) * ADC_FULL_SCALE);

    /* Gain 비율 계산
     * gain_ratio = gain_measured / gain_ideal
     * GAINCAL = (int)(GAINCAL_UNITY / gain_ratio)
     *         = (int)(GAINCAL_UNITY * gain_ideal / gain_measured) */
    float gain_ratio = (float)gain_measured / (float)gain_ideal;
    uint32_t gaincal = (uint32_t)((float)GAINCAL_UNITY / gain_ratio);

    /* 24-bit 범위 클램프 */
    if (gaincal > 0xFFFFFFU) gaincal = 0xFFFFFFU;

    cal->gain_code  = gaincal;
    cal->gain_error = gain_ratio - 1.0f;

    return HAL_OK;
}

/* -----------------------------------------------------------------------
 * 교정값 레지스터에 최종 적용
 * ----------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3465R_ApplyCalibration(MCP3465R_Handle_t *dev,
                                             const MCP3465R_CalResult_t *cal)
{
    HAL_StatusTypeDef ret;

    /* OFFSETCAL: 24-bit 2's complement (음수는 상위 바이트 마스킹) */
    uint32_t offsetcal_reg = (uint32_t)(cal->offset_code & 0x00FFFFFFU);
    ret = MCP3465R_WriteReg24(dev, MCP3465R_REG_OFFSETCAL, offsetcal_reg);
    if (ret != HAL_OK) return ret;

    ret = MCP3465R_WriteReg24(dev, MCP3465R_REG_GAINCAL, cal->gain_code);
    return ret;
}

/* -----------------------------------------------------------------------
 * 전체 교정 시퀀스 (Offset → Gain 순서)
 *
 * 사용법:
 *   1. 함수 호출 전 0.5V를 ADC 입력에 연결
 *   2. MCP3465R_RunFullCal() 첫 번째 단계 실행 (Offset 측정)
 *   3. 함수 내부에서 사용자 프롬프트 대기 (또는 GPIO 신호)
 *   4. 4.5V로 전환 후 두 번째 단계 실행 (Gain 측정)
 *   5. 결과를 레지스터에 적용
 *
 * 여기서는 두 단계 사이에 HAL_Delay 대신 사용자가 직접 전압을 전환하도록
 * 단계를 분리하는 구조로 구현합니다. RunFullCal은 단일 호출 자동화 버전.
 * ----------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3465R_RunFullCal(MCP3465R_Handle_t *dev,
                                       MCP3465R_CalResult_t *cal)
{
    HAL_StatusTypeDef ret;

    /* --- STEP 1: Offset (0.5V 인가 상태 가정) --- */
    ret = MCP3465R_RunOffsetCal(dev, cal);
    if (ret != HAL_OK) return ret;

    /* 실제 시스템에서는 여기서 신호/UART로 사용자에게 4.5V 전환 요청 후
     * 버튼 입력 또는 UART 수신까지 대기합니다.
     * 예: while (HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_RESET) {} */

    /* --- STEP 2: Gain (4.5V 인가 상태 가정) --- */
    ret = MCP3465R_RunGainCal(dev, cal);
    if (ret != HAL_OK) return ret;

    /* --- STEP 3: 레지스터 적용 --- */
    ret = MCP3465R_ApplyCalibration(dev, cal);
    return ret;
}
