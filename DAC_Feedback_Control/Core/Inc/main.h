/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    main.h
 * @brief   4ch DAC 피드백 제어 시스템 – 공통 정의
 *
 * 하드웨어:
 *   MCU  : STM32L552RE
 *   DAC  : AD5641 × 4  (SPI1, 각 채널별 CS)
 *   ADC  : MCP3465R × 1 (SPI2, 8ch: CH0-3 전압, CH4-7 전류)
 *   UART : USART1 (PC 명령/데이터)
 *   CAN  : FDCAN1 (주기 데이터 전송)
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"
#include "cmsis_os.h"

/* =========================================================================
 * 시스템 파라미터
 * ========================================================================= */
#define NUM_CHANNELS            4
#define VREF_MV                 3300U       /* DAC/ADC 기준전압 3.3V */
#define VREF_UV                 3300000UL

/* DAC AD5641 (14비트) */
#define DAC_MAX_CODE            0x3FFFU     /* 2^14 - 1 = 16383 */
#define DAC_RESOLUTION_BITS     14U

/* ADC MCP3465R (16비트 출력 설정) */
#define ADC_MAX_CODE            0x7FFFU     /* signed 16bit 양수 최대값 */
#define ADC_RESOLUTION_BITS     16U

/* 전류 측정 션트 저항 (Ω × 1000 = mΩ) */
#define SHUNT_RESISTOR_MOHM     100U        /* 0.1Ω = 100mΩ */

/* 단락/단선 판정 기준 */
#define SHORT_CURRENT_THRESHOLD_MA  500U    /* 500mA 초과 시 단락 */
#define OPEN_CURRENT_THRESHOLD_MA   1U      /* 1mA 미만 시 단선 (출력 설정 시) */
#define MIN_VOLTAGE_FOR_OPEN_MV     100U    /* 이 전압 이상 설정 시 단선 판정 */

/* =========================================================================
 * GPIO 핀 정의
 * ========================================================================= */
/* DAC CS 핀 */
#define DAC_CS0_PIN             GPIO_PIN_0
#define DAC_CS0_PORT            GPIOC
#define DAC_CS1_PIN             GPIO_PIN_1
#define DAC_CS1_PORT            GPIOC
#define DAC_CS2_PIN             GPIO_PIN_2
#define DAC_CS2_PORT            GPIOC
#define DAC_CS3_PIN             GPIO_PIN_3
#define DAC_CS3_PORT            GPIOC

/* ADC CS 핀 */
#define ADC_CS_PIN              GPIO_PIN_12
#define ADC_CS_PORT             GPIOB
#define ADC_IRQ_PIN             GPIO_PIN_11
#define ADC_IRQ_PORT            GPIOB

/* 상태 LED */
#define LED_STATUS_PIN          GPIO_PIN_13
#define LED_STATUS_PORT         GPIOC

/* =========================================================================
 * 채널 데이터 구조체
 * ========================================================================= */
typedef enum {
    FAULT_NONE    = 0x00,
    FAULT_SHORT   = 0x01,   /* 단락 */
    FAULT_OPEN    = 0x02,   /* 단선 */
    FAULT_OVERTEMP= 0x04,   /* 과온 (예비) */
} FaultStatus_t;

typedef struct {
    uint16_t    target_mv;          /* 목표 전압 (mV) */
    uint16_t    measured_mv;        /* 측정 전압 (mV) */
    int16_t     current_ma;         /* 측정 전류 (mA, 양수=정상) */
    uint16_t    dac_code;           /* 현재 DAC 코드 */
    int32_t     pi_integral;        /* PI 적분항 (× 1000 스케일) */
    FaultStatus_t fault;            /* 결함 상태 */
    uint8_t     enabled;            /* 채널 활성화 여부 */
} ChannelData_t;

/* =========================================================================
 * UART 명령 구조체
 * ========================================================================= */
typedef enum {
    CMD_SET_VOLTAGE  = 0x01,    /* SET <ch> <mV> */
    CMD_GET_STATUS   = 0x02,    /* GET */
    CMD_ENABLE_CH    = 0x03,    /* EN <ch> */
    CMD_DISABLE_CH   = 0x04,    /* DIS <ch> */
    CMD_RESET_FAULT  = 0x05,    /* RST <ch> */
} CmdType_t;

typedef struct {
    CmdType_t   type;
    uint8_t     channel;        /* 0-based (0~3) */
    uint16_t    value;          /* 전압 mV 등 */
} UartCmd_t;

/* =========================================================================
 * 외부 핸들 선언 (CubeMX 생성)
 * ========================================================================= */
extern SPI_HandleTypeDef    hspi1;      /* DAC SPI */
extern SPI_HandleTypeDef    hspi2;      /* ADC SPI */
extern UART_HandleTypeDef   huart1;     /* PC UART */
extern FDCAN_HandleTypeDef  hfdcan1;    /* FDCAN */
extern DMA_HandleTypeDef    hdma_usart1_tx;
extern DMA_HandleTypeDef    hdma_usart1_rx;

/* =========================================================================
 * FreeRTOS 객체 (extern)
 * ========================================================================= */
extern osMutexId_t          xMutexSPI1;
extern osMutexId_t          xMutexSPI2;
extern osMutexId_t          xMutexChannelData;
extern osMessageQueueId_t   xQueueUARTCmd;
extern osMessageQueueId_t   xQueueTxData;

/* =========================================================================
 * 공유 채널 데이터 (전역)
 * ========================================================================= */
extern volatile ChannelData_t g_channels[NUM_CHANNELS];

/* =========================================================================
 * 함수 선언
 * ========================================================================= */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
