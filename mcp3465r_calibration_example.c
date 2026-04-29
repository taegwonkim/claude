/* main.c 또는 application layer에서의 사용 예시 */

#include "mcp3465r_calibration.h"
#include <stdio.h>   /* UART printf 리다이렉트 가정 */

/* STM32CubeMX에서 생성된 SPI 핸들 */
extern SPI_HandleTypeDef hspi1;

/* MCP3465R 핸들 */
static MCP3465R_Handle_t adc = {
    .hspi     = &hspi1,
    .cs_port  = GPIOA,
    .cs_pin   = GPIO_PIN_4,
    .dev_addr = 0x01,   /* A0=VDD, A1=VSS → 0b01 */
};

/* -----------------------------------------------------------------------
 * 사용 예시 1: 단계별 수동 교정 (운영자가 전압 전환)
 * ----------------------------------------------------------------------- */
void Calibration_Manual_Example(void)
{
    MCP3465R_CalResult_t cal = {0};

    printf("[CAL] 0.5V를 ADC 입력에 연결하고 Enter를 누르세요...\r\n");
    /* UART 수신 대기 또는 버튼 입력 대기 */
    /* getchar(); */

    /* STEP 1: Offset 측정 */
    MCP3465R_RunOffsetCal(&adc, &cal);
    printf("[CAL] Offset 오차: %.4f V (코드: %ld)\r\n",
           cal.offset_voltage, (long)cal.offset_code);

    printf("[CAL] 4.5V로 전환 후 Enter를 누르세요...\r\n");
    /* getchar(); */

    /* STEP 2: Gain 측정 + Offset 적용 후 */
    MCP3465R_RunGainCal(&adc, &cal);
    printf("[CAL] Gain 오차: %.4f %% (GAINCAL=0x%06lX)\r\n",
           cal.gain_error * 100.0f, (unsigned long)cal.gain_code);

    /* STEP 3: 두 값 모두 레지스터에 적용 */
    MCP3465R_ApplyCalibration(&adc, &cal);
    printf("[CAL] 교정 완료. OFFSETCAL=0x%06lX, GAINCAL=0x%06lX\r\n",
           (unsigned long)(cal.offset_code & 0xFFFFFF),
           (unsigned long)cal.gain_code);

    /* 교정 결과를 Flash/EEPROM에 저장 (예시) */
    /* Flash_Save(FLASH_CAL_ADDR, &cal, sizeof(cal)); */
}

/* -----------------------------------------------------------------------
 * 사용 예시 2: 부팅 시 저장된 교정값 복원
 * ----------------------------------------------------------------------- */
void Calibration_Restore_Example(void)
{
    MCP3465R_CalResult_t cal = {0};

    /* Flash/EEPROM에서 읽기 */
    /* Flash_Load(FLASH_CAL_ADDR, &cal, sizeof(cal)); */

    /* 임시 하드코딩 값 예시 (실제는 저장된 값 사용) */
    cal.offset_code = -1200;      /* 실제 교정 후 저장한 값 */
    cal.gain_code   = 0x7FF000;   /* 실제 교정 후 저장한 값 */

    MCP3465R_ApplyCalibration(&adc, &cal);
    printf("[CAL] 저장된 교정값 복원 완료\r\n");
}

/* -----------------------------------------------------------------------
 * 사용 예시 3: 교정 후 실제 전압 변환
 *
 * ADC 코드 → 전압 변환 (교정은 하드웨어가 처리하므로 소프트웨어 수식 불변)
 * ----------------------------------------------------------------------- */
float ADC_CodeToVoltage(int32_t adc_code)
{
    /* OFFSETCAL, GAINCAL이 적용된 이후의 코드는 이 수식으로 전압 변환 */
    return ((float)adc_code / ADC_FULL_SCALE) * CAL_VREF;
}

void Measurement_Example(void)
{
    int32_t code    = MCP3465R_ReadADC_Average(&adc, 16);
    float   voltage = ADC_CodeToVoltage(code);
    printf("[MEAS] ADC Code: %ld, Voltage: %.4f V\r\n", (long)code, voltage);
}
