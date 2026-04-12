/**
 * @file    voltage_control.h
 * @brief   4채널 전압 PID 제어 — 채널 관리 및 제어 루프
 *
 * 시스템 구성:
 *   AD5641  (DAC)    : 채널별 제어 전압 설정 (전력단 드라이브)
 *   가변 부하         : 채널 출력에 연결된 동적 부하 → 전압 변동 유발
 *   MCP3465R (ADC)   : 실제 출력 전압 측정 (분압기를 통해 ADC 입력 범위 조정)
 *   PID 제어기        : 측정값 ≡ 설정값이 되도록 DAC 값을 조절
 */
#ifndef INC_VOLTAGE_CONTROL_H_
#define INC_VOLTAGE_CONTROL_H_

#include "pid.h"
#include "ad5641.h"
#include "mcp3465r.h"
#include "cmsis_os2.h"

/* ------------------------------------------------------------------ */
/*  채널 수 및 기본 PID 파라미터                                           */
/* ------------------------------------------------------------------ */
#define VCTRL_NUM_CHANNELS      4U

/** 기본 PID 이득 (실제 플랜트에 맞게 튜닝 필요) */
#define VCTRL_DEFAULT_KP        1.2f
#define VCTRL_DEFAULT_KI        0.5f
#define VCTRL_DEFAULT_KD        0.05f

/** PID 제어 주기 [s] (FreeRTOS 태스크 주기와 일치) */
#define VCTRL_SAMPLE_TIME_S     0.010f   /* 10 ms */

/** DAC 출력 범위 [V] (AD5641 + 전력단 기준) */
#define VCTRL_DAC_OUTPUT_MIN    0.0f
#define VCTRL_DAC_OUTPUT_MAX    3.3f

/** 기본 채널별 목표 전압 [V] */
#define VCTRL_DEFAULT_SETPOINT_CH0  1.0f
#define VCTRL_DEFAULT_SETPOINT_CH1  1.5f
#define VCTRL_DEFAULT_SETPOINT_CH2  2.0f
#define VCTRL_DEFAULT_SETPOINT_CH3  2.5f

/* ------------------------------------------------------------------ */
/*  채널 상태 구조체                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    float     setpoint;     /**< 목표 전압 [V] */
    float     measured;     /**< 측정 전압 [V] (MCP3465R) */
    float     dac_output;   /**< DAC 설정 전압 [V] (AD5641) */
    float     error;        /**< 현재 오차 [V] */
    bool      enabled;      /**< 채널 활성 여부 */
    PID_HandleTypeDef pid;  /**< 채널 전용 PID 핸들 */
} VCtrl_ChannelTypeDef;

/* ------------------------------------------------------------------ */
/*  전체 제어 시스템 핸들                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    AD5641_HandleTypeDef    dac;    /**< AD5641 드라이버 핸들 */
    MCP3465R_HandleTypeDef  adc;    /**< MCP3465R 드라이버 핸들 */
    VCtrl_ChannelTypeDef    ch[VCTRL_NUM_CHANNELS]; /**< 채널 상태 배열 */

    osMutexId_t data_mutex;         /**< 채널 데이터 접근 뮤텍스 */
    uint32_t    loop_count;         /**< 제어 루프 실행 횟수 (통계용) */
    uint32_t    error_count;        /**< 오류 발생 횟수 */
} VCtrl_HandleTypeDef;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief 전체 시스템 초기화 (드라이버 + PID + FreeRTOS 객체)
 * @param hctrl  제어 시스템 핸들 (전역 변수 주소 전달)
 * @param hspi1  AD5641용 SPI 핸들
 * @param hspi2  MCP3465R용 SPI 핸들
 */
HAL_StatusTypeDef VCtrl_Init(VCtrl_HandleTypeDef *hctrl,
                              SPI_HandleTypeDef   *hspi1,
                              SPI_HandleTypeDef   *hspi2);

/**
 * @brief 채널 목표 전압 설정 (스레드 안전)
 * @param channel  0~3
 * @param setpoint 목표 전압 [V]
 */
HAL_StatusTypeDef VCtrl_SetSetpoint(VCtrl_HandleTypeDef *hctrl,
                                     uint8_t channel, float setpoint);

/**
 * @brief 채널 PID 이득 변경 (스레드 안전)
 */
HAL_StatusTypeDef VCtrl_SetGains(VCtrl_HandleTypeDef *hctrl,
                                  uint8_t channel,
                                  float Kp, float Ki, float Kd);

/**
 * @brief 채널 활성/비활성 (비활성 시 DAC 0V 출력)
 */
HAL_StatusTypeDef VCtrl_EnableChannel(VCtrl_HandleTypeDef *hctrl,
                                       uint8_t channel, bool enable);

/**
 * @brief 채널 상태 읽기 (스레드 안전 복사)
 */
HAL_StatusTypeDef VCtrl_GetChannelStatus(VCtrl_HandleTypeDef    *hctrl,
                                          uint8_t                 channel,
                                          VCtrl_ChannelTypeDef   *out);

/**
 * @brief PID 제어 루프 1회 실행 (FreeRTOS PID 태스크에서 호출)
 *        4채널 ADC 읽기 → PID 계산 → DAC 갱신
 */
HAL_StatusTypeDef VCtrl_Update(VCtrl_HandleTypeDef *hctrl);

/**
 * @brief 모든 채널 DAC 출력을 0V로 초기화 (비상 정지)
 */
void VCtrl_EmergencyStop(VCtrl_HandleTypeDef *hctrl);

#endif /* INC_VOLTAGE_CONTROL_H_ */
