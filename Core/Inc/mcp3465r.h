/**
 * mcp3465r.h
 * MCP3465R 24-bit Delta-Sigma ADC SPI driver for STM32L552 (polling mode)
 *
 * Hardware:
 *   VREF = External 3.0V on REFIN+/REFIN-
 *   SPI1: PA5(SCK), PA6(MISO), PA7(MOSI), PA4(CS)
 *   Device address A1=0, A0=0 -> DEV_ADDR = 0x00
 */

#ifndef MCP3465R_H
#define MCP3465R_H

#include "stm32l5xx_hal.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Register addresses                                                  */
/* ------------------------------------------------------------------ */
#define MCP3465R_REG_ADCDATA    0x0U
#define MCP3465R_REG_CONFIG0    0x1U
#define MCP3465R_REG_CONFIG1    0x2U
#define MCP3465R_REG_CONFIG2    0x3U
#define MCP3465R_REG_CONFIG3    0x4U
#define MCP3465R_REG_IRQ        0x5U
#define MCP3465R_REG_MUX        0x6U
#define MCP3465R_REG_SCAN       0x7U
#define MCP3465R_REG_TIMER      0x8U
#define MCP3465R_REG_OFFSETCAL  0x9U
#define MCP3465R_REG_GAINCAL    0xAU
#define MCP3465R_REG_LOCK       0xDU

/* ------------------------------------------------------------------ */
/*  SPI command byte: [7:6]=DEV_ADDR, [5:2]=REG_ADDR, [1:0]=CMD_TYPE  */
/* ------------------------------------------------------------------ */
#define MCP3465R_CMD_FAST_WR    0x0U  /* Fast command (no data phase) */
#define MCP3465R_CMD_STATIC_RD  0x1U  /* Static read                  */
#define MCP3465R_CMD_INC_WR     0x2U  /* Incremental write            */
#define MCP3465R_CMD_INC_RD     0x3U  /* Incremental read             */

#define MCP3465R_MAKE_CMD(dev, reg, type) \
    (uint8_t)(((dev) << 6) | ((reg) << 2) | (type))

/* Fast command bytes (device addr 0x00) */
#define MCP3465R_FASTCMD_START   0x28U  /* Start/Restart conversion */
#define MCP3465R_FASTCMD_STANDBY 0x2CU  /* Standby mode             */
#define MCP3465R_FASTCMD_RESET   0x38U  /* Full reset               */

/* ------------------------------------------------------------------ */
/*  CONFIG0 (1 byte)                                                    */
/*  [7:6] VREF_SEL=00 (external REFIN+/-)                              */
/*  [5:4] CLK_SEL=00  (external SCK)                                   */
/*  [3:2] CS_SEL=00   (no current source)                              */
/*  [1:0] ADC_MODE=00 (standby; start via FastCMD)                     */
/* ------------------------------------------------------------------ */
#define MCP3465R_CONFIG0_VAL    0x00U

/* ------------------------------------------------------------------ */
/*  CONFIG1 (1 byte)                                                    */
/*  [7:6] PRE=00   (AMCLK = MCLK)                                      */
/*  [5:2] OSR       High-res for calibration / normal for measurement   */
/*  [1:0] reserved=00                                                   */
/* ------------------------------------------------------------------ */
#define MCP3465R_CONFIG1_OSR_CAL    0x3CU  /* OSR=98304 (최고 정밀도, 캘리브레이션용)  */
#define MCP3465R_CONFIG1_OSR_NORMAL 0x14U  /* OSR=256  (빠른 변환, 일반 측정용)       */

/* ------------------------------------------------------------------ */
/*  CONFIG2 (1 byte)                                                    */
/*  [7:6] BOOST=10 (1x)                                                 */
/*  [5:3] GAIN=001 (PGA 1x — Vadc 범위 0~VREF)                         */
/*  [2]   AZ_MUX=1 (Auto-Zero MUX enabled)                             */
/*  [1]   AZ_REF=1 (Auto-Zero REF enabled)                             */
/*  [0]   reserved=0                                                    */
/* ------------------------------------------------------------------ */
#define MCP3465R_CONFIG2_VAL    0x8EU  /* 0b_10_001_1_1_0 */

/* ------------------------------------------------------------------ */
/*  CONFIG3 (1 byte)                                                    */
/*  [7:6] CONV_MODE=00  (One-shot → Standby; polling에 적합)            */
/*  [5:4] DATA_FORMAT=11 (32-bit: SGN+CHID[3:0]+data[23:0])            */
/*  [3]   CRC_FORMAT=0                                                  */
/*  [2]   EN_CRCCOM=0                                                   */
/*  [1]   EN_OFFCAL=dynamic                                             */
/*  [0]   EN_GAINCAL=dynamic                                            */
/* ------------------------------------------------------------------ */
#define MCP3465R_CONFIG3_BASE       0x30U  /* CONV_MODE=00, DATA_FORMAT=11 */
#define MCP3465R_CONFIG3_EN_OFFCAL  (1U << 1)
#define MCP3465R_CONFIG3_EN_GAINCAL (1U << 0)

/* ------------------------------------------------------------------ */
/*  MUX channel selection (4-bit each)                                  */
/* ------------------------------------------------------------------ */
#define MCP3465R_MUX_CH0     0x0U
#define MCP3465R_MUX_CH1     0x1U
#define MCP3465R_MUX_CH2     0x2U
#define MCP3465R_MUX_CH3     0x3U
#define MCP3465R_MUX_AGND    0x8U
#define MCP3465R_MUX_AVDD    0x9U
#define MCP3465R_MUX_REFIN_P 0xBU
#define MCP3465R_MUX_REFIN_M 0xCU
#define MCP3465R_MUX_VSS     0xFU

#define MCP3465R_MAKE_MUX(vinp, vinn) (uint8_t)(((vinp) << 4) | (vinn))

/* GAINCAL 기본값 (= 1.0 unity gain) */
#define MCP3465R_GAINCAL_UNITY  0x800000U

/* 24-bit 최대 양수값 (VREF 풀스케일 기대값) */
#define MCP3465R_ADC_FULL_SCALE 0x7FFFFFL

/* 변환 타임아웃 (ms) */
#define MCP3465R_CONV_TIMEOUT_MS 500U

/* ------------------------------------------------------------------ */
/*  Handle                                                              */
/* ------------------------------------------------------------------ */
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    uint8_t            dev_addr;  /* 0~3; A1:A0 핀에 따라 결정 */
    uint8_t            config3;   /* CONFIG3 현재 값 (EN_OFFCAL/EN_GAINCAL 추적용) */
} MCP3465R_Handle_t;

/* ------------------------------------------------------------------ */
/*  Function prototypes                                                 */
/* ------------------------------------------------------------------ */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hdev);
HAL_StatusTypeDef MCP3465R_Reset(MCP3465R_Handle_t *hdev);

HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hdev, uint8_t reg,
                                     const uint8_t *data, uint8_t len);
HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *hdev, uint8_t reg,
                                    uint8_t *data, uint8_t len);

HAL_StatusTypeDef MCP3465R_SetMux(MCP3465R_Handle_t *hdev, uint8_t vinp, uint8_t vinn);
HAL_StatusTypeDef MCP3465R_SetConfig3(MCP3465R_Handle_t *hdev, uint8_t en_offcal,
                                       uint8_t en_gaincal);
HAL_StatusTypeDef MCP3465R_SetOSR(MCP3465R_Handle_t *hdev, uint8_t config1_val);

HAL_StatusTypeDef MCP3465R_StartConversion(MCP3465R_Handle_t *hdev);
HAL_StatusTypeDef MCP3465R_WaitReady(MCP3465R_Handle_t *hdev, uint32_t timeout_ms);
HAL_StatusTypeDef MCP3465R_ReadData(MCP3465R_Handle_t *hdev, int32_t *data,
                                     uint32_t timeout_ms);

HAL_StatusTypeDef MCP3465R_WriteOffsetCal(MCP3465R_Handle_t *hdev, int32_t offset_val);
HAL_StatusTypeDef MCP3465R_WriteGainCal(MCP3465R_Handle_t *hdev, uint32_t gain_val);

#endif /* MCP3465R_H */
