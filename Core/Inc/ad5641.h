/**
 * @file    ad5641.h
 * @brief   AD5641 14-bit DAC SPI 드라이버
 *
 * 최대 4개의 AD5641을 단일 SPI 버스에 공유, CS 핀으로 각각 선택.
 *
 * 하드웨어 연결 (SPI1):
 *   PA5 — SPI1_SCK
 *   PA7 — SPI1_MOSI  (AD5641은 수신 전용, MISO 불필요)
 *   PB0 — CS_DAC_CH0 (Active-LOW)
 *   PB1 — CS_DAC_CH1
 *   PB2 — CS_DAC_CH2
 *   PB10 — CS_DAC_CH3
 *
 * SPI 설정: Mode 0 (CPOL=0, CPHA=0), MSB First, 8-bit, ~8 MHz
 */
#ifndef INC_AD5641_H_
#define INC_AD5641_H_

#include "stm32l5xx_hal.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  상수 정의                                                            */
/* ------------------------------------------------------------------ */
#define AD5641_MAX_CHANNELS     4U
#define AD5641_RESOLUTION       14U
#define AD5641_MAX_CODE         ((1U << AD5641_RESOLUTION) - 1U)  /* 16383 */

/** 파워-다운 모드 (16-bit 프레임의 [15:14] 비트) */
#define AD5641_PD_NORMAL        0x0000U  /**< 정상 동작 */
#define AD5641_PD_1K            0x4000U  /**< 1 kΩ to GND */
#define AD5641_PD_100K          0x8000U  /**< 100 kΩ to GND */
#define AD5641_PD_HIIZ          0xC000U  /**< Hi-Z */

/* ------------------------------------------------------------------ */
/*  핸들 구조체                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *cs_port[AD5641_MAX_CHANNELS]; /**< CS GPIO 포트 배열 */
    uint16_t      cs_pin [AD5641_MAX_CHANNELS]; /**< CS GPIO 핀 번호 배열 */

    float    vref;          /**< 기준 전압 [V] */
    float    gain;          /**< 출력 게인: 1.0 또는 2.0 */

    uint16_t current_code[AD5641_MAX_CHANNELS]; /**< 현재 설정 코드 (읽기용) */
} AD5641_HandleTypeDef;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief AD5641 드라이버 초기화
 * @param hdev  AD5641 핸들
 * @param hspi  STM32 HAL SPI 핸들
 * @param vref  기준 전압 [V]  (예: 2.5 또는 3.3)
 * @param gain  출력 게인      (예: 1.0 또는 2.0)
 */
HAL_StatusTypeDef AD5641_Init(AD5641_HandleTypeDef *hdev,
                               SPI_HandleTypeDef   *hspi,
                               float vref, float gain);

/**
 * @brief 채널별 CS 핀 등록 및 초기 HIGH 설정
 */
HAL_StatusTypeDef AD5641_SetCS(AD5641_HandleTypeDef *hdev,
                                uint8_t channel,
                                GPIO_TypeDef *port, uint16_t pin);

/**
 * @brief 14-bit 코드 직접 출력
 * @param code  0 ~ 16383
 */
HAL_StatusTypeDef AD5641_WriteCode(AD5641_HandleTypeDef *hdev,
                                    uint8_t channel, uint16_t code);

/**
 * @brief 전압값으로 출력 설정
 * @param voltage  출력 전압 [V]  (0 ~ vref*gain 범위로 클램핑)
 */
HAL_StatusTypeDef AD5641_SetVoltage(AD5641_HandleTypeDef *hdev,
                                     uint8_t channel, float voltage);

/**
 * @brief 파워-다운 모드 설정
 * @param pd_mode  AD5641_PD_NORMAL / AD5641_PD_1K / AD5641_PD_100K / AD5641_PD_HIIZ
 */
HAL_StatusTypeDef AD5641_PowerDown(AD5641_HandleTypeDef *hdev,
                                    uint8_t channel, uint16_t pd_mode);

/** 코드 → 전압 변환 유틸 */
float    AD5641_CodeToVoltage(AD5641_HandleTypeDef *hdev, uint16_t code);
/** 전압 → 코드 변환 유틸 */
uint16_t AD5641_VoltageToCode(AD5641_HandleTypeDef *hdev, float voltage);

#endif /* INC_AD5641_H_ */
