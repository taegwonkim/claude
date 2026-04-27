/**
 * adc_calibration.c
 * MCP3465R 자동 캘리브레이션 구현
 *
 * Offset Cal : MUX = AGND/AGND → 잔류 코드 측정 → OFFSETCAL 에 역값 기록
 * Gain Cal   : MUX = REFIN+/REFIN- → 기대값 0x7FFFFF 대비 비율 계산 → GAINCAL 기록
 * Flash 저장 : STM32L552 Bank-2 마지막 페이지 (0x0807F800)
 */

#include "adc_calibration.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  내부: 단순 CRC32 (IEEE 802.3 다항식)                                  */
/* ------------------------------------------------------------------ */
static uint32_t calc_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (uint8_t b = 0; b < 8U; b++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ------------------------------------------------------------------ */
/*  내부: num_samples 회 측정 후 정수 평균 반환                              */
/* ------------------------------------------------------------------ */
static HAL_StatusTypeDef average_samples(MCP3465R_Handle_t *hdev,
                                          uint16_t num_samples,
                                          int32_t *avg_out)
{
    int64_t sum = 0;
    int32_t sample;

    for (uint16_t i = 0; i < num_samples; i++) {
        HAL_StatusTypeDef ret = MCP3465R_ReadData(hdev, &sample, MCP3465R_CONV_TIMEOUT_MS);
        if (ret != HAL_OK) return ret;
        sum += (int64_t)sample;
    }

    *avg_out = (int32_t)(sum / (int64_t)num_samples);
    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  ADC_Cal_OffsetCalibrate                                             */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef ADC_Cal_OffsetCalibrate(MCP3465R_Handle_t *hdev,
                                           ADC_CalData_t *cal,
                                           uint16_t num_samples)
{
    HAL_StatusTypeDef ret;
    int32_t offset_raw;

    /* 1. 보정 레지스터 비활성화 상태로 측정 (순수 오프셋만 획득) */
    ret = MCP3465R_SetConfig3(hdev, 0, 0);
    if (ret != HAL_OK) return ret;

    /* 2. OFFSETCAL 초기화 (0으로 리셋) */
    ret = MCP3465R_WriteOffsetCal(hdev, 0);
    if (ret != HAL_OK) return ret;

    /* 3. 캘리브레이션용 고해상도 OSR로 전환 */
    ret = MCP3465R_SetOSR(hdev, MCP3465R_CONFIG1_OSR_CAL);
    if (ret != HAL_OK) return ret;

    /* 4. MUX: AGND(+) / AGND(-) — 이상적으로는 0이어야 함 */
    ret = MCP3465R_SetMux(hdev, MCP3465R_MUX_AGND, MCP3465R_MUX_AGND);
    if (ret != HAL_OK) return ret;

    /* 5. num_samples 회 평균 측정 */
    ret = average_samples(hdev, num_samples, &offset_raw);
    if (ret != HAL_OK) return ret;

    /*
     * 6. OFFSETCAL = -offset_raw (24-bit 2의 보수)
     *    ADC 내부 계산: corrected = raw + OFFSETCAL → 0 + (-offset) = 0
     */
    int32_t offsetcal_val = -offset_raw;

    /* 24-bit 범위 클램핑 [-8388608, 8388607] */
    if (offsetcal_val > 8388607L)  offsetcal_val = 8388607L;
    if (offsetcal_val < -8388608L) offsetcal_val = -8388608L;

    ret = MCP3465R_WriteOffsetCal(hdev, offsetcal_val);
    if (ret != HAL_OK) return ret;

    /* 7. EN_OFFCAL 활성화 */
    ret = MCP3465R_SetConfig3(hdev, 1, 0);
    if (ret != HAL_OK) return ret;

    /* 8. 결과 저장 */
    cal->offset_cal = offsetcal_val;

    /* 9. OSR을 일반 측정 모드로 복원 */
    ret = MCP3465R_SetOSR(hdev, MCP3465R_CONFIG1_OSR_NORMAL);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  ADC_Cal_GainCalibrate                                               */
/*                                                                      */
/*  REFIN+ (= VREF = 3.0V)을 VIN+, REFIN- (= AGND)를 VIN-로 읽으면     */
/*  이상적인 경우 코드 = 0x7FFFFF (풀스케일 양수 최대값)                    */
/*  실제 코드와 비교하여 게인 오차 보정                                      */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef ADC_Cal_GainCalibrate(MCP3465R_Handle_t *hdev,
                                         ADC_CalData_t *cal,
                                         uint16_t num_samples)
{
    HAL_StatusTypeDef ret;
    int32_t actual_code;

    /* 1. GAINCAL 초기화 (Unity = 0x800000) */
    ret = MCP3465R_WriteGainCal(hdev, MCP3465R_GAINCAL_UNITY);
    if (ret != HAL_OK) return ret;

    /* 2. 고해상도 OSR */
    ret = MCP3465R_SetOSR(hdev, MCP3465R_CONFIG1_OSR_CAL);
    if (ret != HAL_OK) return ret;

    /* 3. MUX: REFIN+(+) / REFIN-(-) — ADC가 자신의 기준전압 측정 */
    ret = MCP3465R_SetMux(hdev, MCP3465R_MUX_REFIN_P, MCP3465R_MUX_REFIN_M);
    if (ret != HAL_OK) return ret;

    /* 4. EN_OFFCAL=1 (오프셋 보정 적용), EN_GAINCAL=0 (게인 보정은 아직 미적용) */
    ret = MCP3465R_SetConfig3(hdev, 1, 0);
    if (ret != HAL_OK) return ret;

    /* 5. 평균 측정 */
    ret = average_samples(hdev, num_samples, &actual_code);
    if (ret != HAL_OK) return ret;

    /* 실제 코드가 0이거나 비이상적으로 작으면 오류 */
    if (actual_code <= 0L) return HAL_ERROR;

    /*
     * 6. GAINCAL 계산
     *    expected_code = 0x7FFFFF (REFIN+ - REFIN- = VREF = 풀스케일)
     *    GAINCAL = (expected / actual) × 0x800000
     *
     *    내부 보정 공식: corrected = raw × (GAINCAL / 0x800000)
     *    → actual × (GAINCAL / 0x800000) = expected
     *    → GAINCAL = expected × 0x800000 / actual
     */
    float gaincal_f = ((float)MCP3465R_ADC_FULL_SCALE / (float)actual_code)
                      * (float)MCP3465R_GAINCAL_UNITY;

    int32_t gaincal = (int32_t)gaincal_f;

    /* 허용 범위: ±50% (0x400000 ~ 0xBFFFFF) */
    if (gaincal < 0x400000L) gaincal = 0x400000L;
    if (gaincal > 0xBFFFFF) gaincal = 0xBFFFFF;

    ret = MCP3465R_WriteGainCal(hdev, (uint32_t)gaincal);
    if (ret != HAL_OK) return ret;

    /* 7. EN_OFFCAL=1, EN_GAINCAL=1 활성화 */
    ret = MCP3465R_SetConfig3(hdev, 1, 1);
    if (ret != HAL_OK) return ret;

    /* 8. 결과 저장 */
    cal->gain_cal = (uint32_t)gaincal;

    /* 9. MUX를 정상 채널로 복원, OSR 복원 */
    ret = MCP3465R_SetMux(hdev, MCP3465R_MUX_CH0, MCP3465R_MUX_AGND);
    if (ret != HAL_OK) return ret;

    ret = MCP3465R_SetOSR(hdev, MCP3465R_CONFIG1_OSR_NORMAL);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  ADC_Cal_RunAll                                                      */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef ADC_Cal_RunAll(MCP3465R_Handle_t *hdev, ADC_CalData_t *cal)
{
    HAL_StatusTypeDef ret;

    memset(cal, 0, sizeof(ADC_CalData_t));

    ret = ADC_Cal_OffsetCalibrate(hdev, cal, CAL_NUM_SAMPLES_OFFSET);
    if (ret != HAL_OK) return ret;

    ret = ADC_Cal_GainCalibrate(hdev, cal, CAL_NUM_SAMPLES_GAIN);
    if (ret != HAL_OK) return ret;

    cal->magic = CAL_MAGIC_WORD;

    /* CRC 계산: magic + offset_cal + gain_cal (12 bytes) */
    cal->crc = calc_crc32((const uint8_t *)cal, sizeof(ADC_CalData_t) - sizeof(uint32_t));

    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  ADC_Cal_Apply                                                       */
/*  이미 유효한 cal 구조체를 ADC 레지스터에 적용                              */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef ADC_Cal_Apply(MCP3465R_Handle_t *hdev, const ADC_CalData_t *cal)
{
    HAL_StatusTypeDef ret;

    ret = MCP3465R_WriteOffsetCal(hdev, cal->offset_cal);
    if (ret != HAL_OK) return ret;

    ret = MCP3465R_WriteGainCal(hdev, cal->gain_cal);
    if (ret != HAL_OK) return ret;

    ret = MCP3465R_SetConfig3(hdev, 1, 1);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  ADC_Cal_SaveToFlash                                                 */
/*  STM32L552 Bank-2 마지막 페이지에 더블워드(64-bit) 단위로 기록            */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef ADC_Cal_SaveToFlash(const ADC_CalData_t *cal)
{
    HAL_StatusTypeDef ret = HAL_OK;
    FLASH_EraseInitTypeDef erase_cfg;
    uint32_t page_error;

    /* sizeof(ADC_CalData_t) = 16 bytes = 2 × 64-bit 더블워드 */
    uint64_t dwords[2];
    memcpy(dwords, cal, sizeof(ADC_CalData_t));

    HAL_FLASH_Unlock();

    /* 페이지 삭제 */
    erase_cfg.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_cfg.Banks     = CAL_FLASH_BANK;
    erase_cfg.Page      = CAL_FLASH_PAGE;
    erase_cfg.NbPages   = 1;

    if (HAL_FLASHEx_Erase(&erase_cfg, &page_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    /* 더블워드 단위로 프로그래밍 */
    for (uint8_t i = 0; i < 2U; i++) {
        uint32_t addr = CAL_FLASH_BASE_ADDR + (uint32_t)(i * 8U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, dwords[i]) != HAL_OK) {
            ret = HAL_ERROR;
            break;
        }
    }

    HAL_FLASH_Lock();
    return ret;
}

/* ------------------------------------------------------------------ */
/*  ADC_Cal_LoadFromFlash                                               */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef ADC_Cal_LoadFromFlash(ADC_CalData_t *cal)
{
    /* Flash 직접 읽기 (STM32L552는 Flash를 메모리맵으로 접근 가능) */
    memcpy(cal, (const void *)CAL_FLASH_BASE_ADDR, sizeof(ADC_CalData_t));

    /* Magic word 확인 */
    if (cal->magic != CAL_MAGIC_WORD) {
        return HAL_ERROR;
    }

    /* CRC 검증 */
    uint32_t expected_crc = calc_crc32((const uint8_t *)cal,
                                        sizeof(ADC_CalData_t) - sizeof(uint32_t));
    if (cal->crc != expected_crc) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/*  ADC_Cal_ConvertToVoltage                                            */
/*  24-bit 부호 정수 → 입력 전압 (0.0 ~ 5.0 V)                           */
/*                                                                      */
/*  Vadc = (raw / 8388607) × VREF                                       */
/*  Vin  = Vadc / DIVIDER_RATIO = Vadc / 0.6                           */
/* ------------------------------------------------------------------ */
float ADC_Cal_ConvertToVoltage(int32_t raw_code)
{
    float vadc = ((float)raw_code / CAL_ADC_FULL_SCALE) * CAL_VREF_V;
    float vin  = vadc / CAL_DIVIDER_RATIO;

    /* 0~5V 범위 클램핑 */
    if (vin < 0.0f)  vin = 0.0f;
    if (vin > 5.0f)  vin = 5.0f;

    return vin;
}

/* ------------------------------------------------------------------ */
/*  ADC_Cal_ReadVoltageRaw                                              */
/*  CH0/AGND MUX 설정 후 단일 변환 → 원시 코드 반환                        */
/*  캘리브레이션 보정(EN_OFFCAL/EN_GAINCAL)은 현재 CONFIG3 상태 유지         */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef ADC_Cal_ReadVoltageRaw(MCP3465R_Handle_t *hdev,
                                          int32_t *raw_code,
                                          uint32_t timeout_ms)
{
    HAL_StatusTypeDef ret;

    ret = MCP3465R_SetMux(hdev, MCP3465R_MUX_CH0, MCP3465R_MUX_AGND);
    if (ret != HAL_OK) return ret;

    return MCP3465R_ReadData(hdev, raw_code, timeout_ms);
}
