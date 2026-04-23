/**
 * mcp3465r.h
 *
 * MCP3465R 24-bit Delta-Sigma ADC driver with automatic offset/gain calibration.
 * Target: STM32 with HAL SPI (CPOL=0, CPHA=0 or CPOL=1, CPHA=1, MSB first).
 *
 * Calibration overview:
 *   1. Offset cal  : MUX -> AGND/AGND (0x88), measure error, write -error to OFFSETCAL
 *   2. Gain cal    : MUX -> REFIN+/2 vs REFIN-/2 (0xBC), expected code = 2^22,
 *                    GAINCAL = expected * 2^23 / measured
 */

#ifndef MCP3465R_H
#define MCP3465R_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"   /* Adjust to your HAL target (e.g. stm32f4xx_hal.h) */

/* -------------------------------------------------------------------------
 * Device address (A1:A0 pins, factory default = 0b01)
 * ------------------------------------------------------------------------- */
#define MCP3465R_DEV_ADDR           0x01U

/* -------------------------------------------------------------------------
 * SPI command types [T1:T0]
 * Command byte format: [A1:A0 | REG3:REG0 | T1:T0]
 * ------------------------------------------------------------------------- */
#define MCP3465R_CMD_FAST           0x00U   /* Fast command (ADC mode control) */
#define MCP3465R_CMD_STATIC_READ    0x01U   /* Static read  (single register)  */
#define MCP3465R_CMD_INC_WRITE      0x02U   /* Incremental write                */
#define MCP3465R_CMD_INC_READ       0x03U   /* Incremental read                 */

#define MCP3465R_CMD_BYTE(reg, type) \
    ((uint8_t)(((MCP3465R_DEV_ADDR) << 6) | (((uint8_t)(reg)) << 2) | ((uint8_t)(type))))

/* -------------------------------------------------------------------------
 * Register map
 * ------------------------------------------------------------------------- */
#define MCP3465R_REG_ADCDATA        0x00U   /* ADC result (1-4 bytes)    */
#define MCP3465R_REG_CONFIG0        0x01U   /* Configuration 0 (1 byte)  */
#define MCP3465R_REG_CONFIG1        0x02U   /* Configuration 1 (1 byte)  */
#define MCP3465R_REG_CONFIG2        0x03U   /* Configuration 2 (1 byte)  */
#define MCP3465R_REG_CONFIG3        0x04U   /* Configuration 3 (1 byte)  */
#define MCP3465R_REG_IRQ            0x05U   /* IRQ / status    (1 byte)  */
#define MCP3465R_REG_MUX            0x06U   /* MUX             (1 byte)  */
#define MCP3465R_REG_SCAN           0x07U   /* SCAN            (3 bytes) */
#define MCP3465R_REG_TIMER          0x08U   /* TIMER           (3 bytes) */
#define MCP3465R_REG_OFFSETCAL      0x09U   /* Offset cal      (3 bytes) */
#define MCP3465R_REG_GAINCAL        0x0AU   /* Gain cal        (3 bytes) */
#define MCP3465R_REG_LOCK           0x0DU   /* Password lock   (1 byte)  */
#define MCP3465R_REG_CRCCFG         0x0FU   /* CRC config      (2 bytes) */

/* -------------------------------------------------------------------------
 * CONFIG0 [7:0]
 * ------------------------------------------------------------------------- */
#define CONFIG0_VREF_EXT            0x00U   /* External VREF (no internal)   */
#define CONFIG0_VREF_INT            0x80U   /* Internal 2.4 V VREF           */
#define CONFIG0_CLK_EXT             0x00U   /* External clock on CLK pin     */
#define CONFIG0_CLK_INT             0x10U   /* Internal oscillator, no output*/
#define CONFIG0_CLK_INT_AMCLK       0x20U   /* Internal oscillator + AMCLK output */
#define CONFIG0_CS_NONE             0x00U   /* No bias current source        */
#define CONFIG0_CS_0_9UA            0x04U
#define CONFIG0_CS_3_7UA            0x08U
#define CONFIG0_CS_15UA             0x0CU
#define CONFIG0_MODE_SHUTDOWN       0x00U   /* ADC off                       */
#define CONFIG0_MODE_STANDBY        0x01U   /* Standby (bias on)             */
#define CONFIG0_MODE_ONESHOT        0x02U   /* One-shot, then standby        */
#define CONFIG0_MODE_CONTINUOUS     0x03U   /* Continuous conversion         */

/* -------------------------------------------------------------------------
 * CONFIG1 [7:0]
 * ------------------------------------------------------------------------- */
#define CONFIG1_PRE_1               0x00U   /* AMCLK prescaler = 1 */
#define CONFIG1_PRE_2               0x40U
#define CONFIG1_PRE_4               0x80U
#define CONFIG1_PRE_8               0xC0U
/* OSR (oversampling ratio) – bits [5:2] */
#define CONFIG1_OSR_32              0x00U
#define CONFIG1_OSR_64              0x04U
#define CONFIG1_OSR_128             0x08U
#define CONFIG1_OSR_256             0x0CU
#define CONFIG1_OSR_512             0x10U
#define CONFIG1_OSR_1024            0x14U
#define CONFIG1_OSR_2048            0x18U
#define CONFIG1_OSR_4096            0x1CU   /* ~10 ms/conversion @ 4.9 MHz */
#define CONFIG1_OSR_8192            0x20U
#define CONFIG1_OSR_16384           0x24U

/* -------------------------------------------------------------------------
 * CONFIG2 [7:0]
 * ------------------------------------------------------------------------- */
#define CONFIG2_BOOST_05            0x00U   /* 0.5× current (low power) */
#define CONFIG2_BOOST_066           0x40U
#define CONFIG2_BOOST_1             0x80U   /* Normal (default)         */
#define CONFIG2_BOOST_2             0xC0U   /* 2× (high speed)          */
#define CONFIG2_GAIN_THIRD          0x00U   /* PGA gain = 1/3  */
#define CONFIG2_GAIN_1              0x08U   /* PGA gain = 1    */
#define CONFIG2_GAIN_2              0x10U
#define CONFIG2_GAIN_4              0x18U
#define CONFIG2_GAIN_8              0x20U
#define CONFIG2_GAIN_16             0x28U
#define CONFIG2_GAIN_32             0x30U
#define CONFIG2_GAIN_64             0x38U
#define CONFIG2_AZ_MUX              0x04U   /* Auto-zero MUX (choppers)  */
#define CONFIG2_AZ_REF              0x02U   /* Auto-zero reference path  */

/* -------------------------------------------------------------------------
 * CONFIG3 [7:0]
 * ------------------------------------------------------------------------- */
#define CONFIG3_CONV_ONE_SHUTDOWN   0x00U   /* One-shot → shutdown       */
#define CONFIG3_CONV_ONE_STANDBY    0x40U   /* One-shot → standby        */
#define CONFIG3_CONV_CONTINUOUS     0xC0U   /* Continuous conversion     */
#define CONFIG3_DATA_24BIT          0x00U   /* 24-bit result             */
#define CONFIG3_DATA_32BIT_ZEROS    0x10U   /* 32-bit, zero-padded       */
#define CONFIG3_DATA_32BIT_CHID     0x20U   /* 32-bit with channel ID    */
#define CONFIG3_DATA_32BIT_SIGN     0x30U   /* 32-bit sign-extended      */
#define CONFIG3_EN_CRCCOM           0x04U   /* CRC on SPI comms          */
#define CONFIG3_EN_OFFCAL           0x02U   /* Apply OFFSETCAL register  */
#define CONFIG3_EN_GAINCAL          0x01U   /* Apply GAINCAL register    */

/* -------------------------------------------------------------------------
 * IRQ register [7:0]
 * ------------------------------------------------------------------------- */
#define IRQ_DR_STATUS               0x40U   /* 0 = new data ready (active-low) */
#define IRQ_CRCCFG_STATUS           0x20U
#define IRQ_POR_STATUS              0x10U

/* -------------------------------------------------------------------------
 * MUX register: upper nibble = VIN+, lower nibble = VIN-
 * ------------------------------------------------------------------------- */
/* VIN+ channel select (bits [7:4]) */
#define MUX_VIP_CH0                 0x00U
#define MUX_VIP_CH1                 0x10U
#define MUX_VIP_CH2                 0x20U
#define MUX_VIP_CH3                 0x30U
#define MUX_VIP_CH4                 0x40U
#define MUX_VIP_CH5                 0x50U
#define MUX_VIP_CH6                 0x60U
#define MUX_VIP_CH7                 0x70U
#define MUX_VIP_AGND                0x80U   /* Analog ground              */
#define MUX_VIP_AVDD                0x90U   /* Analog supply              */
#define MUX_VIP_REFP_HALF           0xB0U   /* REFIN+ / 2                 */
#define MUX_VIP_REFM_HALF           0xC0U   /* REFIN- / 2                 */
#define MUX_VIP_VCM                 0xF0U   /* Common-mode = AVDD/2       */

/* VIN- channel select (bits [3:0]) */
#define MUX_VIN_CH0                 0x00U
#define MUX_VIN_CH1                 0x01U
#define MUX_VIN_CH2                 0x02U
#define MUX_VIN_CH3                 0x03U
#define MUX_VIN_CH4                 0x04U
#define MUX_VIN_CH5                 0x05U
#define MUX_VIN_CH6                 0x06U
#define MUX_VIN_CH7                 0x07U
#define MUX_VIN_AGND                0x08U
#define MUX_VIN_AVDD                0x09U
#define MUX_VIN_REFP_HALF           0x0BU
#define MUX_VIN_REFM_HALF           0x0CU
#define MUX_VIN_VCM                 0x0FU

/* Calibration MUX presets */
#define MUX_OFFSET_CAL  (MUX_VIP_AGND     | MUX_VIN_AGND)      /* 0x88: ΔV = 0        */
#define MUX_GAIN_CAL    (MUX_VIP_REFP_HALF | MUX_VIN_REFM_HALF) /* 0xBC: ΔV = VREF/2   */

/* -------------------------------------------------------------------------
 * Calibration constants
 *
 * GAINCAL unity = 2^23 = 0x800000  (1.0 in Q1.23 fixed-point)
 * For gain cal: VIN_diff = VREF/2 → ideal code = 2^22 = 0x400000
 *   GAINCAL = ideal_code × GAINCAL_UNITY / measured_code
 * ------------------------------------------------------------------------- */
#define MCP3465R_GAINCAL_UNITY      0x800000UL
#define MCP3465R_GAIN_CAL_EXPECTED  0x400000L   /* Code for VREF/2 input (2^22) */
#define MCP3465R_ADC_FULL_SCALE     0x7FFFFFL   /* +FS code in 24-bit mode      */

/* Averaging parameters for calibration */
#define MCP3465R_CAL_DISCARD        8U          /* Samples discarded for settling */
#define MCP3465R_CAL_SAMPLES        64U         /* Samples averaged per phase     */

/* Sanity window: measured gain-cal code must be within ±15 % of expected */
#define MCP3465R_CAL_GAIN_TOL_PCT   15

/* -------------------------------------------------------------------------
 * Types
 * ------------------------------------------------------------------------- */
typedef enum {
    MCP3465R_OK             = 0,
    MCP3465R_ERR_SPI        = -1,
    MCP3465R_ERR_TIMEOUT    = -2,
    MCP3465R_ERR_CAL        = -3,   /* Calibration sanity check failed */
} MCP3465R_Status_t;

/**
 * Hardware binding.  Populate before calling MCP3465R_Init().
 * The CS pin must already be configured as push-pull output by CubeMX / init code.
 */
typedef struct {
    SPI_HandleTypeDef *hspi;        /* HAL SPI handle                     */
    GPIO_TypeDef      *cs_port;     /* CS GPIO port                       */
    uint16_t           cs_pin;      /* CS GPIO pin mask                   */
    uint32_t           timeout_ms;  /* Per-byte SPI timeout               */
} MCP3465R_Handle_t;

/**
 * Results populated by MCP3465R_AutoCalibrate().
 * Useful for logging or non-volatile storage (re-apply without re-running cal).
 */
typedef struct {
    int32_t  offset_raw;        /* Raw ADC reading at zero input (pre-correction) */
    int32_t  gain_raw;          /* Raw ADC reading at VREF/2 (post-offset-correction) */
    int32_t  offsetcal_val;     /* Value written to OFFSETCAL register */
    uint32_t gaincal_val;       /* Value written to GAINCAL  register */
} MCP3465R_CalResult_t;

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * MCP3465R_Init – configure ADC registers and reset calibration to defaults.
 *
 * Default config after Init:
 *   VREF     = internal 2.4 V
 *   Clock    = internal oscillator
 *   OSR      = 4096  (~10 ms conversion time)
 *   PGA gain = 1
 *   Mode     = continuous conversion
 *   Cal regs = disabled (OFFSETCAL=0, GAINCAL=0x800000)
 */
MCP3465R_Status_t MCP3465R_Init(MCP3465R_Handle_t *dev);

/** Write len bytes to a register starting at reg (incremental write). */
MCP3465R_Status_t MCP3465R_WriteReg(MCP3465R_Handle_t *dev, uint8_t reg,
                                     const uint8_t *data, uint8_t len);

/** Read len bytes from a register starting at reg (incremental read). */
MCP3465R_Status_t MCP3465R_ReadReg(MCP3465R_Handle_t *dev, uint8_t reg,
                                    uint8_t *data, uint8_t len);

/** Block until ADCDATA is fresh (polls IRQ register) or timeout_ms elapses. */
MCP3465R_Status_t MCP3465R_WaitDataReady(MCP3465R_Handle_t *dev, uint32_t timeout_ms);

/**
 * MCP3465R_ReadADC – wait for data-ready then return the 24-bit signed result.
 *
 * If EN_OFFCAL / EN_GAINCAL are set in CONFIG3 the hardware has already
 * applied those corrections; the returned value is the final corrected code.
 */
MCP3465R_Status_t MCP3465R_ReadADC(MCP3465R_Handle_t *dev, int32_t *value);

/** Select differential input pair via the MUX register. */
MCP3465R_Status_t MCP3465R_SetMux(MCP3465R_Handle_t *dev, uint8_t mux);

/** Write a 24-bit two's-complement value to OFFSETCAL. */
MCP3465R_Status_t MCP3465R_SetOffsetCal(MCP3465R_Handle_t *dev, int32_t offset);

/** Write a 24-bit value to GAINCAL (0x800000 = unity). */
MCP3465R_Status_t MCP3465R_SetGainCal(MCP3465R_Handle_t *dev, uint32_t gain);

/** Enable or disable the hardware offset / gain calibration registers. */
MCP3465R_Status_t MCP3465R_EnableCalRegisters(MCP3465R_Handle_t *dev,
                                               bool en_offset, bool en_gain);

/**
 * MCP3465R_AutoCalibrate – run two-phase automatic calibration.
 *
 * Phase 1 – Offset:
 *   Switches MUX to AGND/AGND, averages MCP3465R_CAL_SAMPLES readings,
 *   writes -mean to OFFSETCAL, enables EN_OFFCAL.
 *
 * Phase 2 – Gain:
 *   Switches MUX to REFIN+/2 vs REFIN-/2 (expected code = 2^22 = 0x400000),
 *   averages readings (OFFSETCAL already applied in hardware),
 *   computes GAINCAL = 0x400000 × 0x800000 / mean, enables EN_GAINCAL.
 *
 * On return the MUX is restored to CH0/CH1 (0x01) and both calibration
 * registers are active.
 *
 * @param result  Optional; pass NULL to discard diagnostics.
 * @return MCP3465R_OK on success, MCP3465R_ERR_CAL if the measured gain-cal
 *         code is outside the ±15 % sanity window (check wiring / VREF).
 */
MCP3465R_Status_t MCP3465R_AutoCalibrate(MCP3465R_Handle_t *dev,
                                          MCP3465R_CalResult_t *result);

/**
 * MCP3465R_ApplyStoredCal – restore calibration from previously saved result
 * without re-running the measurement phases (useful after MCU reset).
 */
MCP3465R_Status_t MCP3465R_ApplyStoredCal(MCP3465R_Handle_t *dev,
                                            const MCP3465R_CalResult_t *result);

#endif /* MCP3465R_H */
