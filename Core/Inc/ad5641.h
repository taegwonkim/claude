/**
 * @file    ad5641.h
 * @brief   AD5641 14-bit DAC 드라이버 (4채널 개별 CS)
 *
 * 하드웨어 구성
 * ─────────────
 *  · STM32 SPI1 (CPOL=1 / CPHA=1 / MSB first / 16-bit frame)
 *  · 채널별 전용 /SYNC(CS) GPIO
 *  · Vref = 5.0 V (외부 레퍼런스 공급 가정)
 *
 * SPI 전송 포맷 (16비트)
 * ────────────────────────────────────────────────
 *  Bit 15-14 : PD1-PD0  (00 = Normal operation)
 *  Bit 13-0  : D13-D0   (14-bit DAC code)
 * ────────────────────────────────────────────────
 *
 * 전압 변환
 *   Vout = (code / 2^14) * Vref
 *   0.5 V → code = 1638  (= 0.5/5.0 * 16384)
 *   4.5 V → code = 14746 (= 4.5/5.0 * 16384)
 */
#ifndef AD5641_H
#define AD5641_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"
#include <stdint.h>

/* ── 파라미터 상수 ─────────────────────────────────────────────────── */
#define AD5641_NUM_CH       4U          /**< 사용 채널 수 */
#define AD5641_RESOLUTION   14U         /**< DAC 분해능 */
#define AD5641_MAX_CODE     16383U      /**< 2^14 - 1 */
#define AD5641_VREF         5.0f        /**< 기준 전압 [V] */
#define AD5641_VOUT_MIN     0.5f        /**< 출력 하한 [V] */
#define AD5641_VOUT_MAX     4.5f        /**< 출력 상한 [V] */

/* ── Power-Down 모드 ──────────────────────────────────────────────── */
#define AD5641_PD_NORMAL    0x00U       /**< 정상 동작 */
#define AD5641_PD_1K        0x01U       /**< 출력 1 kΩ to GND */
#define AD5641_PD_100K      0x02U       /**< 출력 100 kΩ to GND */
#define AD5641_PD_HIMP      0x03U       /**< 출력 Hi-Z */

/* ── 핀 매핑 ──────────────────────────────────────────────────────── */
/**
 * @brief STM32L552R 권장 핀 배치
 *
 *  SPI1_SCK  = PA5   SPI1_MOSI = PA7
 *  /SYNC_CH0 = PA4   /SYNC_CH1 = PB0
 *  /SYNC_CH2 = PC0   /SYNC_CH3 = PC1
 *
 *  MISO는 AD5641이 단방향(write-only)이므로 연결 불필요.
 */

/* ── 핸들 타입 ─────────────────────────────────────────────────────── */
typedef struct
{
    SPI_HandleTypeDef *hspi;                        /**< SPI 핸들 */
    GPIO_TypeDef      *cs_port[AD5641_NUM_CH];      /**< CS GPIO 포트 */
    uint16_t           cs_pin [AD5641_NUM_CH];      /**< CS GPIO 핀 */
} AD5641_HandleTypeDef;

/* ── 공개 API ─────────────────────────────────────────────────────── */

/**
 * @brief  AD5641 드라이버 초기화 (모든 채널 중간값 출력)
 * @param  hdev  드라이버 핸들 포인터
 * @retval HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef AD5641_Init(AD5641_HandleTypeDef *hdev);

/**
 * @brief  특정 채널에 전압 설정
 * @param  hdev     드라이버 핸들
 * @param  channel  채널 번호 (0~3)
 * @param  voltage  목표 전압 [V] (0.5V~4.5V 범위 클램프)
 * @retval HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef AD5641_SetVoltage(AD5641_HandleTypeDef *hdev,
                                    uint8_t channel,
                                    float   voltage);

/**
 * @brief  특정 채널에 DAC 코드 직접 설정
 * @param  hdev     드라이버 핸들
 * @param  channel  채널 번호 (0~3)
 * @param  code     DAC 코드 (0 ~ 16383)
 * @retval HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef AD5641_SetCode(AD5641_HandleTypeDef *hdev,
                                 uint8_t  channel,
                                 uint16_t code);

/**
 * @brief  전압값을 14-bit DAC 코드로 변환
 * @param  voltage  전압 [V]
 * @retval DAC 코드 (0 ~ 16383)
 */
uint16_t AD5641_VoltageToCode(float voltage);

/**
 * @brief  14-bit DAC 코드를 전압으로 변환
 * @param  code  DAC 코드 (0 ~ 16383)
 * @retval 전압 [V]
 */
float AD5641_CodeToVoltage(uint16_t code);

#ifdef __cplusplus
}
#endif

#endif /* AD5641_H */
