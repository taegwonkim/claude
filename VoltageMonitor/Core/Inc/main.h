/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file
  *                   Voltage Feedback Current Monitor System
  *                   STM32L552RET6 + FreeRTOS
  ******************************************************************************
  */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"

/* Exported defines ----------------------------------------------------------*/

/* Number of channels */
#define NUM_CHANNELS            4

/* SPI1 CS Pins - ADC MCP3465R */
#define ADC_CS1_GPIO_Port       GPIOC
#define ADC_CS1_Pin             GPIO_PIN_0
#define ADC_CS2_GPIO_Port       GPIOC
#define ADC_CS2_Pin             GPIO_PIN_1
#define ADC_CS3_GPIO_Port       GPIOC
#define ADC_CS3_Pin             GPIO_PIN_2
#define ADC_CS4_GPIO_Port       GPIOC
#define ADC_CS4_Pin             GPIO_PIN_3

/* SPI2 CS Pins - DAC AD5641 */
#define DAC_CS1_GPIO_Port       GPIOC
#define DAC_CS1_Pin             GPIO_PIN_4
#define DAC_CS2_GPIO_Port       GPIOC
#define DAC_CS2_Pin             GPIO_PIN_5
#define DAC_CS3_GPIO_Port       GPIOC
#define DAC_CS3_Pin             GPIO_PIN_6
#define DAC_CS4_GPIO_Port       GPIOC
#define DAC_CS4_Pin             GPIO_PIN_7

/* LED Pins */
#define LED_STATUS_GPIO_Port    GPIOB
#define LED_STATUS_Pin          GPIO_PIN_4
#define LED_ERROR_GPIO_Port     GPIOB
#define LED_ERROR_Pin           GPIO_PIN_5

/* Voltage range */
#define VOLTAGE_MIN             0.0f
#define VOLTAGE_MAX             5.0f
#define VREF_DAC                5.0f    /* AD5641 reference voltage */
#define VREF_ADC                5.0f    /* MCP3465R reference voltage */

/* AD5641 resolution: 14-bit */
#define DAC_RESOLUTION          16384   /* 2^14 */

/* MCP3465R resolution: 16-bit */
#define ADC_RESOLUTION          65536   /* 2^16 */

/* Current thresholds (Amps) - based on shunt resistor value */
#define SHUNT_RESISTOR_OHM      0.1f   /* 0.1 ohm shunt resistor */
#define CURRENT_SHORT_THRESHOLD  0.5f   /* > 500mA = short circuit */
#define CURRENT_OPEN_THRESHOLD   0.001f /* < 1mA = open circuit (when V > 0.5V) */
#define VOLTAGE_OPEN_CHECK_MIN   0.5f   /* Min voltage to check open circuit */

/* Report interval (ms) */
#define REPORT_INTERVAL_MS      500

/* Feedback control interval (ms) */
#define FEEDBACK_INTERVAL_MS    10      /* 100 Hz control loop */

/* Current monitor interval (ms) */
#define CURRENT_MON_INTERVAL_MS 50      /* 20 Hz monitoring */

/* Exported types ------------------------------------------------------------*/

/* Channel status */
typedef enum {
    CH_STATUS_OK        = 0x00,
    CH_STATUS_SHORT     = 0x01,
    CH_STATUS_OPEN      = 0x02,
    CH_STATUS_OVERRANGE = 0x03,
    CH_STATUS_ERROR     = 0xFF
} ChannelStatus_t;

/* Voltage command from UART */
typedef struct {
    uint8_t  channel;       /* 0~3 */
    float    voltage;       /* 0.0 ~ 5.0 V */
} VoltageCmd_t;

/* Per-channel data */
typedef struct {
    float           target_voltage;     /* 설정 목표 전압 (V) */
    float           actual_voltage;     /* ADC 측정 전압 (V) */
    float           current_ma;         /* 측정 전류 (mA) */
    uint16_t        dac_code;           /* 현재 DAC 출력 코드 */
    ChannelStatus_t status;             /* 채널 상태 */
} ChannelData_t;

/* Report data packet */
typedef struct {
    ChannelData_t   channels[NUM_CHANNELS];
    uint32_t        timestamp_ms;
} ReportData_t;

/* Debug message */
typedef struct {
    char msg[64];
} DebugMsg_t;

/* Exported variables --------------------------------------------------------*/
extern SPI_HandleTypeDef  hspi1;
extern SPI_HandleTypeDef  hspi2;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart4;
extern FDCAN_HandleTypeDef hfdcan1;

/* Exported functions --------------------------------------------------------*/
void Error_Handler(void);
void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
