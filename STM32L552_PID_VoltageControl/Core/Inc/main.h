/* main.h - STM32L552 4채널 PID 전압 제어
 *
 * 외부 DAC: AD5641 × 4  (SPI1, 각각 개별 CS)
 * 외부 ADC: MCP3465R × 1 (SPI2, 8채널 스캔)
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

/* ==========================================================
 * 프로젝트 버전
 * ========================================================== */
#define APP_VERSION_MAJOR    1
#define APP_VERSION_MINOR    1

/* ==========================================================
 * 채널 수
 * ========================================================== */
#define NUM_CHANNELS         4

/* ==========================================================
 * AD5641 DAC 설정
 *   - 14비트 DAC (코드 범위: 0 ~ 16383)
 *   - VREF = 3.3V (내부 기준 또는 외부 공급)
 *   - 출력 범위: 0 ~ VREF → 외부 앰프로 0~12V 로 증폭
 *   - SPI1, CPOL=1 CPHA=0 (Mode 2), 16bit 프레임
 * ========================================================== */
#define AD5641_FULL_SCALE     16383U      /* 2^14 - 1 */
#define AD5641_VREF_MV        3300U       /* DAC 기준전압 (mV) */

/* PD 비트 (상위 2비트): 00=normal, 01=1kΩ PD, 10=100kΩ PD, 11=3-state */
#define AD5641_CMD_NORMAL     0x0000U     /* 정상 동작 (PD1:PD0 = 00) */

/* ==========================================================
 * MCP3465R ADC 설정
 *   - 16비트 유효 해상도 (24비트 Δ-Σ 출력)
 *   - 8채널 단일 종단 스캔 (CH0~CH7)
 *   - CH0,2,4,6 = 전압 측정 / CH1,3,5,7 = 전류 측정
 *   - SPI2, CPOL=0 CPHA=0 (Mode 0), 8bit 프레임
 *   - /IRQ 핀 → EXTI 인터럽트
 * ========================================================== */
#define MCP3465R_DEV_ADDR     0x01U       /* A1=0, A0=1 (기본값) */
#define MCP3465R_VREF_MV      3300U       /* ADC 기준전압 (mV) */
/* 단일 종단 양수 풀스케일: 2^23 - 1 (24비트 부호 있음) */
#define MCP3465R_FULLSCALE    8388607UL

/* MCP3465R 레지스터 주소 */
#define MCP346X_REG_ADCDATA   0x00U
#define MCP346X_REG_CONFIG0   0x01U
#define MCP346X_REG_CONFIG1   0x02U
#define MCP346X_REG_CONFIG2   0x03U
#define MCP346X_REG_CONFIG3   0x04U
#define MCP346X_REG_IRQ       0x05U
#define MCP346X_REG_MUX       0x06U
#define MCP346X_REG_SCAN      0x07U
#define MCP346X_REG_TIMER     0x08U
#define MCP346X_REG_LOCK      0x0DU

/* 커맨드 타입 */
#define MCP346X_CMD_FAST      0x00U
#define MCP346X_CMD_SREAD     0x01U      /* Static Read */
#define MCP346X_CMD_IWRITE    0x02U      /* Incremental Write */
#define MCP346X_CMD_IREAD     0x03U      /* Incremental Read */

/* 커맨드 바이트 생성 매크로:
   [7:6]=DevAddr [5:2]=RegAddr [1:0]=CmdType */
#define MCP346X_CMD(reg, cmd) \
    ((uint8_t)(((MCP3465R_DEV_ADDR) << 6) | ((reg) << 2) | (cmd)))

/* Fast Command: Conversion Start */
#define MCP346X_FASTCMD_CONV_START \
    ((uint8_t)(((MCP3465R_DEV_ADDR) << 6) | 0x28U))

/* ==========================================================
 * 전압 측정 분압기 (0~12V → 0~3.3V)
 *   R1=27kΩ, R2=10kΩ  →  ratio = 10k / 37k
 * ========================================================== */
#define VOLT_DIV_NUM         10000UL
#define VOLT_DIV_DEN         37000UL

/* ==========================================================
 * 전류 측정 (INA219 또는 동등 회로)
 *   0~5A → 0~3.3V
 * ========================================================== */
#define CURRENT_MAX_MA       5000U

/* ==========================================================
 * 출력 전압 보호 임계값
 * ========================================================== */
#define OUTPUT_VOLT_MIN_MV   0U
#define OUTPUT_VOLT_MAX_MV   12000U
#define OVP_THRESHOLD_MV     13000U
#define OCP_THRESHOLD_MA     5500U

/* ==========================================================
 * SPI1 핀 (AD5641 DAC × 4)
 *   SCK  = PA5 (AF5)
 *   MOSI = PA7 (AF5)
 *   CS0~3= PB0~PB3 (GPIO Out, active LOW)
 * ========================================================== */
#define DAC_CS0_Pin          GPIO_PIN_0
#define DAC_CS0_Port         GPIOB
#define DAC_CS1_Pin          GPIO_PIN_1
#define DAC_CS1_Port         GPIOB
#define DAC_CS2_Pin          GPIO_PIN_2
#define DAC_CS2_Port         GPIOB
#define DAC_CS3_Pin          GPIO_PIN_3
#define DAC_CS3_Port         GPIOB

/* ==========================================================
 * SPI2 핀 (MCP3465R ADC × 1)
 *   SCK  = PB13 (AF5)
 *   MISO = PB14 (AF5)
 *   MOSI = PB15 (AF5)
 *   CS   = PB4  (GPIO Out, active LOW)
 *   /IRQ = PB5  (GPIO In, pull-up, EXTI falling edge)
 * ========================================================== */
#define ADC_CS_Pin           GPIO_PIN_4
#define ADC_CS_Port          GPIOB
#define ADC_IRQ_Pin          GPIO_PIN_5
#define ADC_IRQ_Port         GPIOB

/* ==========================================================
 * LED 핀
 * ========================================================== */
#define LED_GREEN_Pin        GPIO_PIN_7
#define LED_GREEN_GPIO_Port  GPIOC
#define LED_RED_Pin          GPIO_PIN_6
#define LED_RED_GPIO_Port    GPIOC

/* ==========================================================
 * HAL 핸들 외부 선언
 * ========================================================== */
extern SPI_HandleTypeDef  hspi1;    /* AD5641 DAC */
extern SPI_HandleTypeDef  hspi2;    /* MCP3465R ADC */
extern FDCAN_HandleTypeDef hfdcan1;
extern IWDG_HandleTypeDef  hiwdg;

/* ==========================================================
 * CS 핀 제어 인라인 매크로
 * ========================================================== */
#define DAC_CS_LOW(ch)  do { \
    GPIO_TypeDef *p[] = {DAC_CS0_Port, DAC_CS1_Port, \
                         DAC_CS2_Port, DAC_CS3_Port}; \
    uint16_t     b[] = {DAC_CS0_Pin,  DAC_CS1_Pin, \
                         DAC_CS2_Pin,  DAC_CS3_Pin}; \
    HAL_GPIO_WritePin(p[ch], b[ch], GPIO_PIN_RESET); } while(0)

#define DAC_CS_HIGH(ch) do { \
    GPIO_TypeDef *p[] = {DAC_CS0_Port, DAC_CS1_Port, \
                         DAC_CS2_Port, DAC_CS3_Port}; \
    uint16_t     b[] = {DAC_CS0_Pin,  DAC_CS1_Pin, \
                         DAC_CS2_Pin,  DAC_CS3_Pin}; \
    HAL_GPIO_WritePin(p[ch], b[ch], GPIO_PIN_SET); } while(0)

#define ADC_CS_LOW()    HAL_GPIO_WritePin(ADC_CS_Port, ADC_CS_Pin, GPIO_PIN_RESET)
#define ADC_CS_HIGH()   HAL_GPIO_WritePin(ADC_CS_Port, ADC_CS_Pin, GPIO_PIN_SET)

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */
void SystemClock_Config(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* __MAIN_H */
