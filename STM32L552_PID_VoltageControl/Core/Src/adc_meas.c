/* adc_meas.c - MCP3465R 외부 ADC 구현 (SPI2)
 *
 * ============================================================
 * MCP3465R 레지스터 설정 요약
 * ============================================================
 * CONFIG0  = 0x23   내부 OSC, CS 없음, 연속 변환
 * CONFIG1  = 0x00   PRE=1, OSR=32 (빠른 스캔)
 * CONFIG2  = 0xC8   BOOST=1x, GAIN=1x, AZ 비활성
 * CONFIG3  = 0xE0   Continuous Scan, DATA_FORMAT=10(32bit+CHID)
 * IRQ      = 0x00   /IRQ 활성 LOW 출력
 * SCAN     = 0x0001FE  CH0~CH7 단일 종단 스캔
 * TIMER    = 0x000000  스캔 간 딜레이 없음
 * ============================================================
 *
 * 32비트 ADCDATA 포맷 (DATA_FORMAT=10):
 *   [31:28] SGN[3:0]  (부호 확장)
 *   [27:24] CH_ID[3:0] (채널 번호 0~7)
 *   [23: 0] ADC_DATA   (24비트 부호 있는 정수)
 *
 * /IRQ 동작:
 *   변환 완료 → /IRQ LOW
 *   ADCDATA 읽기(SPI) → /IRQ HIGH (자동 deassert)
 *   → 다음 채널 변환 시작
 * ============================================================
 */
#include "adc_meas.h"
#include <string.h>

/* ==========================================================
 * 공유 변수
 * ========================================================== */
volatile int32_t  g_adc_raw[8]     = {0};
volatile uint8_t  g_adc_updated[8] = {0};
SemaphoreHandle_t g_adc_irq_sem    = NULL;

/* ==========================================================
 * MCP3465R_WriteReg1
 *   1바이트 레지스터 쓰기
 *   TX: [CMD_BYTE][DATA_BYTE]
 * ========================================================== */
HAL_StatusTypeDef MCP3465R_WriteReg1(uint8_t reg, uint8_t data)
{
    uint8_t tx[2] = {
        MCP346X_CMD(reg, MCP346X_CMD_IWRITE),
        data
    };

    ADC_CS_LOW();
    HAL_StatusTypeDef ret = HAL_SPI_Transmit(&hspi2, tx, 2U, 10U);
    ADC_CS_HIGH();
    return ret;
}

/* ==========================================================
 * MCP3465R_WriteReg3
 *   3바이트 레지스터 쓰기 (SCAN, TIMER 등 24비트 레지스터)
 *   TX: [CMD_BYTE][DATA[23:16]][DATA[15:8]][DATA[7:0]]
 * ========================================================== */
HAL_StatusTypeDef MCP3465R_WriteReg3(uint8_t reg, uint32_t data)
{
    uint8_t tx[4] = {
        MCP346X_CMD(reg, MCP346X_CMD_IWRITE),
        (uint8_t)((data >> 16) & 0xFFU),
        (uint8_t)((data >>  8) & 0xFFU),
        (uint8_t)( data        & 0xFFU)
    };

    ADC_CS_LOW();
    HAL_StatusTypeDef ret = HAL_SPI_Transmit(&hspi2, tx, 4U, 10U);
    ADC_CS_HIGH();
    return ret;
}

/* ==========================================================
 * MCP3465R_ReadADCData
 *   ADCDATA 레지스터 읽기 (Incremental Read, 32비트)
 *
 *   반환: 24비트 부호 있는 ADC 값 (int32_t로 부호 확장)
 *   ch_id_out: 채널 번호 (0~7) 저장 포인터 (NULL 가능)
 * ========================================================== */
uint32_t MCP3465R_ReadADCData(uint8_t *ch_id_out)
{
    uint8_t tx[5] = {
        MCP346X_CMD(MCP346X_REG_ADCDATA, MCP346X_CMD_IREAD),
        0x00U, 0x00U, 0x00U, 0x00U
    };
    uint8_t rx[5] = {0};

    ADC_CS_LOW();
    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 5U, 10U);
    ADC_CS_HIGH();

    /* rx[0] = 상태 바이트 (첫 번째 바이트는 커맨드 응답)
     * rx[1][7:4] = SGN[3:0]
     * rx[1][3:0] = CH_ID[3:0]
     * rx[2~4]    = ADC_DATA[23:0]
     */
    uint8_t  ch_id   = rx[1] & 0x0FU;
    uint32_t raw24   = ((uint32_t)rx[2] << 16)
                     | ((uint32_t)rx[3] <<  8)
                     | ((uint32_t)rx[4]      );

    if (ch_id_out) {
        *ch_id_out = ch_id;
    }

    return raw24;
}

/* ==========================================================
 * ADCMeas_Init
 *   MCP3465R 레지스터 설정 + Continuous Scan 시작
 * ========================================================== */
HAL_StatusTypeDef ADCMeas_Init(void)
{
    HAL_StatusTypeDef ret;

    memset((void *)g_adc_raw,     0, sizeof(g_adc_raw));
    memset((void *)g_adc_updated, 0, sizeof(g_adc_updated));

    /* FreeRTOS 세마포어 생성 (ISR → 태스크 알림용) */
    if (g_adc_irq_sem == NULL) {
        g_adc_irq_sem = xSemaphoreCreateBinary();
        if (g_adc_irq_sem == NULL) return HAL_ERROR;
    }

    /* -------------------------------------------------------
     * CONFIG0: 내부 RC 발진기(~600kHz), 연속 변환
     *   [7:6] VREF_SEL=00  [5:4] CLK_SEL=10
     *   [3:2] CS_SEL=00    [1:0] ADC_MODE=11
     * ------------------------------------------------------- */
    ret = MCP3465R_WriteReg1(MCP346X_REG_CONFIG0, 0x23U);
    if (ret != HAL_OK) return ret;

    /* -------------------------------------------------------
     * CONFIG1: PRE=1(÷1), OSR=32(0b0000)
     *   [7:6] PRE=00  [5:2] OSR=0000  [1:0]=00
     * ------------------------------------------------------- */
    ret = MCP3465R_WriteReg1(MCP346X_REG_CONFIG1, 0x00U);
    if (ret != HAL_OK) return ret;

    /* -------------------------------------------------------
     * CONFIG2: BOOST=1x(11), GAIN=1x(001), AZ_MUX=0
     *   0b11_001_0_0_0 = 0xC8
     * ------------------------------------------------------- */
    ret = MCP3465R_WriteReg1(MCP346X_REG_CONFIG2, 0xC8U);
    if (ret != HAL_OK) return ret;

    /* -------------------------------------------------------
     * CONFIG3: CONV_MODE=연속(11), DATA_FORMAT=CHID포함(10)
     *   0b11_10_0_0_0_0 = 0xE0
     * ------------------------------------------------------- */
    ret = MCP3465R_WriteReg1(MCP346X_REG_CONFIG3, 0xE0U);
    if (ret != HAL_OK) return ret;

    /* -------------------------------------------------------
     * IRQ: /IRQ 활성 LOW 출력
     *   0x00 (기본값 사용)
     * ------------------------------------------------------- */
    ret = MCP3465R_WriteReg1(MCP346X_REG_IRQ, 0x00U);
    if (ret != HAL_OK) return ret;

    /* -------------------------------------------------------
     * SCAN: CH0~CH7 단일 종단 스캔 활성화
     *   비트[8:1] = SE_7~SE_0 → 0x01FE
     *   딜레이 없음(DLY=000 at bits[23:21])
     *   SCAN = 0x0001FE
     * ------------------------------------------------------- */
    ret = MCP3465R_WriteReg3(MCP346X_REG_SCAN, 0x0001FEU);
    if (ret != HAL_OK) return ret;

    /* -------------------------------------------------------
     * TIMER: 스캔 반복 간 딜레이 없음
     * ------------------------------------------------------- */
    ret = MCP3465R_WriteReg3(MCP346X_REG_TIMER, 0x000000U);
    if (ret != HAL_OK) return ret;

    /* CONFIG0의 ADC_MODE=11이 이미 연속변환 모드이므로
     * 별도의 CONV_START 커맨드 불필요.
     * (또는 Fast Command로 강제 시작 가능) */

    return HAL_OK;
}

/* ==========================================================
 * ADCMeas_IRQHandler
 *   /IRQ EXTI 콜백에서 호출
 *   - SPI로 ADCDATA 읽기 (CS low → SPI transceive → CS high)
 *   - 채널 ID로 버퍼 갱신
 *   - 세마포어로 PID 태스크에 알림
 *
 *   주의: 이 함수는 ISR 컨텍스트에서 호출됨
 *         HAL_SPI_TransmitReceive → 폴링 방식 (짧은 시간)
 * ========================================================== */
void ADCMeas_IRQHandler(void)
{
    uint8_t  ch_id = 0xFF;
    uint32_t raw24 = MCP3465R_ReadADCData(&ch_id);

    if (ch_id > 7U) return;  /* 유효하지 않은 채널 무시 */

    /* 24비트 부호 확장 */
    int32_t signed_val;
    if (raw24 & 0x800000UL) {
        signed_val = (int32_t)(raw24 | 0xFF000000UL);
    } else {
        signed_val = (int32_t)raw24;
    }

    /* 음수 클램프 (단일 종단: 0 미만 = 0) */
    if (signed_val < 0) signed_val = 0;

    g_adc_raw[ch_id]     = signed_val;
    g_adc_updated[ch_id] = 1U;

    /* 8채널 전부 갱신되면 PID 태스크 깨우기 */
    uint8_t all_updated = 1U;
    for (uint8_t i = 0; i < 8U; i++) {
        if (!g_adc_updated[i]) { all_updated = 0U; break; }
    }
    if (all_updated) {
        /* 플래그 리셋 */
        for (uint8_t i = 0; i < 8U; i++) g_adc_updated[i] = 0U;

        /* ISR 컨텍스트에서 세마포어 Give */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(g_adc_irq_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* ==========================================================
 * ADCMeas_GetVoltage_mV
 *
 * 변환식:
 *   V_adc (mV) = raw24 * MCP3465R_VREF_MV / MCP3465R_FULLSCALE
 *   V_in  (mV) = V_adc * VOLT_DIV_DEN / VOLT_DIV_NUM
 * ========================================================== */
uint32_t ADCMeas_GetVoltage_mV(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return 0U;

    uint8_t  adc_idx = (uint8_t)(ch * 2U);   /* 전압 = 짝수 채널 */
    int32_t  raw     = g_adc_raw[adc_idx];

    if (raw < 0) raw = 0;

    /* mV 변환 (64비트 중간 계산) */
    uint32_t v_mV = (uint32_t)(
        ((uint64_t)(uint32_t)raw * MCP3465R_VREF_MV * VOLT_DIV_DEN)
        / ((uint64_t)MCP3465R_FULLSCALE * VOLT_DIV_NUM)
    );

    return v_mV;
}

/* ==========================================================
 * ADCMeas_GetCurrent_mA
 *
 * 변환식:
 *   V_adc (mV) = raw24 * MCP3465R_VREF_MV / MCP3465R_FULLSCALE
 *   I (mA)     = V_adc * CURRENT_MAX_MA / MCP3465R_VREF_MV
 *              = raw24 * CURRENT_MAX_MA / MCP3465R_FULLSCALE
 * ========================================================== */
uint32_t ADCMeas_GetCurrent_mA(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return 0U;

    uint8_t  adc_idx = (uint8_t)(ch * 2U + 1U);  /* 전류 = 홀수 채널 */
    int32_t  raw     = g_adc_raw[adc_idx];

    if (raw < 0) raw = 0;

    uint32_t i_mA = (uint32_t)(
        ((uint64_t)(uint32_t)raw * CURRENT_MAX_MA)
        / (uint64_t)MCP3465R_FULLSCALE
    );

    return i_mA;
}

/* ==========================================================
 * HAL_GPIO_EXTI_Callback (weak 함수 오버라이드)
 *   PB5 (/IRQ_ADC) 하강 엣지 → ADCMeas_IRQHandler 호출
 * ========================================================== */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ADC_IRQ_Pin) {
        ADCMeas_IRQHandler();
    }
}
