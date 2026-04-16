/**
 * @file    main.h
 * @brief   4채널 PID 전압 제어 애플리케이션 헤더
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                     핀 배치 요약                                 │
 * ├───────────────┬──────────────────┬──────────────────────────────┤
 * │ 기능          │ STM32L552R 핀    │ 비고                         │
 * ├───────────────┼──────────────────┼──────────────────────────────┤
 * │ SPI1_SCK      │ PA5              │ AD5641 (DAC) 클럭            │
 * │ SPI1_MOSI     │ PA7              │ AD5641 데이터 입력           │
 * │ DAC_CS_CH0    │ PA4              │ AD5641 채널 0 /SYNC          │
 * │ DAC_CS_CH1    │ PB0              │ AD5641 채널 1 /SYNC          │
 * │ DAC_CS_CH2    │ PC0              │ AD5641 채널 2 /SYNC          │
 * │ DAC_CS_CH3    │ PC1              │ AD5641 채널 3 /SYNC          │
 * ├───────────────┼──────────────────┼──────────────────────────────┤
 * │ SPI2_SCK      │ PB13             │ MCP3465R (ADC) 클럭          │
 * │ SPI2_MISO     │ PB14             │ MCP3465R 데이터 출력         │
 * │ SPI2_MOSI     │ PB15             │ MCP3465R 데이터 입력         │
 * │ ADC_CS        │ PB12             │ MCP3465R /CS                 │
 * └───────────────┴──────────────────┴──────────────────────────────┘
 *
 * STM32CubeIDE 설정 요약
 * ───────────────────────
 *  SPI1: Mode=Transmit Only Master, CPOL=High, CPHA=2Edge,
 *        DataSize=8bit, Prescaler=적절히 (≤30MHz AD5641 최대)
 *  SPI2: Mode=Full-Duplex Master, CPOL=Low, CPHA=1Edge,
 *        DataSize=8bit, Prescaler=적절히 (≤20MHz MCP3465R 최대)
 *  GPIO: PA4/PB0/PC0/PC1/PB12 → Output Push-Pull, 초기값=High
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"
#include "ad5641.h"
#include "mcp3465r.h"
#include "pid.h"

/* ── 제어 루프 파라미터 ───────────────────────────────────────────── */
#define CTRL_SAMPLE_MS      10U         /**< PID 제어 주기 [ms] */
#define CTRL_SAMPLE_SEC     (CTRL_SAMPLE_MS / 1000.0f)  /**< 제어 주기 [s] */
#define NUM_CHANNELS        4U          /**< 제어 채널 수 */

/* ── 기본 PID 게인 ────────────────────────────────────────────────── */
/* 실제 시스템에서 Ziegler-Nichols 또는 시행착오로 튜닝 필요          */
#define PID_KP_DEFAULT      1.5f
#define PID_KI_DEFAULT      0.8f
#define PID_KD_DEFAULT      0.05f

/* ── 기본 목표 전압 [V] ──────────────────────────────────────────── */
#define DEFAULT_SETPOINT_CH0    2.5f
#define DEFAULT_SETPOINT_CH1    2.5f
#define DEFAULT_SETPOINT_CH2    2.5f
#define DEFAULT_SETPOINT_CH3    2.5f

/* ── CS 핀 정의 ───────────────────────────────────────────────────── */
#define DAC_CS_CH0_PORT     GPIOA
#define DAC_CS_CH0_PIN      GPIO_PIN_4
#define DAC_CS_CH1_PORT     GPIOB
#define DAC_CS_CH1_PIN      GPIO_PIN_0
#define DAC_CS_CH2_PORT     GPIOC
#define DAC_CS_CH2_PIN      GPIO_PIN_0
#define DAC_CS_CH3_PORT     GPIOC
#define DAC_CS_CH3_PIN      GPIO_PIN_1

#define ADC_CS_PORT         GPIOB
#define ADC_CS_PIN          GPIO_PIN_12

/* ── 전역 상태 구조체 ─────────────────────────────────────────────── */
typedef struct
{
    float setpoint[NUM_CHANNELS];       /**< 목표 전압 [V] */
    float measured[NUM_CHANNELS];       /**< 측정 전압 [V] */
    float output[NUM_CHANNELS];         /**< DAC 출력 전압 [V] */
    float error[NUM_CHANNELS];          /**< 현재 오차 [V] */
    PID_TypeDef  pid[NUM_CHANNELS];     /**< PID 인스턴스 */
} ControlState_t;

/* ── 외부 선언 (CubeMX 생성 핸들) ───────────────────────────────── */
extern SPI_HandleTypeDef hspi1;         /**< AD5641용 SPI1 */
extern SPI_HandleTypeDef hspi2;         /**< MCP3465R용 SPI2 */

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
