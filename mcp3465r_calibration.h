/**
 * MCP3465R Automatic Calibration for STM32L552 + FreeRTOS
 *
 * Hardware assumptions:
 *   - VREF_EXT = 3.000 V (connected to REFIN+ pin)
 *   - VIN range = 0–5 V via external 2kΩ/3kΩ divider → 0–3.0 V at AINP
 *   - ADDR pins → device address = 0x01
 *   - PGA = 1× (gain configured in CONFIG2)
 *
 * Calibration sequence (both steps are fully automatic / internal):
 *   Step 1 – Offset  : reroute ADC MUX to AGND/AGND, average N samples,
 *                      write −mean into OFFSETCAL register.
 *   Step 2 – Gain    : reroute ADC MUX to REFIN+/AGND, compare measured
 *                      code to expected full-scale, write correction into
 *                      GAINCAL register.
 *
 * Calibration registers (always active, no enable bit needed):
 *   OFFSETCAL (0x09) : signed 24-bit, default 0x000000
 *   GAINCAL   (0x0A) : unsigned 24-bit Q1.23, default 0x800000 (= 1.0)
 *
 *   Corrected output = (raw + OFFSETCAL) × GAINCAL / 2^23
 */

#ifndef MCP3465R_CALIBRATION_H
#define MCP3465R_CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l5xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Device / register constants (Microchip MCP346x naming convention) */
/* ------------------------------------------------------------------ */

#define MCP3465R_DEVICE_ADDR        0x01u   /* set by ADDR[1:0] pins */

/* Register addresses */
#define MCP3465R_REG_ADCDATA        0x00u
#define MCP3465R_REG_CONFIG0        0x01u
#define MCP3465R_REG_CONFIG1        0x02u
#define MCP3465R_REG_CONFIG2        0x03u
#define MCP3465R_REG_CONFIG3        0x04u
#define MCP3465R_REG_IRQ            0x05u
#define MCP3465R_REG_MUX            0x06u
#define MCP3465R_REG_SCAN           0x07u
#define MCP3465R_REG_TIMER          0x08u
#define MCP3465R_REG_OFFSETCAL      0x09u
#define MCP3465R_REG_GAINCAL        0x0Au
#define MCP3465R_REG_LOCK           0x0Du
#define MCP3465R_REG_CRCCFG         0x0Fu

/* SPI command types */
#define MCP3465R_CMD_FAST           0x00u
#define MCP3465R_CMD_STATIC_READ    0x01u
#define MCP3465R_CMD_INCR_WRITE     0x02u
#define MCP3465R_CMD_INCR_READ      0x03u

/* Build first-byte command: [DEV_ADDR:2][REG_ADDR:4][CMD_TYPE:2] */
#define MCP3465R_CMD_BYTE(dev, reg, cmd) \
    (uint8_t)(((dev) << 6) | ((reg) << 2) | (cmd))

/* Fast commands */
#define MCP3465R_FAST_START         0xA8u   /* start/restart conversion */
#define MCP3465R_FAST_STANDBY       0xACu
#define MCP3465R_FAST_SHUTDOWN      0xB0u
#define MCP3465R_FAST_RESET         0xB8u

/* ------------------------------------------------------------------
 * CONFIG0 (0x01)
 * [7:6] VREF_SEL  00=internal, 01=external
 * [5:4] CS_SEL    bias current
 * [3:2] CLK_SEL   00=internal, 10=external
 * [1:0] ADC_MODE  00=shutdown, 10=standby, 11=conversion
 * ------------------------------------------------------------------ */
#define MCP3465R_CONFIG0_VREF_EXT   (1u << 6)   /* use external VREF */
#define MCP3465R_CONFIG0_CS_0UA     (0u << 4)
#define MCP3465R_CONFIG0_CLK_INT    (0u << 2)
#define MCP3465R_CONFIG0_ADC_CONV   (3u << 0)
#define MCP3465R_CONFIG0_ADC_STBY   (2u << 0)
#define MCP3465R_CONFIG0_ADC_SHTDWN (0u << 0)

/* ------------------------------------------------------------------
 * CONFIG1 (0x02)
 * [7:6] PRE        AMCLK prescaler
 * [5:2] OSR        oversampling ratio
 * ------------------------------------------------------------------ */
#define MCP3465R_CONFIG1_PRE_1      (0u << 6)
#define MCP3465R_CONFIG1_OSR_256    (5u << 2)   /* OSR = 256 → ~3.9 kSPS */
#define MCP3465R_CONFIG1_OSR_1024   (7u << 2)
#define MCP3465R_CONFIG1_OSR_4096   (9u << 2)   /* OSR = 4096 for cal accuracy */

/* ------------------------------------------------------------------
 * CONFIG2 (0x03)
 * [7:6] BOOST      current boost
 * [5:3] GAIN       000=1/3  001=1  010=2  …
 * [2]   AZ_MUX     auto-zero MUX on each conversion
 * [1]   AZ_REF     auto-zero reference buffer
 * [0]   AZ_VREF    auto-zero VREF
 * ------------------------------------------------------------------ */
#define MCP3465R_CONFIG2_BOOST_1    (2u << 6)
#define MCP3465R_CONFIG2_GAIN_1_3   (0u << 3)
#define MCP3465R_CONFIG2_GAIN_1     (1u << 3)
#define MCP3465R_CONFIG2_GAIN_2     (2u << 3)
#define MCP3465R_CONFIG2_AZ_MUX_EN  (1u << 2)

/* ------------------------------------------------------------------
 * CONFIG3 (0x04)
 * [7:6] CONV_MODE  00=one-shot→shutdown  11=continuous
 * [5:4] DATA_FORMAT 00=24-bit  10=32-bit sign-extended
 * [1]   EN_CRCCOM
 * [0]   EN_GAINCAL (enable GAINCAL register)
 * ------------------------------------------------------------------ */
#define MCP3465R_CONFIG3_CONV_CONT  (3u << 6)
#define MCP3465R_CONFIG3_CONV_1SHOT (0u << 6)
#define MCP3465R_CONFIG3_FMT_32EXT  (2u << 4)
#define MCP3465R_CONFIG3_FMT_24     (0u << 4)
#define MCP3465R_CONFIG3_EN_GAINCAL (1u << 0)

/* ------------------------------------------------------------------
 * MUX register (0x06)  [7:4]=VIN+  [3:0]=VIN−
 * ------------------------------------------------------------------ */
#define MCP3465R_MUX_CH0            0x0u
#define MCP3465R_MUX_CH1            0x1u
#define MCP3465R_MUX_AGND           0x8u
#define MCP3465R_MUX_AVDD           0x9u
#define MCP3465R_MUX_REFIN_POS      0xAu
#define MCP3465R_MUX_REFIN_NEG      0xBu
#define MCP3465R_MUX_TEMP_POS       0xDu
#define MCP3465R_MUX_TEMP_NEG       0xEu
#define MCP3465R_MUX_AVSS           0xFu

#define MCP3465R_MUX_REG(pos, neg)  (uint8_t)(((pos) << 4) | (neg))

/* Internal node connections used for calibration */
#define MCP3465R_MUX_OFFSET_CAL     MCP3465R_MUX_REG(MCP3465R_MUX_AGND,      MCP3465R_MUX_AGND)
#define MCP3465R_MUX_GAIN_CAL       MCP3465R_MUX_REG(MCP3465R_MUX_REFIN_POS, MCP3465R_MUX_AGND)
#define MCP3465R_MUX_NORMAL         MCP3465R_MUX_REG(MCP3465R_MUX_CH0,       MCP3465R_MUX_AGND)

/* ------------------------------------------------------------------
 * Calibration parameters
 * ------------------------------------------------------------------ */
#define MCP3465R_CAL_SAMPLES        64u     /* averaged for each cal step */
#define MCP3465R_ADC_FULLSCALE      ((int32_t)0x7FFFFF)   /* +full scale 2^23−1 */
#define MCP3465R_GAINCAL_UNITY      ((int32_t)0x800000)   /* Q1.23 = 1.000000 */

/* VREF = 3.000 V.  Gain-cal measures REFIN+ vs AGND (= VREF/VREF × 2^23).
 * The gain-cal MUX connects REFIN+ → VIN+ and AGND → VIN−, so the ideal
 * code is +full-scale = 0x7FFFFF.  Any deviation is pure gain error.       */
#define MCP3465R_GAINCAL_EXPECTED   MCP3465R_ADC_FULLSCALE

/* SPI timeout (ms) */
#define MCP3465R_SPI_TIMEOUT_MS     10u

/* ------------------------------------------------------------------ */
/*  Public types                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    MCP3465R_CAL_OK          = 0,
    MCP3465R_CAL_SPI_ERR     = -1,
    MCP3465R_CAL_TIMEOUT     = -2,
    MCP3465R_CAL_NO_DATA     = -3,
    MCP3465R_CAL_SATURATION  = -4,
    MCP3465R_CAL_DIVZERO     = -5,
} MCP3465R_CalStatus_t;

/** SPI + GPIO handle passed to every driver call */
typedef struct {
    SPI_HandleTypeDef  *hspi;
    GPIO_TypeDef       *cs_port;
    uint16_t            cs_pin;
    SemaphoreHandle_t   spi_mutex;    /* FreeRTOS mutex for bus sharing */
    uint8_t             dev_addr;     /* 0x01 by default */
} MCP3465R_Handle_t;

/** Results written to OFFSETCAL / GAINCAL registers */
typedef struct {
    int32_t  offset_raw;     /* raw offset measured (before writing) */
    int32_t  offset_reg;     /* value written to OFFSETCAL */
    int32_t  gain_raw;       /* raw gain reference measured */
    int32_t  gain_reg;       /* value written to GAINCAL */
    bool     offset_done;
    bool     gain_done;
} MCP3465R_CalResult_t;

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

/**
 * Initialise the MCP3465R handle.
 * Call once before any other function.
 */
MCP3465R_CalStatus_t MCP3465R_Init(MCP3465R_Handle_t *h);

/**
 * Full automatic calibration: offset then gain.
 * Both steps use internal MUX routing — no external wiring change needed.
 * Thread-safe (acquires h->spi_mutex).
 */
MCP3465R_CalStatus_t MCP3465R_AutoCalibrate(MCP3465R_Handle_t   *h,
                                             MCP3465R_CalResult_t *result);

/**
 * Offset calibration only.
 * Routes MUX to AGND/AGND, averages CAL_SAMPLES readings, writes −mean
 * into OFFSETCAL register.
 */
MCP3465R_CalStatus_t MCP3465R_CalibrateOffset(MCP3465R_Handle_t   *h,
                                               MCP3465R_CalResult_t *result);

/**
 * Gain calibration only.
 * Routes MUX to REFIN+/AGND, averages CAL_SAMPLES readings, computes
 * GAINCAL correction and writes it.  Run AFTER offset calibration.
 */
MCP3465R_CalStatus_t MCP3465R_CalibrateGain(MCP3465R_Handle_t   *h,
                                             MCP3465R_CalResult_t *result);

/**
 * Read a calibrated ADC sample on the configured channel.
 * Returns signed 24-bit value (corrected by hardware OFFSETCAL/GAINCAL).
 *
 * Convert to volts:  V_divider = (code / 2^23) × VREF
 *                    V_input   = V_divider × (5/3)   [divider ratio]
 */
MCP3465R_CalStatus_t MCP3465R_ReadCalibratedSample(MCP3465R_Handle_t *h,
                                                    int32_t           *out_code);

/**
 * Helper: convert ADC code → real input voltage (0–5 V scale).
 * divider_ratio = R2/(R1+R2) = 3/5 = 0.6 for a 2kΩ/3kΩ divider.
 */
static inline float MCP3465R_CodeToVoltage(int32_t  code,
                                            float    vref,
                                            float    divider_ratio)
{
    float v_at_adc = ((float)code / (float)MCP3465R_GAINCAL_UNITY) * vref;
    return v_at_adc / divider_ratio;
}

/* Low-level register access (public for diagnostics) */
MCP3465R_CalStatus_t MCP3465R_WriteReg(MCP3465R_Handle_t *h,
                                        uint8_t reg, uint32_t val, uint8_t nbytes);
MCP3465R_CalStatus_t MCP3465R_ReadReg (MCP3465R_Handle_t *h,
                                        uint8_t reg, uint32_t *val, uint8_t nbytes);

#ifdef __cplusplus
}
#endif
#endif /* MCP3465R_CALIBRATION_H */
