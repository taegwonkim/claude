/**
 ******************************************************************************
 * @file    adc_mcp3465r.h
 * @brief   MCP3465R 16비트 델타-시그마 ADC 드라이버
 *
 * MCP3465R SPI 명령 포맷 (첫 바이트):
 *   [7:6] DEVICE_ADDR (기본 0b01)
 *   [5:2] Register Address
 *   [1:0] Command Type:
 *         00 = Don't care
 *         01 = Incremental Write
 *         10 = Incremental Read
 *         11 = Fast Command
 *
 * 채널 할당:
 *   CH0 : 채널1 전압 측정
 *   CH1 : 채널2 전압 측정
 *   CH2 : 채널3 전압 측정
 *   CH3 : 채널4 전압 측정
 *   CH4 : 채널1 전류 측정 (션트)
 *   CH5 : 채널2 전류 측정 (션트)
 *   CH6 : 채널3 전류 측정 (션트)
 *   CH7 : 채널4 전류 측정 (션트)
 *
 * 출력 데이터: 16비트 모드 사용 (CONFIG3.DATA_FORMAT = 11b)
 *             부호있는 16비트 정수
 ******************************************************************************
 */

#ifndef __ADC_MCP3465R_H
#define __ADC_MCP3465R_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* -------------------------------------------------------------------------
 * MCP3465R 레지스터 주소
 * ------------------------------------------------------------------------- */
#define MCP3465R_ADCDATA        0x00U
#define MCP3465R_CONFIG0        0x01U
#define MCP3465R_CONFIG1        0x02U
#define MCP3465R_CONFIG2        0x03U
#define MCP3465R_CONFIG3        0x04U
#define MCP3465R_IRQ            0x05U
#define MCP3465R_MUX            0x06U
#define MCP3465R_SCAN           0x07U
#define MCP3465R_TIMER          0x08U
#define MCP3465R_OFFSETCAL      0x09U
#define MCP3465R_GAINCAL        0x0AU
#define MCP3465R_LOCK           0x0DU
#define MCP3465R_CRCCFG         0x0FU

/* -------------------------------------------------------------------------
 * 명령 타입
 * ------------------------------------------------------------------------- */
#define MCP3465R_CMD_FAST       0x03U   /* Fast Command */
#define MCP3465R_CMD_WRITE      0x01U   /* Incremental Write */
#define MCP3465R_CMD_READ       0x02U   /* Incremental Read (Static Read도 가능) */
#define MCP3465R_CMD_STATIC_READ 0x01U  /* Static Read (address + 01) – 실제론 0x02 */

/* Device address (기본값 0b01) */
#define MCP3465R_DEV_ADDR       0x01U

/* 첫 바이트 생성 매크로 */
#define MCP3465R_CMD_BYTE(reg, cmd)  (uint8_t)(((MCP3465R_DEV_ADDR) << 6) | ((reg) << 2) | (cmd))

/* Fast Command 정의 */
#define MCP3465R_FAST_START_CONT  0xA8U  /* Start/Restart ADC Conversion (Continuous) */
#define MCP3465R_FAST_START_ONE   0xACU  /* Start/Restart ADC Conversion (One-shot) */
#define MCP3465R_FAST_STANDBY     0xB4U  /* Standby */
#define MCP3465R_FAST_SHUTDOWN    0xB8U  /* Shutdown */
#define MCP3465R_FAST_RESET       0xBCU  /* Full Reset */

/* -------------------------------------------------------------------------
 * CONFIG0 레지스터 비트
 * VREF_SEL[7:6], CLK_SEL[5:4], CS_SEL[3:2], ADC_MODE[1:0]
 * ------------------------------------------------------------------------- */
#define MCP3465R_CONFIG0_VREF_INT       (0x00U << 6)   /* Internal VREF (disabled – no int VREF on R) */
#define MCP3465R_CONFIG0_VREF_EXT       (0x02U << 6)   /* External VREF (REFIN) */
#define MCP3465R_CONFIG0_CLK_INT        (0x02U << 4)   /* Internal oscillator, no CLK output */
#define MCP3465R_CONFIG0_CLK_EXT        (0x00U << 4)   /* External clock (SCLK) */
#define MCP3465R_CONFIG0_CS_NONE        (0x00U << 2)   /* No current source */
#define MCP3465R_CONFIG0_MODE_SHUTDOWN  (0x00U)
#define MCP3465R_CONFIG0_MODE_STANDBY   (0x02U)
#define MCP3465R_CONFIG0_MODE_CONT      (0x03U)

/* -------------------------------------------------------------------------
 * CONFIG1 – OSR (Over-Sampling Ratio)
 * OSR[7:4], DITHER[3:2]
 * ------------------------------------------------------------------------- */
#define MCP3465R_OSR_32         (0x00U << 4)   /* 32   → ~500 ksps @ MCLK=16MHz */
#define MCP3465R_OSR_64         (0x01U << 4)   /* 64 */
#define MCP3465R_OSR_128        (0x02U << 4)   /* 128 */
#define MCP3465R_OSR_256        (0x03U << 4)   /* 256  → ~62.5 ksps */
#define MCP3465R_OSR_512        (0x04U << 4)   /* 512 */
#define MCP3465R_OSR_1024       (0x05U << 4)   /* 1024 → ~15.6 ksps */
#define MCP3465R_OSR_2048       (0x06U << 4)   /* 2048 */
#define MCP3465R_OSR_4096       (0x07U << 4)   /* 4096 → ~3.9 ksps */
#define MCP3465R_OSR_8192       (0x08U << 4)   /* 8192 → ~1.95 ksps */
#define MCP3465R_OSR_16384      (0x09U << 4)   /* 16384 → ~977 sps */
#define MCP3465R_OSR_20480      (0x0AU << 4)
#define MCP3465R_OSR_24576      (0x0BU << 4)
#define MCP3465R_OSR_40960      (0x0CU << 4)
#define MCP3465R_OSR_49152      (0x0DU << 4)
#define MCP3465R_OSR_81920      (0x0EU << 4)
#define MCP3465R_OSR_98304      (0x0FU << 4)

/* -------------------------------------------------------------------------
 * CONFIG2 – GAIN, AZ_MUX, VREF_EXT
 * ------------------------------------------------------------------------- */
#define MCP3465R_GAIN_THIRD     (0x00U << 3)   /* 1/3 */
#define MCP3465R_GAIN_1         (0x01U << 3)   /* 1 */
#define MCP3465R_GAIN_2         (0x02U << 3)   /* 2 */
#define MCP3465R_GAIN_4         (0x03U << 3)   /* 4 */
#define MCP3465R_GAIN_8         (0x04U << 3)   /* 8 */
#define MCP3465R_GAIN_16        (0x05U << 3)   /* 16 */
#define MCP3465R_GAIN_32        (0x06U << 3)   /* 32 (channel 7 only) */
#define MCP3465R_AZ_MUX_EN      (0x01U << 2)

/* -------------------------------------------------------------------------
 * CONFIG3 – DATA_FORMAT, CRC_FORMAT 등
 * ------------------------------------------------------------------------- */
#define MCP3465R_FMT_16BIT_SGN  (0x03U << 6)   /* 16비트 부호 있음 */
#define MCP3465R_FMT_16BIT_UNS  (0x02U << 6)   /* 16비트 부호 없음 */
#define MCP3465R_FMT_32BIT_SGN  (0x01U << 6)   /* 32비트 (24비트 데이터 + 8비트 상태) */
#define MCP3465R_CONV_ONE_SHOT  (0x02U << 4)   /* One-shot shutdown */
#define MCP3465R_CONV_CONT      (0x03U << 4)   /* Continuous */
#define MCP3465R_CRC_OFF        (0x00U << 2)
#define MCP3465R_CRC_16         (0x01U << 2)

/* -------------------------------------------------------------------------
 * MUX 레지스터 – 채널 선택
 * ------------------------------------------------------------------------- */
#define MCP3465R_MUX_CH(pos, neg) (uint8_t)(((pos) << 4) | (neg))
#define MCP3465R_AGND           0x0FU   /* AGND (negative input으로 사용) */
/* 단일 끝 입력: MCP3465R_MUX_CH(n, MCP3465R_AGND) */

/* -------------------------------------------------------------------------
 * 전체 ADC 측정 결과 구조체
 * ------------------------------------------------------------------------- */
typedef struct {
    int16_t raw[8];             /* 채널별 원시 ADC 코드 (CH0~CH7) */
    uint16_t voltage_mv[4];     /* CH0~3 → 전압 mV */
    int16_t  current_ma[4];     /* CH4~7 → 전류 mA */
    uint32_t timestamp;         /* osKernelGetTickCount() */
} AdcMeasurement_t;

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * @brief  MCP3465R 초기화
 *         - 내부 오실레이터, 외부 VREF, Continuous 변환, 16비트 부호형
 */
HAL_StatusTypeDef ADC_Init(void);

/**
 * @brief  단일 채널 즉시 측정 (MUX 변경 후 읽기)
 * @param  mux_ch  MUX 설정값 (MCP3465R_MUX_CH() 매크로 사용)
 * @param  out     측정된 원시 코드 (부호 있는 16비트)
 */
HAL_StatusTypeDef ADC_ReadChannel(uint8_t mux_ch, int16_t *out);

/**
 * @brief  8채널 순차 측정 후 결과 저장
 * @param  result  결과 구조체 포인터
 */
HAL_StatusTypeDef ADC_ReadAllChannels(AdcMeasurement_t *result);

/**
 * @brief  원시 코드 → 전압(mV) 변환
 */
static inline uint16_t ADC_RawToMillivolts(int16_t raw)
{
    /* 16비트 부호형: -32768 ~ 32767, 단극성 입력 사용 시 0~32767 */
    if (raw < 0) raw = 0;
    return (uint16_t)(((uint32_t)raw * VREF_MV) / 32767U);
}

/**
 * @brief  원시 코드 → 전류(mA) 변환 (션트 저항 기준)
 *         Vshunt_uV = raw_code × Vref_uV / 32767
 *         I_mA      = Vshunt_uV / Rshunt_mΩ
 */
static inline int16_t ADC_RawToMilliamps(int16_t raw)
{
    int32_t v_uv = ((int32_t)raw * (int32_t)VREF_UV) / 32767;
    return (int16_t)(v_uv / (int32_t)SHUNT_RESISTOR_MOHM);
}

#ifdef __cplusplus
}
#endif

#endif /* __ADC_MCP3465R_H */
