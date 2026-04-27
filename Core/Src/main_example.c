/**
 * main_example.c
 * STM32L552 + MCP3465R 캘리브레이션 통합 예시
 *
 * STM32CubeIDE에서 생성된 main.c의 USER CODE 섹션에 삽입하여 사용
 *
 * SPI1 설정 (STM32CubeMX):
 *   Mode: Full-Duplex Master
 *   Data Size: 8 bits
 *   Clock Polarity: LOW (CPOL=0)
 *   Clock Phase: 1 Edge (CPHA=0)    ← SPI Mode 0
 *   Prescaler: SPI clock ≤ 10 MHz (MCP3465R 최대 FSCK)
 *   NSS: Software (CS는 GPIO로 수동 제어)
 *
 * GPIO 설정:
 *   PA4: Output Push-Pull, No Pull, High Speed → CS (초기값 HIGH)
 */

#include "main.h"          /* STM32CubeIDE 자동 생성 헤더 */
#include "mcp3465r.h"
#include "adc_calibration.h"
#include <stdio.h>         /* printf (semihosting 또는 UART redirect) */

/* STM32CubeIDE 자동 생성 핸들 (main.c에 선언됨) */
extern SPI_HandleTypeDef hspi1;

/* ------------------------------------------------------------------ */
/*  전역 객체                                                             */
/* ------------------------------------------------------------------ */
static MCP3465R_Handle_t g_adc = {
    .hspi     = &hspi1,
    .cs_port  = GPIOA,
    .cs_pin   = GPIO_PIN_4,
    .dev_addr = 0x00U,    /* A1=GND, A0=GND */
    .config3  = 0x30U,    /* 초기값 (Init에서 덮어씀) */
};

static ADC_CalData_t g_cal;

/* ------------------------------------------------------------------ */
/*  온도 모니터링용 (재캘리브레이션 트리거)                                   */
/* ------------------------------------------------------------------ */
static float g_last_cal_temp_c = -999.0f;
#define CAL_TEMP_THRESHOLD_C  10.0f

/* MCU 내부 온도 센서 읽기 (ADC1_IN17, Vtemp 공식 참고)                    */
/* 실제 구현 시 STM32CubeMX로 ADC1 CH17 활성화 필요                        */
static float read_mcu_temperature(void)
{
    /* 예시: 실제 프로젝트에서는 HAL_ADC_Start/PollForConversion 사용 */
    return 25.0f;
}

/* ------------------------------------------------------------------ */
/*  캘리브레이션 초기화 흐름 (main() USER CODE BEGIN 2 에 삽입)             */
/* ------------------------------------------------------------------ */
void ADC_System_Init(void)
{
    HAL_StatusTypeDef ret;

    /* ADC 초기화 */
    ret = MCP3465R_Init(&g_adc);
    if (ret != HAL_OK) {
        /* 초기화 실패: 시스템 에러 처리 */
        Error_Handler();
    }

    /* Flash에서 캘리브레이션 값 로드 시도 */
    if (ADC_Cal_LoadFromFlash(&g_cal) == HAL_OK) {
        /* 유효한 캘리브레이션 값 존재 → ADC 레지스터에 적용 */
        ret = ADC_Cal_Apply(&g_adc, &g_cal);
        if (ret != HAL_OK) Error_Handler();
    } else {
        /* 저장된 값 없음(최초 구동) → 새로 캘리브레이션 실행 */
        ret = ADC_Cal_RunAll(&g_adc, &g_cal);
        if (ret != HAL_OK) Error_Handler();

        /* Flash에 저장 */
        ret = ADC_Cal_SaveToFlash(&g_cal);
        if (ret != HAL_OK) Error_Handler();
    }

    g_last_cal_temp_c = read_mcu_temperature();
}

/* ------------------------------------------------------------------ */
/*  메인 루프에서 호출: 측정 + 주기적 재캘리브레이션 체크                      */
/* ------------------------------------------------------------------ */
void ADC_System_Process(void)
{
    int32_t  raw;
    float    voltage;
    float    current_temp;

    /* 전압 측정 */
    if (ADC_Cal_ReadVoltageRaw(&g_adc, &raw, 200U) == HAL_OK) {
        voltage = ADC_Cal_ConvertToVoltage(raw);
        /* 결과 사용 예시 */
        printf("Vin = %.4f V (raw = %ld)\r\n", (double)voltage, (long)raw);
    }

    /* 온도 변화 감지 → 자동 재캘리브레이션 */
    current_temp = read_mcu_temperature();
    if ((current_temp - g_last_cal_temp_c) > CAL_TEMP_THRESHOLD_C ||
        (g_last_cal_temp_c - current_temp) > CAL_TEMP_THRESHOLD_C) {

        if (ADC_Cal_RunAll(&g_adc, &g_cal) == HAL_OK) {
            ADC_Cal_SaveToFlash(&g_cal);
            g_last_cal_temp_c = current_temp;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  강제 재캘리브레이션 (버튼/UART 명령으로 호출)                             */
/* ------------------------------------------------------------------ */
void ADC_ForceRecalibrate(void)
{
    if (ADC_Cal_RunAll(&g_adc, &g_cal) == HAL_OK) {
        ADC_Cal_SaveToFlash(&g_cal);
        g_last_cal_temp_c = read_mcu_temperature();
    }
}

/*
 * main() 통합 예시:
 *
 * int main(void)
 * {
 *     HAL_Init();
 *     SystemClock_Config();
 *     MX_GPIO_Init();
 *     MX_SPI1_Init();            // STM32CubeMX 생성
 *
 *     ADC_System_Init();         // MCP3465R 초기화 + 캘리브레이션
 *
 *     while (1)
 *     {
 *         ADC_System_Process();  // 측정 + 자동 재캘리브레이션 체크
 *         HAL_Delay(100);
 *     }
 * }
 */
