/**
 ******************************************************************************
 * @file    app_config.h
 * @brief   Application-wide configuration and constants
 ******************************************************************************
 */
#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── System ────────────────────────────────────────────────────────────── */
#define NUM_CHANNELS                4U
#define VREF_MV                     3300U     /* MCU VDD / reference voltage (mV) */
#define DAC_RESOLUTION              4096U     /* 12-bit DAC */
#define ADC_RESOLUTION              4096U     /* 12-bit internal ADC */
#define EXT_ADC_RESOLUTION          65536U    /* MCP3465R 16-bit */

/* ── Voltage Output Range ─────────────────────────────────────────────── */
#define VOLTAGE_MIN_MV              0U
#define VOLTAGE_MAX_MV              3300U     /* 0 ~ 3.3V range */

/* ── PI Controller ────────────────────────────────────────────────────── */
#define PI_KP                       0.5f      /* Proportional gain */
#define PI_KI                       0.1f      /* Integral gain */
#define PI_OUTPUT_MIN               0.0f
#define PI_OUTPUT_MAX               4095.0f
#define PI_INTEGRAL_LIMIT           2000.0f   /* Anti-windup clamp */
#define CTRL_LOOP_PERIOD_MS         10U       /* 100 Hz control loop */
#define VOLTAGE_TOLERANCE_MV        10U       /* ±10mV tolerance band */

/* ── Fault Detection Thresholds ───────────────────────────────────────── */
#define CURRENT_SHORT_THRESHOLD_MA  500U      /* > 500mA → short circuit */
#define CURRENT_OPEN_THRESHOLD_MA   1U        /* < 1mA when output > 100mV → open */
#define OPEN_DETECT_VOLTAGE_MIN_MV  100U      /* Minimum output to check open */
#define FAULT_DEBOUNCE_COUNT        5U        /* Consecutive faults before alarm */
#define SHUNT_RESISTOR_MOHM         100U      /* 0.1 Ω = 100 mΩ */
#define CURRENT_AMP_GAIN            50U       /* INA180 gain */

/* ── UART Protocol ────────────────────────────────────────────────────── */
#define UART_RX_BUF_SIZE            128U
#define UART_TX_BUF_SIZE            256U
#define UART_CMD_MAX_LEN            64U

/* ── FDCAN ────────────────────────────────────────────────────────────── */
#define FDCAN_TX_ID_VOLTAGE         0x100U    /* Voltage measurement CAN ID */
#define FDCAN_TX_ID_FAULT           0x200U    /* Fault status CAN ID */
#define FDCAN_TX_ID_STATUS          0x300U    /* System status CAN ID */

/* ── Task Periods ─────────────────────────────────────────────────────── */
#define ADC_READ_PERIOD_MS          10U       /* 100 Hz ADC sampling */
#define FAULT_CHECK_PERIOD_MS       50U       /* 20 Hz fault check */
#define COMM_TX_PERIOD_MS           100U      /* 10 Hz data reporting */
#define HEARTBEAT_PERIOD_MS         1000U

/* ── Queue Sizes ──────────────────────────────────────────────────────── */
#define UART_CMD_QUEUE_SIZE         10U
#define ADC_DATA_QUEUE_SIZE         10U
#define COMM_TX_QUEUE_SIZE          20U
#define FAULT_EVENT_QUEUE_SIZE      10U

#ifdef __cplusplus
}
#endif

#endif /* __APP_CONFIG_H */
