/**
 * @file    main.h
 * @brief   STM32H5 FreeRTOS 4CH DAC/ADC 전압 제어 시스템
 *
 * === 하드웨어 구성 (STM32H563ZI Nucleo) ===
 *
 * [ DAC 출력 ]
 *   CH1 : DAC1_OUT1  -> PA4  (0~3.3V, 12-bit)
 *   CH2 : DAC1_OUT2  -> PA5  (0~3.3V, 12-bit)
 *   CH3 : MCP4922_A  -> SPI2 (PB15=MOSI, PB13=SCK, PB12=CS)
 *   CH4 : MCP4922_B  -> SPI2 (동일 칩, CHB 선택)
 *
 * [ ADC 입력 - 전압 감지 (분압기 사용) ]
 *   V_SENSE_CH1 : PA0  (ADC1_INP0)
 *   V_SENSE_CH2 : PA1  (ADC1_INP1)
 *   V_SENSE_CH3 : PA2  (ADC1_INP6)   -- PC0 사용 시 INP10
 *   V_SENSE_CH4 : PA3  (ADC1_INP7)
 *
 * [ ADC 입력 - 전류 감지 (션트 저항 + INA213/opamp) ]
 *   I_SENSE_CH1 : PC0  (ADC1_INP10)
 *   I_SENSE_CH2 : PC1  (ADC1_INP11)
 *   I_SENSE_CH3 : PC2  (ADC1_INP12)
 *   I_SENSE_CH4 : PC3  (ADC1_INP13)
 *
 * [ UART - PC 통신 ]
 *   USART3 : PD8(TX), PD9(RX) -> ST-Link Virtual COM Port
 *   Baud   : 115200, 8N1
 *
 * === 통신 프로토콜 (ASCII) ===
 *   PC -> MCU:
 *     SET <ch> <mV>       전압 설정 (ch=1~4, mV=0~3300)
 *     GET <ch>            특정 채널 상태 조회
 *     GET ALL             전체 채널 상태 조회
 *     EN <ch>             채널 활성화
 *     DIS <ch>            채널 비활성화
 *     KP <ch> <val>       PID Kp 설정
 *     KI <ch> <val>       PID Ki 설정
 *     KD <ch> <val>       PID Kd 설정
 *     ILIM <ch> <mA>      전류 제한값 설정
 *     HELP                명령어 목록 출력
 *
 *   MCU -> PC:
 *     OK
 *     ERR:<reason>
 *     DATA:<ch>,V=<mv>,I=<ma>,ST=<status>
 *     ALERT:<ch>,<OC|OV|OPEN>
 */

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * 시스템 파라미터
 * ========================================================= */
#define NUM_CHANNELS            4
#define VREF_MV                 3300.0f     /* ADC/DAC 기준 전압 (mV) */
#define ADC_RESOLUTION          4096.0f     /* 12-bit */
#define DAC_RESOLUTION          4096.0f     /* 12-bit */
#define DAC_MAX_CODE            4095U
#define VDAC_MAX_MV             3300.0f     /* DAC 최대 출력 전압 (mV) */

/* 전압 분압기 비율: V_actual = V_adc * V_DIV_RATIO
 * 예) 10V 측정 시 분압 1/3 사용 -> V_DIV_RATIO = 3.0f */
#define V_DIV_RATIO             1.0f

/* 전류 센싱 설정
 * I(mA) = Vadc(mV) / (SHUNT_MOHM * 0.001 * CURRENT_AMP_GAIN * 1000)
 * 예) 100mΩ 션트, 게인 20배 -> 1V = 500mA */
#define SHUNT_RESISTANCE_MOHM   100.0f
#define CURRENT_AMP_GAIN        20.0f

/* =========================================================
 * 보호 파라미터
 * ========================================================= */
#define MAX_CURRENT_MA              2000.0f     /* 채널당 최대 전류 (mA) */
#define OPEN_CIRCUIT_THRESHOLD_MA   5.0f        /* 오픈 서킷 판단 전류 (mA) */
#define OPEN_CIRCUIT_MIN_VOLTAGE_MV 100.0f      /* 오픈 서킷 감지 최소 설정전압 */
#define OPEN_CIRCUIT_TIMEOUT_MS     2000U       /* 오픈 서킷 감지 대기 시간 (ms) */
#define OVERCURRENT_LATCH           1           /* 1=래치(수동복구), 0=자동복구 */
#define OVERCURRENT_RECOVER_DELAY   5000U       /* 자동복구 대기 시간 (ms) */

/* =========================================================
 * FreeRTOS 태스크 우선순위
 * ========================================================= */
#define TASK_PRI_FAULT          (configMAX_PRIORITIES - 1)  /* 최고 우선순위 */
#define TASK_PRI_UART_RX        (configMAX_PRIORITIES - 2)
#define TASK_PRI_CONTROL        (configMAX_PRIORITIES - 3)
#define TASK_PRI_ADC_SAMPLE     (configMAX_PRIORITIES - 4)
#define TASK_PRI_UART_TX        (configMAX_PRIORITIES - 5)

/* FreeRTOS 스택 크기 (words) */
#define STACK_FAULT             256
#define STACK_UART_RX           512
#define STACK_CONTROL           512
#define STACK_ADC               256
#define STACK_UART_TX           512

/* =========================================================
 * 제어 루프 주기
 * ========================================================= */
#define CONTROL_PERIOD_MS       10U     /* 제어 루프 주기 (ms) */
#define ADC_SAMPLE_PERIOD_MS    5U      /* ADC 샘플링 주기 (ms) */
#define TELEMETRY_PERIOD_MS     100U    /* 텔레메트리 전송 주기 (ms) */
#define FAULT_CHECK_PERIOD_MS   1U      /* 폴트 감지 주기 (ms) */

/* =========================================================
 * 주변장치 핸들 (main.c에서 정의)
 * ========================================================= */
extern DAC_HandleTypeDef    hdac1;
extern SPI_HandleTypeDef    hspi2;
extern ADC_HandleTypeDef    hadc1;
extern UART_HandleTypeDef   huart3;
extern DMA_HandleTypeDef    hdma_adc1;

/* =========================================================
 * FreeRTOS 오브젝트 핸들 (main.c에서 정의)
 * ========================================================= */
extern QueueHandle_t        xCmdQueue;      /* 명령 큐 (PC -> 제어) */
extern QueueHandle_t        xTxQueue;       /* 전송 큐 (제어 -> UART TX) */
extern SemaphoreHandle_t    xAdcSemaphore;  /* ADC 변환 완료 세마포어 */
extern SemaphoreHandle_t    xChannelMutex;  /* 채널 데이터 뮤텍스 */

/* =========================================================
 * GPIO 정의
 * ========================================================= */
/* MCP4922 외부 DAC SPI CS 핀 */
#define MCP4922_CS_GPIO         GPIOB
#define MCP4922_CS_PIN          GPIO_PIN_12

/* 상태 LED (Nucleo 기본 LED) */
#define LED_GREEN_GPIO          GPIOA
#define LED_GREEN_PIN           GPIO_PIN_5
#define LED_RED_GPIO            GPIOG
#define LED_RED_PIN             GPIO_PIN_0

/* =========================================================
 * 오류 핸들러
 * ========================================================= */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
