/**
 ******************************************************************************
 * @file    voltage_control.c
 * @brief   4채널 전압 PID 제어 시스템 구현
 *
 * 시스템 동작 흐름:
 *   1. VCS_Init()으로 하드웨어 및 PID 초기화
 *   2. VCS_StartTasks()로 FreeRTOS 태스크 생성
 *   3. vVoltageControlTask: 10ms 주기로 PID 제어 루프 실행
 *      - MCP3465R에서 4채널 전압 읽기
 *      - 각 채널 PID 연산
 *      - AD5641 DAC로 보정된 전압 출력
 *   4. vMonitorTask: 500ms 주기로 상태 출력 및 UART 명령 처리
 *
 * UART 명령 형식:
 *   "Sn=V.VV"   : 채널 n 목표 전압 설정 (예: S0=2.50)
 *   "Pn=V.VV"   : 채널 n Kp 설정 (예: P0=2.00)
 *   "In=V.VV"   : 채널 n Ki 설정 (예: I0=5.00)
 *   "Dn=V.VV"   : 채널 n Kd 설정 (예: D0=0.10)
 *   "En"         : 채널 n 활성화 (예: E0)
 *   "Xn"         : 채널 n 비활성화 (예: X0)
 *   "STATUS"     : 전체 상태 출력
 *   "RESET"      : 시스템 리셋
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "voltage_control.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Private defines -----------------------------------------------------------*/
#define UART_TX_TIMEOUT     100
#define SETTLED_COUNT_THRESHOLD  50  /* 50 × 10ms = 0.5초 안정 유지 시 settled */

/* Private variables ---------------------------------------------------------*/

/* 글로벌 전압 제어 시스템 인스턴스 */
VoltageControlSystem_t g_vcs;

/* 뮤텍스: 다중 태스크에서 시스템 상태 접근 보호 */
static SemaphoreHandle_t s_vcs_mutex;

/* UART 수신 버퍼 */
static char s_uart_rx_buf[CMD_BUFFER_SIZE];
static volatile uint16_t s_uart_rx_idx = 0;

/* UART 송신 버퍼 */
static char s_uart_tx_buf[512];

/* DAC CS 핀 매핑 테이블 */
static const struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} s_dac_cs_pins[NUM_CHANNELS] = {
    { DAC_CS0_PORT, DAC_CS0_PIN },
    { DAC_CS1_PORT, DAC_CS1_PIN },
    { DAC_CS2_PORT, DAC_CS2_PIN },
    { DAC_CS3_PORT, DAC_CS3_PIN },
};

/* External variables --------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart2;

/* Private function prototypes -----------------------------------------------*/
static void VCS_InitChannels(void);
static void VCS_ProcessCommand(const char *cmd);
static void VCS_PrintStatus(void);
static void VCS_PrintChannelStatus(uint8_t ch);
static void UART_Print(const char *str);

/* Exported functions --------------------------------------------------------*/

HAL_StatusTypeDef VCS_Init(void)
{
    HAL_StatusTypeDef status;

    memset(&g_vcs, 0, sizeof(g_vcs));

    /* 뮤텍스 생성 */
    s_vcs_mutex = xSemaphoreCreateMutex();
    if (s_vcs_mutex == NULL) {
        return HAL_ERROR;
    }

    /* AD5641 DAC x4 초기화 */
    for (int i = 0; i < NUM_CHANNELS; i++) {
        status = AD5641_Init(&g_vcs.dac[i], &hspi1,
                              s_dac_cs_pins[i].port, s_dac_cs_pins[i].pin,
                              DAC_VREF);
        if (status != HAL_OK) {
            return status;
        }
    }

    /* MCP3465R ADC 초기화 */
    status = MCP3465R_Init(&g_vcs.adc, &hspi2,
                            ADC_CS_PORT, ADC_CS_PIN,
                            ADC_VREF, ADC_VOLTAGE_DIVIDER_SCALE);
    if (status != HAL_OK) {
        return status;
    }

    /* IRQ 핀 설정 */
    MCP3465R_SetIRQPin(&g_vcs.adc, ADC_IRQ_PORT, ADC_IRQ_PIN);

    /* 채널 초기화 */
    VCS_InitChannels();

    g_vcs.system_enabled = true;
    g_vcs.loop_count = 0;
    g_vcs.error_count = 0;

    return HAL_OK;
}

HAL_StatusTypeDef VCS_StartTasks(void)
{
    BaseType_t ret;

    /* PID 제어 태스크 (최고 우선순위, 10ms 주기) */
    ret = xTaskCreate(vVoltageControlTask, "VoltCtrl",
                       VOLTAGE_CTRL_TASK_STACK_SIZE, NULL,
                       VOLTAGE_CTRL_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        return HAL_ERROR;
    }

    /* 모니터링/명령 처리 태스크 */
    ret = xTaskCreate(vMonitorTask, "Monitor",
                       MONITOR_TASK_STACK_SIZE, NULL,
                       MONITOR_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef VCS_SetTargetVoltage(uint8_t channel, float voltage)
{
    if (channel >= NUM_CHANNELS) {
        return HAL_ERROR;
    }

    /* 전압 범위 제한 */
    if (voltage < VOLTAGE_MIN) voltage = VOLTAGE_MIN;
    if (voltage > VOLTAGE_MAX) voltage = VOLTAGE_MAX;

    if (xSemaphoreTake(s_vcs_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_vcs.channels[channel].target_voltage = voltage;
        g_vcs.channels[channel].settled_count = 0;
        g_vcs.channels[channel].state = CH_STATE_ACTIVE;
        xSemaphoreGive(s_vcs_mutex);
        return HAL_OK;
    }

    return HAL_ERROR;
}

HAL_StatusTypeDef VCS_SetPIDGains(uint8_t channel, float kp, float ki,
                                   float kd)
{
    if (channel >= NUM_CHANNELS) {
        return HAL_ERROR;
    }

    if (xSemaphoreTake(s_vcs_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        PID_SetGains(&g_vcs.channels[channel].pid, kp, ki, kd);
        xSemaphoreGive(s_vcs_mutex);
        return HAL_OK;
    }

    return HAL_ERROR;
}

HAL_StatusTypeDef VCS_EnableChannel(uint8_t channel, bool enable)
{
    if (channel >= NUM_CHANNELS) {
        return HAL_ERROR;
    }

    if (xSemaphoreTake(s_vcs_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        VoltageChannel_t *ch = &g_vcs.channels[channel];

        if (enable) {
            PID_SetEnabled(&ch->pid, true);
            ch->state = CH_STATE_ACTIVE;
        } else {
            PID_SetEnabled(&ch->pid, false);
            ch->state = CH_STATE_IDLE;
            /* DAC 출력 0V */
            AD5641_SetVoltage(&g_vcs.dac[channel], 0.0f);
        }

        xSemaphoreGive(s_vcs_mutex);
        return HAL_OK;
    }

    return HAL_ERROR;
}

void VCS_EnableSystem(bool enable)
{
    if (xSemaphoreTake(s_vcs_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_vcs.system_enabled = enable;

        if (!enable) {
            /* 시스템 비활성화 시 모든 DAC 출력 0V */
            for (int i = 0; i < NUM_CHANNELS; i++) {
                AD5641_SetVoltage(&g_vcs.dac[i], 0.0f);
                g_vcs.channels[i].state = CH_STATE_IDLE;
                PID_Reset(&g_vcs.channels[i].pid);
            }
        }

        xSemaphoreGive(s_vcs_mutex);
    }
}

const VoltageChannel_t *VCS_GetChannelStatus(uint8_t channel)
{
    if (channel >= NUM_CHANNELS) {
        return NULL;
    }
    return &g_vcs.channels[channel];
}

/* FreeRTOS Tasks ------------------------------------------------------------*/

/**
 * @brief  전압 PID 제어 태스크
 *
 * 10ms 주기로 실행:
 *   1. ADC에서 4채널 전압 읽기
 *   2. 각 채널에 PID 연산 수행
 *   3. PID 출력을 DAC에 쓰기
 *   4. 채널 상태 업데이트
 */
void vVoltageControlTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime;
    float measured_voltages[NUM_CHANNELS];

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* 정확한 주기 유지를 위한 절대 시간 기반 딜레이 */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PID_CONTROL_PERIOD_MS));

        if (!g_vcs.system_enabled) {
            continue;
        }

        /* 1. 4채널 전압 읽기 */
        HAL_StatusTypeDef adc_status =
            MCP3465R_ReadAllChannels(&g_vcs.adc, measured_voltages);

        if (adc_status != HAL_OK) {
            g_vcs.error_count++;
            /* ADC 오류 시 이전 값 유지, 제어 건너뛰기 */
            continue;
        }

        /* 2. 뮤텍스 획득 후 각 채널 PID 제어 */
        if (xSemaphoreTake(s_vcs_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            for (int i = 0; i < NUM_CHANNELS; i++) {
                VoltageChannel_t *ch = &g_vcs.channels[i];

                if (ch->state == CH_STATE_IDLE) {
                    continue;
                }

                /* 측정 전압 업데이트 */
                ch->actual_voltage = measured_voltages[i];
                ch->error = ch->target_voltage - ch->actual_voltage;

                /* PID 연산 */
                float pid_output = PID_Compute(&ch->pid,
                                                ch->target_voltage,
                                                ch->actual_voltage);

                /* DAC 출력 설정 */
                ch->dac_output = pid_output;
                HAL_StatusTypeDef dac_status =
                    AD5641_SetVoltage(&g_vcs.dac[i], pid_output);

                if (dac_status != HAL_OK) {
                    ch->state = CH_STATE_ERROR;
                    g_vcs.error_count++;
                    continue;
                }

                /* 안정 상태 판정 */
                if (ch->error > -VOLTAGE_SETTLED_THRESHOLD &&
                    ch->error < VOLTAGE_SETTLED_THRESHOLD) {
                    ch->settled_count++;
                    if (ch->settled_count >= SETTLED_COUNT_THRESHOLD) {
                        ch->state = CH_STATE_SETTLED;
                    }
                } else {
                    ch->settled_count = 0;
                    ch->state = CH_STATE_ACTIVE;
                }
            }

            g_vcs.loop_count++;
            xSemaphoreGive(s_vcs_mutex);
        }

        /* 상태 LED 토글 (heartbeat) */
        if (g_vcs.loop_count % 50 == 0) {  /* 500ms마다 */
            HAL_GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
        }
    }
}

/**
 * @brief  모니터링 및 UART 명령 처리 태스크
 *
 * 500ms 주기로 실행:
 *   - 채널 상태를 UART로 출력
 *   - UART로 수신된 명령 처리
 */
void vMonitorTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime;
    uint8_t rx_byte;
    uint32_t print_counter = 0;

    xLastWakeTime = xTaskGetTickCount();

    /* 시작 메시지 출력 */
    UART_Print("\r\n========================================\r\n");
    UART_Print("  4-CH PID Voltage Controller v1.0\r\n");
    UART_Print("  MCU: STM32L552R | DAC: AD5641 x4\r\n");
    UART_Print("  ADC: MCP3465R   | RTOS: FreeRTOS\r\n");
    UART_Print("  Range: 0.5V ~ 4.5V\r\n");
    UART_Print("========================================\r\n");
    UART_Print("Commands:\r\n");
    UART_Print("  Sn=V.VV  Set target voltage (e.g. S0=2.50)\r\n");
    UART_Print("  Pn=V.VV  Set Kp (e.g. P0=2.00)\r\n");
    UART_Print("  In=V.VV  Set Ki (e.g. I0=5.00)\r\n");
    UART_Print("  Dn=V.VV  Set Kd (e.g. D0=0.10)\r\n");
    UART_Print("  En       Enable channel (e.g. E0)\r\n");
    UART_Print("  Xn       Disable channel (e.g. X0)\r\n");
    UART_Print("  STATUS   Print all status\r\n");
    UART_Print("  RESET    Reset system\r\n");
    UART_Print("========================================\r\n\r\n");

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(MONITOR_PERIOD_MS));

        /* UART 수신 처리 (폴링) */
        while (HAL_UART_Receive(&huart2, &rx_byte, 1, 0) == HAL_OK) {
            if (rx_byte == '\r' || rx_byte == '\n') {
                if (s_uart_rx_idx > 0) {
                    s_uart_rx_buf[s_uart_rx_idx] = '\0';
                    VCS_ProcessCommand(s_uart_rx_buf);
                    s_uart_rx_idx = 0;
                }
            } else if (s_uart_rx_idx < CMD_BUFFER_SIZE - 1) {
                s_uart_rx_buf[s_uart_rx_idx++] = (char)rx_byte;
            }
        }

        /* 주기적 상태 출력 (2초마다 = 4 × 500ms) */
        print_counter++;
        if (print_counter >= 4) {
            print_counter = 0;
            VCS_PrintStatus();
        }
    }
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  채널 초기 설정
 */
static void VCS_InitChannels(void)
{
    PID_Config_t pid_config = {
        .kp = PID_DEFAULT_KP,
        .ki = PID_DEFAULT_KI,
        .kd = PID_DEFAULT_KD,
        .dt = (float)PID_CONTROL_PERIOD_MS / 1000.0f,
        .out_min = PID_OUTPUT_MIN,
        .out_max = PID_OUTPUT_MAX,
        .integral_min = PID_INTEGRAL_MIN,
        .integral_max = PID_INTEGRAL_MAX,
        .d_filter_alpha = PID_D_FILTER_ALPHA,
    };

    for (int i = 0; i < NUM_CHANNELS; i++) {
        VoltageChannel_t *ch = &g_vcs.channels[i];

        ch->channel_id = (uint8_t)i;
        ch->state = CH_STATE_ACTIVE;
        ch->target_voltage = VOLTAGE_DEFAULT;
        ch->actual_voltage = 0.0f;
        ch->dac_output = 0.0f;
        ch->error = 0.0f;
        ch->settled_count = 0;

        PID_Init(&ch->pid, &pid_config);
    }
}

/**
 * @brief  UART 명령 파싱 및 실행
 */
static void VCS_ProcessCommand(const char *cmd)
{
    if (cmd == NULL || strlen(cmd) == 0) {
        return;
    }

    char response[128];

    /* STATUS 명령 */
    if (strcmp(cmd, "STATUS") == 0) {
        VCS_PrintStatus();
        return;
    }

    /* RESET 명령 */
    if (strcmp(cmd, "RESET") == 0) {
        VCS_EnableSystem(false);
        HAL_Delay(100);

        /* 모든 채널 재초기화 */
        VCS_InitChannels();
        g_vcs.system_enabled = true;
        g_vcs.loop_count = 0;
        g_vcs.error_count = 0;

        UART_Print("[OK] System reset complete\r\n");
        return;
    }

    /* 채널별 명령 (Sn=, Pn=, In=, Dn=, En, Xn) */
    char cmd_type = cmd[0];
    if (strlen(cmd) < 2) {
        UART_Print("[ERR] Invalid command\r\n");
        return;
    }

    uint8_t ch = (uint8_t)(cmd[1] - '0');
    if (ch >= NUM_CHANNELS) {
        UART_Print("[ERR] Invalid channel (0-3)\r\n");
        return;
    }

    float value = 0.0f;

    switch (cmd_type) {
    case 'S': case 's':  /* 목표 전압 설정 */
        if (strlen(cmd) < 4 || cmd[2] != '=') {
            UART_Print("[ERR] Format: Sn=V.VV\r\n");
            return;
        }
        value = (float)atof(&cmd[3]);
        if (VCS_SetTargetVoltage(ch, value) == HAL_OK) {
            snprintf(response, sizeof(response),
                     "[OK] CH%d target = %.3fV\r\n", ch, value);
            UART_Print(response);
        }
        break;

    case 'P': case 'p':  /* Kp 설정 */
        if (strlen(cmd) < 4 || cmd[2] != '=') {
            UART_Print("[ERR] Format: Pn=V.VV\r\n");
            return;
        }
        value = (float)atof(&cmd[3]);
        if (VCS_SetPIDGains(ch, value,
                             g_vcs.channels[ch].pid.ki,
                             g_vcs.channels[ch].pid.kd) == HAL_OK) {
            snprintf(response, sizeof(response),
                     "[OK] CH%d Kp = %.3f\r\n", ch, value);
            UART_Print(response);
        }
        break;

    case 'I': case 'i':  /* Ki 설정 */
        if (strlen(cmd) < 4 || cmd[2] != '=') {
            UART_Print("[ERR] Format: In=V.VV\r\n");
            return;
        }
        value = (float)atof(&cmd[3]);
        if (VCS_SetPIDGains(ch,
                             g_vcs.channels[ch].pid.kp,
                             value,
                             g_vcs.channels[ch].pid.kd) == HAL_OK) {
            snprintf(response, sizeof(response),
                     "[OK] CH%d Ki = %.3f\r\n", ch, value);
            UART_Print(response);
        }
        break;

    case 'D': case 'd':  /* Kd 설정 */
        if (strlen(cmd) < 4 || cmd[2] != '=') {
            UART_Print("[ERR] Format: Dn=V.VV\r\n");
            return;
        }
        value = (float)atof(&cmd[3]);
        if (VCS_SetPIDGains(ch,
                             g_vcs.channels[ch].pid.kp,
                             g_vcs.channels[ch].pid.ki,
                             value) == HAL_OK) {
            snprintf(response, sizeof(response),
                     "[OK] CH%d Kd = %.3f\r\n", ch, value);
            UART_Print(response);
        }
        break;

    case 'E': case 'e':  /* 채널 활성화 */
        if (VCS_EnableChannel(ch, true) == HAL_OK) {
            snprintf(response, sizeof(response),
                     "[OK] CH%d enabled\r\n", ch);
            UART_Print(response);
        }
        break;

    case 'X': case 'x':  /* 채널 비활성화 */
        if (VCS_EnableChannel(ch, false) == HAL_OK) {
            snprintf(response, sizeof(response),
                     "[OK] CH%d disabled\r\n", ch);
            UART_Print(response);
        }
        break;

    default:
        UART_Print("[ERR] Unknown command\r\n");
        break;
    }
}

/**
 * @brief  전체 채널 상태 출력
 */
static void VCS_PrintStatus(void)
{
    snprintf(s_uart_tx_buf, sizeof(s_uart_tx_buf),
             "\r\n--- Status (loop=%lu, err=%lu) ---\r\n",
             g_vcs.loop_count, g_vcs.error_count);
    UART_Print(s_uart_tx_buf);

    UART_Print("CH | State   | Target(V) | Actual(V) |  Error(V) | DAC(V)\r\n");
    UART_Print("---+---------+-----------+-----------+-----------+--------\r\n");

    for (int i = 0; i < NUM_CHANNELS; i++) {
        VCS_PrintChannelStatus((uint8_t)i);
    }

    UART_Print("\r\n");
}

/**
 * @brief  단일 채널 상태 출력
 */
static void VCS_PrintChannelStatus(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return;

    const VoltageChannel_t *c = &g_vcs.channels[ch];

    static const char *state_str[] = {
        "IDLE   ", "ACTIVE ", "SETTLED", "ERROR  "
    };

    const char *st = (c->state <= CH_STATE_ERROR)
                      ? state_str[c->state] : "UNKNOWN";

    snprintf(s_uart_tx_buf, sizeof(s_uart_tx_buf),
             "%2d | %s | %9.3f | %9.3f | %+9.3f | %6.3f\r\n",
             ch, st,
             c->target_voltage,
             c->actual_voltage,
             c->error,
             c->dac_output);

    UART_Print(s_uart_tx_buf);
}

/**
 * @brief  UART 문자열 전송
 */
static void UART_Print(const char *str)
{
    if (str == NULL) return;
    HAL_UART_Transmit(&huart2, (uint8_t *)str, (uint16_t)strlen(str),
                       UART_TX_TIMEOUT);
}
