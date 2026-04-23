/**
 * example_main.c
 *
 * MCP3465R 자동 캘리브레이션 사용 예제 – 입력 범위 0 ~ 5 V (STM32 HAL 기반)
 *
 * 사용 흐름:
 *   1. MCP3465R_Init()          – ADC 레지스터 초기값 설정 (외부 5 V VREF)
 *   2. MCP3465R_AutoCalibrate() – offset/gain 자동 측정 후 보정 레지스터 활성화
 *   3. MCP3465R_ReadADC()       – 보정이 적용된 최종 ADC 코드 읽기
 *
 * 필수 외부 배선:
 *   REFIN+  → 5 V 정밀 기준 전압 (예: REF5050, LT1021-5, 또는 디커플링된 AVDD)
 *   REFIN-  → AGND
 *   AVDD    = 5 V  (입력 신호가 AVDD + 0.3 V 를 초과하면 안 됨)
 *   CH0(AIN0) → 측정 대상 신호 (0 ~ 5 V)
 *   AGND    → 신호 기준 GND
 *
 *   Offset 캘리브레이션: 내부 AGND 라우팅이므로 외부 배선 불필요
 *   Gain   캘리브레이션: MUX → REFIN+/2 vs REFIN-/2 → ΔV = 2.5 V (자동)
 *
 * 전압 환산 공식:
 *   Vin [mV] = code × VREF_mV / (GAIN × 2^23)
 *   예) VREF = 5000 mV, GAIN = 1, code = 0x7FFFFF (풀스케일)
 *     → 5000 / 8388608 × 8388607 ≈ 4999.9994 mV
 *   예) code = 0x400000 (절반)
 *     → 5000 / 8388608 × 4194304 = 2500.0 mV
 */

#include "main.h"           /* CubeMX 생성 파일 (SPI/GPIO 핸들 선언 포함) */
#include "mcp3465r.h"
#include <stdio.h>          /* printf (ITM / UART-redirect 전제) */

/* -------------------------------------------------------------------------
 * 하드웨어 매핑 – 프로젝트에 맞게 수정
 * ------------------------------------------------------------------------- */
extern SPI_HandleTypeDef hspi1;     /* CubeMX 생성 SPI 핸들          */
#define ADC_CS_PORT     GPIOA       /* CS 핀 포트                     */
#define ADC_CS_PIN      GPIO_PIN_4  /* CS 핀 번호                     */

/* VREF 및 PGA gain – Init 설정 및 외부 배선과 일치해야 함 */
#define ADC_VREF_MV     5000.0f     /* 외부 VREF = 5 V (REFIN+ 인가 전압) */
#define ADC_PGA_GAIN    1.0f        /* CONFIG2_GAIN_1 에 대응             */

/* 입력 범위 (0~5 V 단일극 단일 채널) */
#define ADC_INPUT_MIN_MV    0.0f
#define ADC_INPUT_MAX_MV    5000.0f

/* -------------------------------------------------------------------------
 * 보정 결과를 비휘발성 메모리에 보관할 때 활용하는 전역 구조체
 * (예: Flash / EEPROM에 저장 후 전원 재공급 시 MCP3465R_ApplyStoredCal() 호출)
 * ------------------------------------------------------------------------- */
static MCP3465R_CalResult_t g_cal_result;

/* -------------------------------------------------------------------------
 * ADC 코드 → 전압 환산 (mV)
 *
 * 단일극 0~5 V 입력에서 코드 범위:
 *   0x000000 (0)       → 0.000 mV    (0 V)
 *   0x400000 (4194304) → 2500.0 mV   (2.5 V, 중간)
 *   0x7FFFFF (8388607) → 4999.999 mV (5 V, 풀스케일)
 *
 * 입력 신호 노이즈로 인해 0V 근방에서 음수 코드가 나올 수 있으므로
 * 호출 측에서 클램핑하는 것을 권장함.
 * ------------------------------------------------------------------------- */
static float adc_code_to_mv(int32_t code)
{
    return (float)code * ADC_VREF_MV / (ADC_PGA_GAIN * (float)(1L << 23));
}

/* =========================================================================
 * 메인 예제
 * ========================================================================= */
void example_mcp3465r_main(void)
{
    /* ------------------------------------------------------------------ */
    /* 1. 드라이버 핸들 초기화                                              */
    /* ------------------------------------------------------------------ */
    MCP3465R_Handle_t adc = {
        .hspi       = &hspi1,
        .cs_port    = ADC_CS_PORT,
        .cs_pin     = ADC_CS_PIN,
        .timeout_ms = 100,
    };

    /* ------------------------------------------------------------------ */
    /* 2. ADC 초기화                                                        */
    /*    외부 VREF 5 V, 내부 클록, OSR=4096, 연속 변환 모드               */
    /*    MUX: CH0(VIN+) vs AGND(VIN-)  → 0~5 V 단일극 입력               */
    /* ------------------------------------------------------------------ */
    if (MCP3465R_Init(&adc) != MCP3465R_OK) {
        printf("[ERROR] MCP3465R_Init failed\r\n");
        return;
    }
    printf("[OK] MCP3465R initialized (VREF=5V ext, OSR=4096, CH0 vs AGND)\r\n");

    /* ------------------------------------------------------------------ */
    /* 3. 자동 캘리브레이션                                                 */
    /*                                                                     */
    /*  Phase 1 – Offset:                                                  */
    /*    MUX → AGND/AGND, 64샘플 평균 → OFFSETCAL = –mean               */
    /*                                                                     */
    /*  Phase 2 – Gain:                                                    */
    /*    MUX → REFIN+/2 vs REFIN-/2 (ΔV = VREF/2)                       */
    /*    64샘플 평균 → GAINCAL = 0x400000 × 0x800000 / mean              */
    /*                                                                     */
    /*  완료 후: EN_OFFCAL + EN_GAINCAL 활성, MUX 복원 (CH0/AGND)         */
    /* ------------------------------------------------------------------ */
    printf("[CAL] Starting auto-calibration...\r\n");

    MCP3465R_Status_t cal_status = MCP3465R_AutoCalibrate(&adc, &g_cal_result);

    if (cal_status == MCP3465R_OK) {
        printf("[CAL] Offset cal : raw error = %ld LSB, OFFSETCAL = 0x%06lX\r\n",
               (long)g_cal_result.offset_raw,
               (unsigned long)(g_cal_result.offsetcal_val & 0xFFFFFF));
        printf("[CAL] Gain   cal : measured  = %ld LSB, GAINCAL  = 0x%06lX\r\n",
               (long)g_cal_result.gain_raw,
               (unsigned long)g_cal_result.gaincal_val);
        printf("[CAL] Calibration complete. Both registers active.\r\n");
    } else if (cal_status == MCP3465R_ERR_CAL) {
        /*
         * GAIN 캘리브레이션 실패 원인 점검 목록:
         *  - REFIN+ 에 5 V 정밀 기준 전압이 연결되어 있지 않음
         *  - REFIN- 가 AGND 에 연결되어 있지 않음
         *  - AVDD 가 5 V 인지 확인 (입력 범위: 0 ~ AVDD)
         *  - 측정 코드가 기대값(0x400000 = 2.5 V / 5 V × 2^23) ±15 % 범위를 벗어남
         */
        printf("[ERROR] Gain calibration sanity check failed. "
               "Check REFIN+/- wiring (REFIN+=5V, REFIN-=GND). "
               "measured=%ld, expected=0x400000\r\n",
               (long)g_cal_result.gain_raw);
        printf("[WARN] Continuing with offset-only correction.\r\n");
    } else {
        printf("[ERROR] Calibration SPI error (code=%d)\r\n", (int)cal_status);
        return;
    }

    /* ------------------------------------------------------------------ */
    /* 4. 캘리브레이션 결과를 NVM에 저장하는 예제 (의사 코드)               */
    /* ------------------------------------------------------------------ */
    /*
     * Flash_WriteCalibration(&g_cal_result, sizeof(g_cal_result));
     *
     * 전원 재공급 후 재측정 없이 복원하려면:
     *   Flash_ReadCalibration(&g_cal_result, sizeof(g_cal_result));
     *   MCP3465R_Init(&adc);
     *   MCP3465R_ApplyStoredCal(&adc, &g_cal_result);
     */

    /* ------------------------------------------------------------------ */
    /* 5. 보정된 ADC 값 연속 읽기 루프                                       */
    /*                                                                     */
    /*  ReadADC()가 반환하는 code에는 이미 하드웨어가 적용한               */
    /*  OFFSETCAL + GAINCAL 보정이 포함되어 있음                           */
    /*  입력 범위: 0 ~ 5 V (CH0 vs AGND, 단일극)                          */
    /* ------------------------------------------------------------------ */
    printf("[RUN] Reading calibrated ADC values (CH0 vs AGND, 0-5V range):\r\n");
    printf("      code range: 0x000000 (0V) ~ 0x7FFFFF (5V)\r\n");

    uint32_t sample_idx = 0;

    while (1) {
        int32_t code;
        MCP3465R_Status_t rd_st = MCP3465R_ReadADC(&adc, &code);

        if (rd_st == MCP3465R_OK) {
            float mv = adc_code_to_mv(code);

            /* 0~5 V 범위 클램핑 (노이즈로 인한 미소 범위 이탈 처리) */
            if (mv < ADC_INPUT_MIN_MV) mv = ADC_INPUT_MIN_MV;
            if (mv > ADC_INPUT_MAX_MV) mv = ADC_INPUT_MAX_MV;

            float pct = mv / ADC_INPUT_MAX_MV * 100.0f;

            printf("[%4lu] code=0x%06lX (%8ld)  %7.2f mV  (%5.2f %%)\r\n",
                   (unsigned long)sample_idx,
                   (unsigned long)(code & 0xFFFFFF),
                   (long)code,
                   mv, pct);
        } else if (rd_st == MCP3465R_ERR_TIMEOUT) {
            printf("[%4lu] Timeout waiting for data ready\r\n",
                   (unsigned long)sample_idx);
        } else {
            printf("[%4lu] SPI error\r\n", (unsigned long)sample_idx);
        }

        sample_idx++;
        HAL_Delay(100);     /* 100 ms 간격으로 출력 (선택 사항) */
    }
}
