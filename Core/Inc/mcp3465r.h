/**
 * @file    mcp3465r.h
 * @brief   MCP3465R 16-bit Delta-Sigma ADC 드라이버 (4채널 단일 CS)
 *
 * 하드웨어 구성
 * ─────────────
 *  · STM32 SPI2 (CPOL=0 / CPHA=0 / MSB first / 8-bit frame)
 *  · 단일 /CS GPIO (PB12)
 *  · Vref = 5.0 V (외부 레퍼런스 공급 가정)
 *
 * SPI 커맨드 바이트 포맷
 * ─────────────────────────────────────────────────────
 *  [7:6] A1:A0   = 01  (MCP3465R 고정 디바이스 주소)
 *  [5:2] R3:R0   = 레지스터 주소
 *  [1:0] C1:C0   = 커맨드 타입
 *        00 = Fast command
 *        01 = Static Read
 *        10 = Incremental Write
 *        11 = Incremental Read
 * ─────────────────────────────────────────────────────
 *
 * 채널 단일단 측정 (Single-Ended, VIN- = AGND)
 *   CH0: MUX = 0x08   CH1: MUX = 0x18
 *   CH2: MUX = 0x28   CH3: MUX = 0x38
 *
 * 전압 변환 (단극성, OSR=256, GAIN=1×)
 *   ADC 출력: 16-bit signed (–32768 ~ +32767)
 *   단일단 양전압: 0x0000(0 V) ~ 0x7FFF(Vref)
 *   Vin [V] = (int16_t)code / 32768.0 * Vref
 */
#ifndef MCP3465R_H
#define MCP3465R_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"
#include <stdint.h>

/* ── 디바이스 주소 ────────────────────────────────────────────────── */
#define MCP3465R_DEV_ADDR       0x01U   /**< A1=0, A0=1 (R-suffix 고정) */

/* ── 레지스터 주소 ────────────────────────────────────────────────── */
#define MCP3465R_REG_ADCDATA    0x00U
#define MCP3465R_REG_CONFIG0    0x01U
#define MCP3465R_REG_CONFIG1    0x02U
#define MCP3465R_REG_CONFIG2    0x03U
#define MCP3465R_REG_CONFIG3    0x04U
#define MCP3465R_REG_IRQ        0x05U
#define MCP3465R_REG_MUX        0x06U
#define MCP3465R_REG_SCAN       0x07U
#define MCP3465R_REG_TIMER      0x08U
#define MCP3465R_REG_OFFSETCAL  0x09U
#define MCP3465R_REG_GAINCAL    0x0AU
#define MCP3465R_REG_LOCK       0x0DU
#define MCP3465R_REG_CRCCFG     0x0FU

/* ── 커맨드 타입 ──────────────────────────────────────────────────── */
#define MCP3465R_CMD_FAST       0x00U
#define MCP3465R_CMD_STATIC_R   0x01U
#define MCP3465R_CMD_INC_W      0x02U
#define MCP3465R_CMD_INC_R      0x03U

/* ── Fast 커맨드 바이트 ───────────────────────────────────────────── */
#define MCP3465R_FAST_ADC_START 0x68U   /**< 변환 시작/재시작 */
#define MCP3465R_FAST_STANDBY   0x6CU   /**< ADC 대기 */
#define MCP3465R_FAST_SHUTDOWN  0x70U   /**< ADC 셧다운 */
#define MCP3465R_FAST_RESET     0x78U   /**< Full Reset */

/* ── CONFIG0 ─────────────────────────────────────────────────────── */
/* [7]   VREF_SEL : 0=외부 Vref, 1=내부 Vref  → 외부 사용: 0 */
/* [6]   reserved                                                    */
/* [5:4] CLK_SEL  : 00=외부 CLK, 10=내부 AMCLK(출력없음)            */
/* [3:2] CS_SEL   : 전류원 없음 = 00                                 */
/* [1:0] ADC_MODE : 11=연속 변환                                     */
#define MCP3465R_CONFIG0_DEFAULT    0x02U   /**< 외부Vref, 내부CLK, 연속변환 */
/* bit[5:4]=00(외부CLK), bit[1:0]=11(연속변환) → 0x03
   내부CLK 사용 시 bit[5:4]=10 → 0x23
   여기서는 외부 CLK 입력을 가정하지 않고 내부 클럭 사용: */
#define MCP3465R_CONFIG0_INT_CLK_CONT  0x23U  /* 내부CLK + 연속변환 */

/* ── CONFIG1 ─────────────────────────────────────────────────────── */
/* [7:6] PRE   : 00=AMCLK prescaler÷1                               */
/* [5:2] OSR   : 0011=256× (약 3.8 kSPS @ 4.915MHz 내부CLK)         */
/* [1:0] reserved                                                    */
#define MCP3465R_CONFIG1_PRE1_OSR256    0x0CU   /**< PRE=1, OSR=256 */

/* ── CONFIG2 ─────────────────────────────────────────────────────── */
/* [7:6] BOOST : 10=1× (기본)                                       */
/* [5:3] GAIN  : 001=1×                                              */
/* [2]   AZ_MUX: 0=auto-zero MUX 비활성                             */
/* [1]   AZ_REF: 0=auto-zero REF 비활성                              */
/* [0]   reserved (0)                                                */
#define MCP3465R_CONFIG2_BOOST1_GAIN1   0x88U   /**< BOOST=1×, GAIN=1× */

/* ── CONFIG3 ─────────────────────────────────────────────────────── */
/* [7:6] CONV_MODE  : 11=연속 변환                                   */
/* [5:4] DATA_FORMAT: 11=32-bit + 채널ID + 부호확장                  */
/* [3:2] CRC_FORMAT : 00=CRC 비활성                                  */
/* [1]   EN_CRCCOM  : 0                                              */
/* [0]   EN_OFFCAL  : 0                                              */
#define MCP3465R_CONFIG3_CONT_32BIT_CHID  0xF0U  /**< 연속, 32bit+CHID */

/* ── IRQ ─────────────────────────────────────────────────────────── */
/* [7:6] reserved                                                    */
/* [5:4] IRQ_MODE: 11=IRQ핀 비활성 (High-Z)                         */
/* [3]   EN_FASTCMD: 1=Fast command 활성                             */
/* [2]   EN_STP: 1=변환 완료 시 STP 인터럽트 활성                    */
/* [1:0] reserved                                                    */
#define MCP3465R_IRQ_DEFAULT            0x37U   /**< IRQ핀 Hi-Z, Fast cmd 활성 */

/* ── MUX 채널 선택값 ─────────────────────────────────────────────── */
#define MCP3465R_MUX_CH0    0x0U
#define MCP3465R_MUX_CH1    0x1U
#define MCP3465R_MUX_CH2    0x2U
#define MCP3465R_MUX_CH3    0x3U
#define MCP3465R_MUX_AGND   0x8U    /**< AGND (VIN- 기준) */
#define MCP3465R_MUX_AVDD   0x9U

/** 단일단 VIN- = AGND 설정 매크로: MUX = (VIN+ << 4) | VIN- */
#define MCP3465R_MUX_SE(ch)   (uint8_t)(((ch) << 4) | MCP3465R_MUX_AGND)

/* ── 파라미터 ────────────────────────────────────────────────────── */
#define MCP3465R_VREF           5.0f    /**< 기준 전압 [V] */
#define MCP3465R_NUM_CH         4U      /**< 사용 채널 수 */
#define MCP3465R_FULL_SCALE     32768   /**< 2^15 (단극성 풀스케일 코드) */
#define MCP3465R_TIMEOUT_MS     10U     /**< SPI 타임아웃 [ms] */

/* ── 핸들 타입 ─────────────────────────────────────────────────────── */
typedef struct
{
    SPI_HandleTypeDef *hspi;        /**< SPI 핸들 */
    GPIO_TypeDef      *cs_port;     /**< CS GPIO 포트 */
    uint16_t           cs_pin;      /**< CS GPIO 핀 */
} MCP3465R_HandleTypeDef;

/* ── 공개 API ─────────────────────────────────────────────────────── */

/**
 * @brief  MCP3465R 초기화 (연속 변환 모드 설정)
 * @param  hdev  드라이버 핸들 포인터
 * @retval HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_HandleTypeDef *hdev);

/**
 * @brief  단일 채널 전압 읽기 (MUX 전환 후 변환 대기)
 * @param  hdev     드라이버 핸들
 * @param  channel  채널 번호 (0~3)
 * @param  voltage  측정된 전압 [V] (출력)
 * @retval HAL_OK / HAL_ERROR / HAL_TIMEOUT
 */
HAL_StatusTypeDef MCP3465R_ReadChannel(MCP3465R_HandleTypeDef *hdev,
                                        uint8_t  channel,
                                        float   *voltage);

/**
 * @brief  4채널 전압 순차 읽기
 * @param  hdev     드라이버 핸들
 * @param  voltages 채널별 측정 전압 배열 [V] (출력, 크기 4)
 * @retval HAL_OK (부분 실패 시에도 가능한 값 유지)
 */
HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_HandleTypeDef *hdev,
                                            float voltages[MCP3465R_NUM_CH]);

#ifdef __cplusplus
}
#endif

#endif /* MCP3465R_H */
