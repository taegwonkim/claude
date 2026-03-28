/**
 * @file    freertos_tasks.c
 * @brief   FreeRTOS 태스크 구현
 *
 * 태스크 구조:
 * ┌────────────────────────────────────────────────────────────────────┐
 * │  vADCTask (최고 우선순위)                                           │
 * │    ADC DMA 완료 세마포어 대기 → VCtrl_UpdateFeedback() 호출        │
 * │    → xADCDoneSem Give (vControlTask 깨움)                          │
 * ├────────────────────────────────────────────────────────────────────┤
 * │  vControlTask (높은 우선순위)                                       │
 * │    xADCDoneSem 대기 → PID 연산 → DAC/PWM 출력 갱신                │
 * │    주기: ADC 완료 이벤트 기반 (≈ 1 kHz ADC DMA 완료율)            │
 * │    PID는 10 ms 마다 실행 (vTaskDelayUntil 활용)                    │
 * ├────────────────────────────────────────────────────────────────────┤
 * │  vMonitorTask (낮은 우선순위)                                       │
 * │    500 ms 주기로 UART를 통해 채널 상태 출력                        │
 * └────────────────────────────────────────────────────────────────────┘
 */

#include "main.h"
#include "voltage_ctrl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>

/* ======================================================================== */
/*  제어 주기 설정                                                            */
/* ======================================================================== */

/** PID 제어 주기 [ms] — pid.h의 dt(0.01s)와 일치해야 함 */
#define CTRL_PERIOD_MS          10U

/** 모니터 출력 주기 [ms] */
#define MONITOR_PERIOD_MS       500U

/** ADC 세마포어 최대 대기 시간 */
#define ADC_SEM_TIMEOUT_MS      50U

/* ======================================================================== */
/*  목표 전압 테이블 (예시, 런타임 변경 가능)                                */
/* ======================================================================== */

/**
 * 초기 목표 전압 설정 [V]
 * 실제 애플리케이션에서는 UART 명령, EEPROM, 또는 상위 제어기로부터 설정 가능
 */
static const float s_default_setpoints[VCTRL_NUM_CHANNELS] = {
    1.0f,   /**< 채널 0: 1.0 V */
    1.5f,   /**< 채널 1: 1.5 V */
    2.0f,   /**< 채널 2: 2.0 V */
    2.5f,   /**< 채널 3: 2.5 V */
};

/* ======================================================================== */
/*  UART 전송 헬퍼 (printf 대체, FreeRTOS safe)                              */
/* ======================================================================== */

static char s_uart_buf[256];

static void uart_print(const char *str)
{
    uint16_t len = (uint16_t)strlen(str);
    /* 폴링 모드 전송 (모니터 태스크에서만 사용, 낮은 우선순위) */
    HAL_UART_Transmit(&huart3, (const uint8_t *)str, len, 100);
}

/* ======================================================================== */
/*  vADCTask                                                                  */
/* ======================================================================== */

/**
 * @brief  ADC DMA 완료 시 피드백 데이터를 업데이트하는 태스크
 *
 * 동작 흐름:
 *  1. HAL_ADC_ConvCpltCallback()에서 xADCDoneSem을 Give
 *  2. 이 태스크가 세마포어를 Take하여 깨어남
 *  3. DMA 버퍼에서 ADC 값을 읽어 VCtrl_UpdateFeedback() 호출
 *
 * @param  pvParameters  main()에서 DMA 버퍼 포인터(uint16_t*)를 전달
 */
void vADCTask(void *pvParameters)
{
    uint16_t *adc_buf = (uint16_t *)pvParameters;

    /* 초기 목표 전압 설정 */
    for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
        VCtrl_SetTarget(i, s_default_setpoints[i]);
    }

    uart_print("[ADC] Task started\r\n");

    while (1) {
        /* ADC DMA 완료 인터럽트 대기 */
        if (xSemaphoreTake(xADCDoneSem, pdMS_TO_TICKS(ADC_SEM_TIMEOUT_MS)) == pdTRUE) {
            /*
             * DMA 버퍼가 캐시에 올라있는 경우 캐시 무효화 필요
             * (STM32H5 D-Cache 활성화 시)
             */
#if defined(STM32H5xx) && defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
            SCB_InvalidateDCache_by_Addr((uint32_t *)adc_buf,
                                         VCTRL_NUM_CHANNELS * sizeof(uint16_t));
#endif
            /* 뮤텍스 보호 아래 피드백 업데이트 */
            if (xSemaphoreTake(xCtrlMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                VCtrl_UpdateFeedback(adc_buf);
                xSemaphoreGive(xCtrlMutex);
            }
        }
        /* else: ADC 타임아웃 → 오류 로깅 가능 */
    }
}

/* ======================================================================== */
/*  vControlTask                                                              */
/* ======================================================================== */

/**
 * @brief  PID 연산 및 DAC/PWM 출력을 갱신하는 제어 태스크
 *
 * CTRL_PERIOD_MS(10 ms) 주기로 실행.
 * vTaskDelayUntil을 사용하여 드리프트 없이 정확한 주기를 유지.
 */
void vControlTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(CTRL_PERIOD_MS);

    uart_print("[CTRL] Task started\r\n");

    while (1) {
        /* 정확한 주기 대기 */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* 뮤텍스 보호 아래 PID 연산 + 출력 적용 */
        if (xSemaphoreTake(xCtrlMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            VCtrl_RunControl();
            xSemaphoreGive(xCtrlMutex);
        }
    }
}

/* ======================================================================== */
/*  vMonitorTask                                                              */
/* ======================================================================== */

/**
 * @brief  채널 상태를 UART로 출력하는 모니터링 태스크
 *
 * 출력 형식 (500 ms 주기):
 * ---[ DAC/ADC Feedback Controller ]---
 * CH0: SET=1.000V  FB=0.998V  ERR=+0.002V  OUT=1240
 * CH1: SET=1.500V  FB=1.497V  ERR=+0.003V  OUT=1860
 * CH2: SET=2.000V  FB=1.995V  ERR=+0.005V  OUT=2482
 * CH3: SET=2.500V  FB=2.501V  ERR=-0.001V  OUT=3103
 */
void vMonitorTask(void *pvParameters)
{
    (void)pvParameters;

    const TickType_t xPeriod = pdMS_TO_TICKS(MONITOR_PERIOD_MS);
    VCtrl_Channel_t ch_info;

    /* 부팅 배너 */
    uart_print("\r\n========================================\r\n");
    uart_print("  STM32H5 DAC/ADC Feedback Controller\r\n");
    uart_print("  4-Channel PID Voltage Regulator\r\n");
    uart_print("========================================\r\n");

    while (1) {
        vTaskDelay(xPeriod);

        uart_print("---[ Status ]---\r\n");

        for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
            /* 안전한 복사본 획득 */
            if (xSemaphoreTake(xCtrlMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                VCtrl_GetChannelInfo(i, &ch_info);
                xSemaphoreGive(xCtrlMutex);
            } else {
                continue;
            }

            int len = snprintf(s_uart_buf, sizeof(s_uart_buf),
                "CH%u: SET=%5.3fV  FB=%5.3fV  ERR=%+6.3fV  OUT=%4u  [%s]\r\n",
                i,
                (double)ch_info.setpoint_v,
                (double)ch_info.feedback_v,
                (double)ch_info.error_v,
                (unsigned int)(uint32_t)ch_info.output_raw,
                (ch_info.out_type == VCTRL_OUT_DAC) ? "DAC" : "PWM"
            );
            if (len > 0) {
                uart_print(s_uart_buf);
            }
        }
        uart_print("\r\n");
    }
}

/* ======================================================================== */
/*  UART 수신 명령 처리 (선택적 확장)                                        */
/*                                                                            */
/*  명령 형식: "SET <ch> <voltage>\r\n"                                      */
/*  예시     : "SET 0 2.5\r\n"  → 채널 0 목표 전압을 2.5V로 설정            */
/*                                                                            */
/*  실제 프로젝트에서는 별도 vUARTCommandTask로 분리하여 구현 권장           */
/* ======================================================================== */

/**
 * @brief  UART 수신 문자열로부터 SET 명령을 파싱하여 채널 setpoint를 갱신
 * @param  cmd  NULL-terminated 명령 문자열
 */
void VCtrl_ParseCommand(const char *cmd)
{
    unsigned int ch;
    double volt;

    if (sscanf(cmd, "SET %u %lf", &ch, &volt) == 2) {
        if (ch < VCTRL_NUM_CHANNELS) {
            VCtrl_SetTarget((uint8_t)ch, (float)volt);

            int len = snprintf(s_uart_buf, sizeof(s_uart_buf),
                "[CMD] CH%u setpoint -> %.3fV\r\n", ch, volt);
            if (len > 0) uart_print(s_uart_buf);
        }
    }
}
