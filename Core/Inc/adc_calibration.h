/**
 * adc_calibration.h
 * High-level calibration interface for MCP3465R on STM32L552
 *
 * Calibration strategy:
 *   Offset : MUX → AGND/AGND, measure residual code, store in OFFSETCAL
 *   Gain   : MUX → REFIN+/REFIN-, compare to expected full-scale 0x7FFFFF
 *
 * Results are saved to the last Flash page (0x0807F800) and auto-loaded
 * on the next boot.
 */

#ifndef ADC_CALIBRATION_H
#define ADC_CALIBRATION_H

#include "mcp3465r.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Flash storage (STM32L552 마지막 페이지: 0x0807F800, 2KB)            */
/* ------------------------------------------------------------------ */
#define CAL_FLASH_BASE_ADDR  0x0807F800UL
#define CAL_FLASH_BANK       FLASH_BANK_2
#define CAL_FLASH_PAGE       127U           /* Bank-2 마지막 페이지 */
#define CAL_MAGIC_WORD       0xCA11B8A7UL   /* 유효성 식별자 */

/* ------------------------------------------------------------------ */
/*  캘리브레이션 샘플 수                                                  */
/* ------------------------------------------------------------------ */
#define CAL_NUM_SAMPLES_OFFSET  32U
#define CAL_NUM_SAMPLES_GAIN    32U

/* ------------------------------------------------------------------ */
/*  전압 변환 상수                                                        */
/* ------------------------------------------------------------------ */
#define CAL_VREF_V              3.0f   /* 외부 기준 전압 (V) */
#define CAL_DIVIDER_RATIO       0.6f   /* Vadc = Vin × 0.6 (R1=2kΩ, R2=3kΩ) */
#define CAL_ADC_FULL_SCALE      8388607.0f  /* 2^23 - 1 */

/* ------------------------------------------------------------------ */
/*  캘리브레이션 데이터 구조체 (Flash 저장 레이아웃과 일치)                 */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t magic;       /* CAL_MAGIC_WORD — Flash 유효성 검사용 */
    int32_t  offset_cal;  /* OFFSETCAL 레지스터 값 (24-bit 2의 보수) */
    uint32_t gain_cal;    /* GAINCAL 레지스터 값 (0x800000 = 1.0)    */
    uint32_t crc;         /* 앞 3개 필드의 CRC32                      */
} ADC_CalData_t;

/* ------------------------------------------------------------------ */
/*  Function prototypes                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief  오프셋 캘리브레이션
 *         MUX를 AGND/AGND로 설정 후 num_samples회 측정, OFFSETCAL에 기록
 * @param  hdev        MCP3465R 핸들
 * @param  cal         캘리브레이션 데이터 출력 구조체
 * @param  num_samples 평균 샘플 수 (권장: 32)
 */
HAL_StatusTypeDef ADC_Cal_OffsetCalibrate(MCP3465R_Handle_t *hdev,
                                           ADC_CalData_t *cal,
                                           uint16_t num_samples);

/**
 * @brief  게인 캘리브레이션
 *         MUX를 REFIN+/REFIN-로 설정, 기대값 0x7FFFFF 대비 오차 보정
 * @param  hdev        MCP3465R 핸들
 * @param  cal         캘리브레이션 데이터 출력 구조체 (offset_cal이 이미 유효해야 함)
 * @param  num_samples 평균 샘플 수 (권장: 32)
 */
HAL_StatusTypeDef ADC_Cal_GainCalibrate(MCP3465R_Handle_t *hdev,
                                         ADC_CalData_t *cal,
                                         uint16_t num_samples);

/**
 * @brief  오프셋 + 게인 캘리브레이션 전체 수행
 */
HAL_StatusTypeDef ADC_Cal_RunAll(MCP3465R_Handle_t *hdev, ADC_CalData_t *cal);

/**
 * @brief  cal 구조체의 값을 MCP3465R 레지스터에 적용 및 EN_OFFCAL/EN_GAINCAL 활성화
 */
HAL_StatusTypeDef ADC_Cal_Apply(MCP3465R_Handle_t *hdev, const ADC_CalData_t *cal);

/**
 * @brief  캘리브레이션 데이터를 Flash에 저장 (CRC32 포함)
 */
HAL_StatusTypeDef ADC_Cal_SaveToFlash(const ADC_CalData_t *cal);

/**
 * @brief  Flash에서 캘리브레이션 데이터 로드 및 CRC 검증
 * @return HAL_OK: 유효한 데이터 로드 성공 / HAL_ERROR: 데이터 없음 또는 손상
 */
HAL_StatusTypeDef ADC_Cal_LoadFromFlash(ADC_CalData_t *cal);

/**
 * @brief  원시 ADC 코드를 입력 전압(V)으로 변환
 *         전압 분배기 역보정 포함: Vin = Vadc / 0.6
 * @param  raw_code  MCP3465R_ReadData()에서 얻은 24-bit 부호 정수
 * @return 입력 전압 (0.0 ~ 5.0 V), 범위 초과 시 클램핑
 */
float ADC_Cal_ConvertToVoltage(int32_t raw_code);

/**
 * @brief  CH0/AGND MUX로 전환 후 단일 측정값(원시 코드) 반환
 *         캘리브레이션 적용 상태(EN_OFFCAL/EN_GAINCAL)는 유지
 */
HAL_StatusTypeDef ADC_Cal_ReadVoltageRaw(MCP3465R_Handle_t *hdev,
                                          int32_t *raw_code,
                                          uint32_t timeout_ms);

#endif /* ADC_CALIBRATION_H */
