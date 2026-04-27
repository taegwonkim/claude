/**
 * MCP3465R Auto-calibration Driver
 * Target: STM32L552 + FreeRTOS
 * Input range : 0~5V (3/5 voltage divider → ADC sees 0~3V)
 * VREF        : External 3.0V on REFIN+/REFIN-
 *
 * Voltage divider: R1=2kΩ (top), R2=3kΩ (bottom)
 *   V_adc = V_in × R2/(R1+R2) = V_in × 0.6
 *
 * Calibration procedure (fully automatic, no external signal needed):
 *   1. Offset : MUX → AGND/AGND, average 64 samples, write −avg to OFFSETCAL
 *   2. Gain   : MUX → REFIN+/REFIN-, average 64 samples,
 *               GAINCAL = (0x7FFFFF × 0x800000) / avg
 *   Both corrections are applied in hardware via EN_OFFCAL / EN_GAINCAL bits.
 */

#ifndef MCP3465R_H
#define MCP3465R_H

#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>

/* ---------- Hardware binding -------------------------------------------- */
#define MCP3465R_SPI            hspi1       /* extern SPI_HandleTypeDef       */
#define MCP3465R_CS_GPIO        GPIOA
#define MCP3465R_CS_PIN         GPIO_PIN_4
#define MCP3465R_SPI_TIMEOUT_MS 10u

/* ---------- Device address (HW pins A1:A0, default = 0b01) --------------- */
#define MCP3465R_DEV_ADDR       0x01u

/* ---------- Register addresses ------------------------------------------ */
#define REG_ADCDATA             0x00u
#define REG_CONFIG0             0x01u
#define REG_CONFIG1             0x02u
#define REG_CONFIG2             0x03u
#define REG_CONFIG3             0x04u
#define REG_IRQ                 0x05u
#define REG_MUX                 0x06u
#define REG_SCAN                0x07u
#define REG_TIMER               0x08u
#define REG_OFFSETCAL           0x09u
#define REG_GAINCAL             0x0Au
#define REG_LOCK                0x0Du

/* ---------- SPI command types ------------------------------------------- */
#define CMD_FAST_CMD            0x00u
#define CMD_STATIC_READ         0x01u
#define CMD_INC_WRITE           0x02u
#define CMD_INC_READ            0x03u

/* Fast commands */
#define FAST_START_CONV         0x28u   /* (DEV_ADDR<<6)|0x28 */
#define FAST_STANDBY            0x2Cu
#define FAST_SHUTDOWN           0x30u
#define FAST_FULL_RESET         0x38u

/* ---------- CONFIG0 bit fields ------------------------------------------ */
/* [7]   VREF_SEL : 0 = external VREF, 1 = internal 2.4V                   */
/* [5:4] CLK_SEL  : 10 = internal oscillator (~4.9 MHz AMCLK)               */
/* [3:2] CS_SEL   : 00 = no bias current                                     */
/* [1:0] ADC_MODE : 10 = standby, 11 = conversion                           */
#define CONFIG0_EXT_VREF_STBY   0x22u   /* external VREF, int osc, standby  */
#define CONFIG0_EXT_VREF_CONV   0x23u   /* external VREF, int osc, convert  */

/* ---------- CONFIG1 --------------------------------------------------------
 * [5:2] OSR[3:0]: oversampling ratio
 *   0x00=32, 0x01=64, 0x02=128, 0x03=256,
 *   0x04=512, 0x05=1024, 0x06=2048, 0x07=4096,
 *   0x08=8192, 0x09=16384, 0x0A=20480, 0x0B=24576,
 *   0x0C=40960, 0x0D=49152, 0x0E=81920, 0x0F=98304
 * Use OSR=4096 during calibration for maximum noise immunity
 * Use OSR=256  during normal operation for ~1 kHz throughput
 */
#define CONFIG1_OSR_256         0x0Cu   /* OSR=256,  ~1000 SPS  */
#define CONFIG1_OSR_4096        0x1Cu   /* OSR=4096, ~62.5 SPS  */

/* ---------- CONFIG2 --------------------------------------------------------
 * [7:6] BOOST[1:0] : 11 = 2x (full bias current, best THD+noise)
 * [5:3] GAIN[2:0]  : 010 = 1x PGA gain
 * [2]   AZ_MUX     : 1 = auto-zero MUX enabled
 * [1]   AZ_REF     : 0
 * [0]   AZ_VREF    : 0
 */
#define CONFIG2_BOOST2_GAIN1_AZ 0xD4u  /* 11 010 1 00 */

/* ---------- CONFIG3 --------------------------------------------------------
 * [7:6] CONV_MODE   : 11 = continuous conversion
 * [5:4] DATA_FORMAT : 00 = 24-bit (MSB-justified, no channel ID)
 * [3]   CRC_FORMAT  : 0
 * [2]   EN_CRCCOM   : 0
 * [1]   EN_OFFCAL   : 1 = apply OFFSETCAL in hardware
 * [0]   EN_GAINCAL  : 1 = apply GAINCAL in hardware
 */
#define CONFIG3_CAL_NONE        0xC0u   /* no offset/gain correction applied */
#define CONFIG3_CAL_OFFSET      0xC2u   /* offset correction only            */
#define CONFIG3_CAL_BOTH        0xC3u   /* offset + gain correction          */

/* ---------- MUX channel IDs --------------------------------------------- */
#define MUX_CH0                 0x0u
#define MUX_CH1                 0x1u
#define MUX_CH2                 0x2u
#define MUX_CH3                 0x3u
#define MUX_AGND                0x8u    /* internal AGND                     */
#define MUX_AVDD                0x9u    /* AVDD                               */
#define MUX_REFIN_POS           0xBu    /* REFIN+ pin                        */
#define MUX_REFIN_NEG           0xCu    /* REFIN− pin                        */
#define MUX_TEMP_DIODE_P        0xDu    /* internal temp diode +             */
#define MUX_TEMP_DIODE_M        0xEu    /* internal temp diode −             */

#define MUX_REG(pos, neg)       (((pos) << 4) | (neg))

/* ---------- Calibration constants --------------------------------------- */
#define CAL_SAMPLES             64u
#define CAL_DISCARD_SAMPLES     8u      /* thrown away before averaging      */
/* Per-sample delay at OSR=4096, int-osc (~4.9 MHz):
 *   Conversion time ≈ OSR × (1/f_MOD)
 *   f_MOD = AMCLK / DMCLK_ratio. At int osc ≈ 4.9 MHz / 4 ≈ 1.23 MHz
 *   t_conv = 4096 / 1.23e6 ≈ 3.3 ms → 5 ms margin                        */
#define CAL_SAMPLE_DELAY_MS     5u
#define CAL_SETTLE_DELAY_MS     100u    /* settling after MUX switch         */

/* ---------- Measurement constants --------------------------------------- */
#define ADC_FULL_SCALE          8388607L    /* 2^23 - 1 = 0x7FFFFF          */
#define GAINCAL_UNITY           0x800000u   /* 1.0 in Q0.23 fixed-point      */
#define VREF_MV                 3000.0f     /* external reference, mV        */
#define VIN_DIVIDER_RATIO       0.6f        /* R2/(R1+R2) = 3/5              */
#define VIN_MAX_MV              5000.0f

/* ---------- Return / status --------------------------------------------- */
typedef enum {
    MCP_OK              = 0,
    MCP_ERR_SPI         = 1,
    MCP_ERR_INVALID     = 2,
    MCP_ERR_CAL_FAILED  = 3,
} MCP3465R_Status_t;

/* ---------- Calibration result ------------------------------------------ */
typedef struct {
    int32_t  offset_raw;        /* raw code measured at AGND/AGND input     */
    int32_t  offset_cal_reg;    /* value written to OFFSETCAL               */
    uint32_t gain_cal_reg;      /* value written to GAINCAL                 */
    float    gain_factor;       /* gain_cal_reg / GAINCAL_UNITY             */
    bool     valid;             /* true after successful calibration         */
} MCP3465R_CalResult_t;

/* ---------- Public API -------------------------------------------------- */
extern SPI_HandleTypeDef MCP3465R_SPI;

MCP3465R_Status_t MCP3465R_Init(void);
MCP3465R_Status_t MCP3465R_Calibrate(MCP3465R_CalResult_t *result);
MCP3465R_Status_t MCP3465R_ReadRaw(int32_t *raw_code);
MCP3465R_Status_t MCP3465R_ReadVoltage(float *voltage_mv);

/* FreeRTOS tasks — create via osThreadNew() */
void Task_MCP3465R_Init(void *arg);   /* one-shot: init + calibrate, then terminates */
void Task_MCP3465R_Measure(void *arg);/* periodic measurement loop                   */

/* Shared measurement result (written by Task_MCP3465R_Measure) */
extern volatile float     g_voltage_mv;
extern volatile int32_t   g_adc_raw;
extern volatile bool      g_cal_valid;
extern osMutexId_t        g_mcp_mutex;

#endif /* MCP3465R_H */
