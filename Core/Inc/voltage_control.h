/**
 ******************************************************************************
 * @file    voltage_control.h
 * @brief   4채널 전압 PID 제어 시스템 헤더
 *
 * 시스템 구성:
 *   - AD5641 x4 (DAC) → 전압 출력 설정
 *   - MCP3465R (ADC) → 전압 피드백 읽기
 *   - PID x4 → 각 채널 독립 제어
 *   - FreeRTOS 태스크 기반 제어 루프
 *
 * FreeRTOS Tasks:
 *   1. vVoltageControlTask: PID 제어 루프 (주기: 10ms, 우선순위: High)
 *   2. vMonitorTask: UART 모니터링/명령 처리 (주기: 500ms, 우선순위: Normal)
 ******************************************************************************
 */

#ifndef __VOLTAGE_CONTROL_H
#define __VOLTAGE_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ad5641.h"
#include "mcp3465r.h"
#include "pid_controller.h"
#include <stdbool.h>

/* Defines -------------------------------------------------------------------*/

/* FreeRTOS task priorities */
#define VOLTAGE_CTRL_TASK_PRIORITY      (configMAX_PRIORITIES - 1)
#define MONITOR_TASK_PRIORITY           (configMAX_PRIORITIES - 3)

/* FreeRTOS task stack sizes (words) */
#define VOLTAGE_CTRL_TASK_STACK_SIZE    512
#define MONITOR_TASK_STACK_SIZE         512

/* Default PID gains */
#define PID_DEFAULT_KP                  2.0f
#define PID_DEFAULT_KI                  5.0f
#define PID_DEFAULT_KD                  0.1f

/* PID output is voltage to write to DAC (0~5V range) */
#define PID_OUTPUT_MIN                  0.0f
#define PID_OUTPUT_MAX                  5.0f

/* PID integral anti-windup limits */
#define PID_INTEGRAL_MIN                (-2.0f)
#define PID_INTEGRAL_MAX                2.0f

/* Derivative filter coefficient (0.0=no filter, 0.9=heavy filter) */
#define PID_D_FILTER_ALPHA              0.3f

/* Error threshold for "settled" detection (V) */
#define VOLTAGE_SETTLED_THRESHOLD       0.01f

/* UART command buffer size */
#define CMD_BUFFER_SIZE                 128

/* Types ---------------------------------------------------------------------*/

/**
 * @brief 채널 상태 열거형
 */
typedef enum {
    CH_STATE_IDLE = 0,       /**< 비활성 */
    CH_STATE_ACTIVE,         /**< 제어 중 */
    CH_STATE_SETTLED,        /**< 목표 전압 안정 */
    CH_STATE_ERROR           /**< 오류 발생 */
} ChannelState_t;

/**
 * @brief 개별 채널 상태 구조체
 */
typedef struct {
    uint8_t         channel_id;     /**< 채널 번호 (0~3) */
    ChannelState_t  state;          /**< 채널 상태 */
    float           target_voltage; /**< 목표 전압 (V) */
    float           actual_voltage; /**< 측정 전압 (V) */
    float           dac_output;     /**< DAC 출력 전압 (V) */
    float           error;          /**< 현재 오차 (V) */
    uint32_t        settled_count;  /**< 안정 상태 카운터 */
    PID_Handle_t    pid;            /**< PID 제어기 */
} VoltageChannel_t;

/**
 * @brief 전압 제어 시스템 구조체
 */
typedef struct {
    VoltageChannel_t channels[NUM_CHANNELS];  /**< 4채널 상태 */
    AD5641_Handle_t  dac[NUM_CHANNELS];       /**< AD5641 DAC x4 */
    MCP3465R_Handle_t adc;                    /**< MCP3465R ADC */
    bool             system_enabled;           /**< 시스템 활성화 */
    uint32_t         loop_count;               /**< 제어 루프 카운터 */
    uint32_t         error_count;              /**< 오류 카운터 */
} VoltageControlSystem_t;

/* Exported variables --------------------------------------------------------*/
extern VoltageControlSystem_t g_vcs;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  전압 제어 시스템 초기화
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef VCS_Init(void);

/**
 * @brief  FreeRTOS 태스크 생성 및 시작
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef VCS_StartTasks(void);

/**
 * @brief  채널 목표 전압 설정
 * @param  channel: 채널 번호 (0~3)
 * @param  voltage: 목표 전압 (0.5V ~ 4.5V)
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef VCS_SetTargetVoltage(uint8_t channel, float voltage);

/**
 * @brief  채널 PID 게인 설정
 * @param  channel: 채널 번호 (0~3)
 * @param  kp: 비례 게인
 * @param  ki: 적분 게인
 * @param  kd: 미분 게인
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef VCS_SetPIDGains(uint8_t channel, float kp, float ki,
                                   float kd);

/**
 * @brief  채널 활성화/비활성화
 * @param  channel: 채널 번호 (0~3)
 * @param  enable: true=활성화
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef VCS_EnableChannel(uint8_t channel, bool enable);

/**
 * @brief  전체 시스템 활성화/비활성화
 * @param  enable: true=활성화
 */
void VCS_EnableSystem(bool enable);

/**
 * @brief  채널 상태 조회
 * @param  channel: 채널 번호 (0~3)
 * @retval VoltageChannel_t 포인터 (NULL=잘못된 채널)
 */
const VoltageChannel_t *VCS_GetChannelStatus(uint8_t channel);

/**
 * @brief  전압 제어 태스크 (FreeRTOS)
 * @param  pvParameters: 태스크 파라미터 (사용 안함)
 */
void vVoltageControlTask(void *pvParameters);

/**
 * @brief  모니터링/명령 처리 태스크 (FreeRTOS)
 * @param  pvParameters: 태스크 파라미터 (사용 안함)
 */
void vMonitorTask(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* __VOLTAGE_CONTROL_H */
