/**
 * @file    freertos.c
 * @brief   FreeRTOS 태스크 및 동기화 객체 정의
 *
 * ====================================================
 *  태스크 구성
 * ====================================================
 *
 *  [PIDControlTask]  — 10ms 주기, 오실리스크 High 우선순위
 *    4채널 ADC 읽기 → PID 계산 → DAC 갱신
 *    VCtrl_Update() 호출
 *
 *  [MonitorTask]     — 500ms 주기, Normal 우선순위
 *    UART로 채널 상태(설정값/측정값/DAC출력/오차) 출력
 *
 * ====================================================
 *  동기화
 * ====================================================
 *  - g_voltageCtrl.data_mutex : 채널 데이터 보호 뮤텍스
 *  - ADC INT 인터럽트 → g_adcReadySem (바이너리 세마포어)
 *    * MCP3465R_WaitReady()에서 폴링 방식으로 대기하므로
 *      현재 구현에서는 세마포어 없이 INT 핀 직접 폴링
 *    * 인터럽트 방식으로 전환 시 아래 주석 해제
 */

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "main.h"
#include "voltage_control.h"

#include <stdio.h>
#include <string.h>

/* ================================================================== */
/*  전역 제어 시스템 핸들                                                  */
/* ================================================================== */
VCtrl_HandleTypeDef g_voltageCtrl;

/* ================================================================== */
/*  태스크 핸들                                                           */
/* ================================================================== */
static osThreadId_t g_pidTaskHandle;
static osThreadId_t g_monitorTaskHandle;

/* ================================================================== */
/*  태스크 속성                                                           */
/* ================================================================== */
static const osThreadAttr_t k_pidTaskAttr = {
    .name       = "PIDControl",
    .stack_size = 512 * 4,           /* 512 word = 2 kB */
    .priority   = osPriorityHigh,
};

static const osThreadAttr_t k_monitorTaskAttr = {
    .name       = "Monitor",
    .stack_size = 256 * 4,           /* 256 word = 1 kB */
    .priority   = osPriorityNormal,
};

/* ================================================================== */
/*  태스크 함수 프로토타입                                                  */
/* ================================================================== */
static void PIDControlTask(void *pvParameters);
static void MonitorTask(void *pvParameters);

/* ================================================================== */
/*  UART 디버그 출력 헬퍼                                                  */
/* ================================================================== */
static void dbg_print(const char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, (uint16_t)strlen(str), 100);
}

/* ================================================================== */
/*  FreeRTOS 초기화 (main.c에서 호출)                                    */
/* ================================================================== */
void MX_FREERTOS_Init(void)
{
    /* osKernelInitialize()는 STM32CubeIDE가 자동 호출하거나
     * main()에서 MX_FREERTOS_Init() 전에 호출해야 함.
     * CubeMX 미사용 시 직접 호출: */
    osKernelInitialize();

    /* 전압 제어 시스템 초기화 */
    if (VCtrl_Init(&g_voltageCtrl, &hspi1, &hspi2) != HAL_OK) {
        /* 초기화 실패 — 오류 처리 */
        Error_Handler();
    }

    /* 태스크 생성 */
    g_pidTaskHandle     = osThreadNew(PIDControlTask, NULL, &k_pidTaskAttr);
    g_monitorTaskHandle = osThreadNew(MonitorTask,    NULL, &k_monitorTaskAttr);

    if (g_pidTaskHandle == NULL || g_monitorTaskHandle == NULL) {
        Error_Handler();
    }
}

/* ================================================================== */
/*  PIDControlTask — 10ms 주기 PID 제어 루프                            */
/* ================================================================== */
static void PIDControlTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t       xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod       = pdMS_TO_TICKS(10); /* 10 ms = 100 Hz */

    dbg_print("[PID] Task started\r\n");

    for (;;) {
        /* 정확한 주기 실행 (jitter 최소화) */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* 4채널 ADC 읽기 → PID 계산 → DAC 갱신 */
        HAL_StatusTypeDef st = VCtrl_Update(&g_voltageCtrl);
        if (st != HAL_OK) {
            /* 연속 오류 발생 시 비상 정지 (선택적 로직) */
            if (g_voltageCtrl.error_count > 10U) {
                VCtrl_EmergencyStop(&g_voltageCtrl);
                dbg_print("[PID] ERROR: Emergency stop triggered\r\n");
                g_voltageCtrl.error_count = 0;
                osDelay(1000);  /* 1초 대기 후 재시작 */

                /* 재시작 — 모든 채널 재활성화 */
                for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
                    VCtrl_EnableChannel(&g_voltageCtrl, ch, true);
                }
            }
        }
    }
}

/* ================================================================== */
/*  MonitorTask — 500ms 주기 상태 출력                                  */
/* ================================================================== */
static void MonitorTask(void *pvParameters)
{
    (void)pvParameters;

    char    line[128];
    uint32_t last_loop = 0;

    osDelay(500); /* 초기화 완료 대기 */
    dbg_print("[MON] Monitor started\r\n");
    dbg_print("CH | Setpoint | Measured | DAC Out  | Error\r\n");
    dbg_print("---+---------+---------+---------+--------\r\n");

    for (;;) {
        osDelay(500);

        /* 루프 카운트 출력 (PID 실행 빈도 확인) */
        uint32_t loops = g_voltageCtrl.loop_count;
        uint32_t delta = loops - last_loop;
        last_loop      = loops;

        snprintf(line, sizeof(line),
                 "\r\n[Loop:%lu  +%lu/500ms  Err:%lu]\r\n",
                 (unsigned long)loops,
                 (unsigned long)delta,
                 (unsigned long)g_voltageCtrl.error_count);
        dbg_print(line);
        dbg_print("CH | Setpt[V] | Meas[V]  | DACout[V]| Err[mV]\r\n");
        dbg_print("---+----------+----------+----------+--------\r\n");

        for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
            VCtrl_ChannelTypeDef status;
            VCtrl_GetChannelStatus(&g_voltageCtrl, ch, &status);

            snprintf(line, sizeof(line),
                     " %u | %8.4f | %8.4f | %8.4f | %+7.1f %s\r\n",
                     ch,
                     (double)status.setpoint,
                     (double)status.measured,
                     (double)status.dac_output,
                     (double)(status.error * 1000.0f),   /* V → mV */
                     status.enabled ? "" : "[DIS]");
            dbg_print(line);
        }
    }
}

/* ================================================================== */
/*  EXTI 콜백 — MCP3465R INT 핀 (PC0)                                  */
/*                                                                     */
/*  현재: MCP3465R_WaitReady()가 HAL_GPIO_ReadPin() 폴링으로 대기.       */
/*  인터럽트 방식 전환 시 이 콜백에서 세마포어 Give를 수행.                  */
/*                                                                     */
/*  전환 방법:                                                           */
/*   1. 아래 주석을 해제하고 g_adcReadySem 선언 추가                       */
/*   2. MCP3465R_WaitReady()를 세마포어 Take로 변경                       */
/* ================================================================== */
/*
static osSemaphoreId_t g_adcReadySem;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ADC_INT_PIN) {
        osSemaphoreRelease(g_adcReadySem);
    }
}
*/

/* ================================================================== */
/*  런타임 설정점 변경 예시 함수 (외부에서 호출 가능)                        */
/* ================================================================== */

/**
 * @brief 채널 목표 전압 실시간 변경
 * @param channel   0~3
 * @param setpoint  목표 전압 [V]
 */
void App_SetChannelVoltage(uint8_t channel, float setpoint)
{
    VCtrl_SetSetpoint(&g_voltageCtrl, channel, setpoint);
}

/**
 * @brief 채널 PID 이득 실시간 변경
 */
void App_SetChannelPIDGains(uint8_t channel, float Kp, float Ki, float Kd)
{
    VCtrl_SetGains(&g_voltageCtrl, channel, Kp, Ki, Kd);
}
