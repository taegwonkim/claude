/**
 * @file    mcp3465r.h
 * @brief   MCP3465R 24-bit Delta-Sigma ADC SPI 드라이버
 *
 * 4채널 단일 종단(single-ended) 전압 측정.
 * 내부 2.4V 기준 전압, 내부 클럭 사용.
 * One-shot 변환 모드: MUX 설정 → 변환 시작 → IRQ 대기 → 데이터 읽기.
 *
 * 하드웨어 연결 (SPI2):
 *   PB13 — SPI2_SCK
 *   PB14 — SPI2_MISO
 *   PB15 — SPI2_MOSI
 *   PC7  — CS_ADC    (Active-LOW)
 *   PC0  — ADC_INT   (Active-LOW, EXTI 하강 에지 인터럽트)
 *
 * SPI 설정: Mode 0 (CPOL=0, CPHA=0), MSB First, 8-bit, ~8 MHz
 */
#ifndef INC_MCP3465R_H_
#define INC_MCP3465R_H_

#include "stm32l5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  디바이스 주소 (MCP3465R 고정값)                                       */
/* ------------------------------------------------------------------ */
#define MCP3465R_DEV_ADDR           0x01U  /**< DEV_ADDR[1:0] = 0b01 */

/* ------------------------------------------------------------------ */
/*  레지스터 주소                                                         */
/* ------------------------------------------------------------------ */
#define MCP3465R_REG_ADCDATA        0x00U
#define MCP3465R_REG_CONFIG0        0x01U
#define MCP3465R_REG_CONFIG1        0x02U
#define MCP3465R_REG_CONFIG2        0x03U
#define MCP3465R_REG_CONFIG3        0x04U
#define MCP3465R_REG_IRQ            0x05U
#define MCP3465R_REG_MUX            0x06U
#define MCP3465R_REG_SCAN           0x07U
#define MCP3465R_REG_TIMER          0x08U
#define MCP3465R_REG_OFFSETCAL      0x09U
#define MCP3465R_REG_GAINCAL        0x0AU
#define MCP3465R_REG_LOCK           0x0DU
#define MCP3465R_REG_CRCCFG         0x0FU

/* ------------------------------------------------------------------ */
/*  커맨드 바이트 빌더                                                     */
/*  형식: [DEV_ADDR(7:6) | REG_ADDR(5:2) | CMD_TYPE(1:0)]              */
/* ------------------------------------------------------------------ */
#define MCP3465R_CMD_BYTE(reg, cmd) \
    ((uint8_t)(((MCP3465R_DEV_ADDR) << 6) | ((reg) << 2) | (cmd)))

#define MCP3465R_CMD_INCR_WRITE     0x02U   /**< 증분 쓰기 */
#define MCP3465R_CMD_INCR_READ      0x03U   /**< 증분 읽기 */

/* Fast command 바이트 (REG_ADDR 필드에 fast-cmd 코드 인코딩) */
#define MCP3465R_FAST_ADCSTART      0x68U   /**< ADC 변환 시작/재시작 */
#define MCP3465R_FAST_STANDBY       0x6CU   /**< 대기 모드 */
#define MCP3465R_FAST_SHUTDOWN      0x70U   /**< 셧다운 */
#define MCP3465R_FAST_FULL_RESET    0x78U   /**< 전체 리셋 */
#define MCP3465R_FAST_SPI_RESET     0x7CU   /**< SPI 리셋 */

/* ------------------------------------------------------------------ */
/*  CONFIG0 비트 필드                                                    */
/* ------------------------------------------------------------------ */
#define MCP3465R_CFG0_VREF_INT      (1U << 7)   /**< 내부 2.4V Vref 활성화 */
#define MCP3465R_CFG0_CLK_INT       (0x3U << 4) /**< 내부 클럭, 핀 출력 없음 */
#define MCP3465R_CFG0_CS_NONE       (0x0U << 2) /**< 전류 소스 없음 */
#define MCP3465R_CFG0_MODE_STANDBY  (0x2U << 0) /**< 대기 모드 */
#define MCP3465R_CFG0_MODE_CONV     (0x3U << 0) /**< 연속 변환 모드 */

/* ------------------------------------------------------------------ */
/*  CONFIG1 — OSR 설정                                                  */
/* ------------------------------------------------------------------ */
#define MCP3465R_CFG1_PRE_1         (0x0U << 6) /**< AMCLK = MCLK (÷1) */
#define MCP3465R_CFG1_OSR_256       (0x3U << 2) /**< OSR=256  ≈ 0.5ms/ch  */
#define MCP3465R_CFG1_OSR_512       (0x4U << 2) /**< OSR=512  ≈ 1ms/ch   */
#define MCP3465R_CFG1_OSR_1024      (0x5U << 2) /**< OSR=1024 ≈ 2ms/ch   */

/* ------------------------------------------------------------------ */
/*  CONFIG2 — Boost/Gain                                               */
/* ------------------------------------------------------------------ */
#define MCP3465R_CFG2_BOOST_1X      (0x2U << 6) /**< 전류 부스트 x1 */
#define MCP3465R_CFG2_GAIN_1X       (0x1U << 3) /**< 게인 x1 */
#define MCP3465R_CFG2_AZ_MUX_OFF    (0U   << 2) /**< Auto-zero MUX 비활성 */

/* ------------------------------------------------------------------ */
/*  CONFIG3 — 변환 모드 / 데이터 포맷                                     */
/* ------------------------------------------------------------------ */
#define MCP3465R_CFG3_CONV_ONESHOT  (0x0U << 6) /**< One-shot → standby */
#define MCP3465R_CFG3_FMT_32_CHID  (0x3U << 4) /**< 32-bit, CH_ID 포함, 우측 정렬 */
#define MCP3465R_CFG3_CRC_OFF       (0x0U << 2)
#define MCP3465R_CFG3_OFFCAL_OFF    (0U   << 1)
#define MCP3465R_CFG3_GAINCAL_OFF   (0U   << 0)

/* ------------------------------------------------------------------ */
/*  IRQ 레지스터 (쓰기 가능 필드)                                          */
/* ------------------------------------------------------------------ */
#define MCP3465R_IRQ_MODE_IRQHI     (0x2U << 2) /**< 비활성=HIGH, 활성=LOW */
#define MCP3465R_IRQ_EN_FASTCMD     (1U   << 1) /**< Fast command 활성화 */

/* ------------------------------------------------------------------ */
/*  MUX 채널 코드 (단일 종단: AINx vs AGND)                               */
/* ------------------------------------------------------------------ */
#define MCP3465R_MUX_SE(ch)  ((uint8_t)(((ch) << 4) | 0x08U))
    /* ch=0→0x08, ch=1→0x18, ch=2→0x28, ch=3→0x38 */

#define MCP3465R_NUM_CHANNELS       4U
#define MCP3465R_VREF_INTERNAL      2.4f    /**< 내부 기준 전압 [V] */
#define MCP3465R_FULLSCALE_CODE     8388608 /**< 2^23 */

/* ------------------------------------------------------------------ */
/*  핸들 구조체                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *cs_port;  /**< CS GPIO 포트 */
    uint16_t      cs_pin;   /**< CS GPIO 핀 */
    GPIO_TypeDef *int_port; /**< INT GPIO 포트 (IRQ 핀) */
    uint16_t      int_pin;  /**< INT GPIO 핀 */

    float vref;             /**< 기준 전압 [V] (내부=2.4V) */

    /* 읽기 결과 캐시 */
    int32_t  raw_data[MCP3465R_NUM_CHANNELS]; /**< 최근 원시 ADC 코드 */
    bool     data_valid[MCP3465R_NUM_CHANNELS];

    uint32_t conv_timeout_ms; /**< 변환 대기 최대 시간 [ms] */
} MCP3465R_HandleTypeDef;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief MCP3465R 초기화 및 레지스터 설정
 */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_HandleTypeDef *hdev,
                                 SPI_HandleTypeDef       *hspi,
                                 GPIO_TypeDef *cs_port,  uint16_t cs_pin,
                                 GPIO_TypeDef *int_port, uint16_t int_pin);

/**
 * @brief Fast command 전송 (ADCSTART, STANDBY, RESET 등)
 */
HAL_StatusTypeDef MCP3465R_FastCmd(MCP3465R_HandleTypeDef *hdev, uint8_t cmd);

/**
 * @brief 레지스터 단일 바이트 쓰기 (1바이트 레지스터용)
 */
HAL_StatusTypeDef MCP3465R_WriteReg8(MCP3465R_HandleTypeDef *hdev,
                                      uint8_t reg, uint8_t value);

/**
 * @brief 레지스터 증분 쓰기 (복수 바이트)
 */
HAL_StatusTypeDef MCP3465R_WriteRegs(MCP3465R_HandleTypeDef *hdev,
                                      uint8_t reg, const uint8_t *data, uint8_t len);

/**
 * @brief 레지스터 증분 읽기
 */
HAL_StatusTypeDef MCP3465R_ReadRegs(MCP3465R_HandleTypeDef *hdev,
                                     uint8_t reg, uint8_t *data, uint8_t len);

/**
 * @brief MUX 채널 선택 (단일 종단: AINch vs AGND)
 * @param channel  0~3
 */
HAL_StatusTypeDef MCP3465R_SetMuxChannel(MCP3465R_HandleTypeDef *hdev, uint8_t channel);

/**
 * @brief 변환 시작 (Fast ADC Start command)
 */
HAL_StatusTypeDef MCP3465R_StartConversion(MCP3465R_HandleTypeDef *hdev);

/**
 * @brief INT 핀 폴링으로 변환 완료 대기 (FreeRTOS osDelay 사용)
 * @param timeout_ms 최대 대기 시간 [ms]
 */
HAL_StatusTypeDef MCP3465R_WaitReady(MCP3465R_HandleTypeDef *hdev, uint32_t timeout_ms);

/**
 * @brief ADCDATA 레지스터에서 32-bit 변환 결과 읽기
 * @param[out] ch_id    채널 ID (0~3), NULL 허용
 * @param[out] raw_data 부호 있는 24-bit 원시 코드
 */
HAL_StatusTypeDef MCP3465R_ReadConversionResult(MCP3465R_HandleTypeDef *hdev,
                                                  uint8_t *ch_id, int32_t *raw_data);

/**
 * @brief 지정 채널 단일 변환 수행 (MUX 설정 → 시작 → 대기 → 읽기 일괄 처리)
 * @param[out] voltage 변환된 전압 [V]
 */
HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_HandleTypeDef *hdev,
                                        uint8_t channel, float *voltage);

/**
 * @brief 4개 채널 순차 읽기, hdev->raw_data[] 및 hdev->data_valid[] 갱신
 */
HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_HandleTypeDef *hdev,
                                            float voltage[MCP3465R_NUM_CHANNELS]);

/**
 * @brief 원시 코드 → 전압 변환
 */
float MCP3465R_RawToVoltage(MCP3465R_HandleTypeDef *hdev, int32_t raw);

#endif /* INC_MCP3465R_H_ */
