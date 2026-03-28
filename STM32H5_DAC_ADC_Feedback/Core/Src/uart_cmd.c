/**
 * @file    uart_cmd.c
 * @brief   PC ↔ STM32H5 UART 명령 파서 및 응답 송신 구현
 *
 * 명령 파싱은 모두 이 파일에서 처리하므로, freertos_tasks.c는
 * 수신 버퍼 관리와 태스크 스케줄링에만 집중한다.
 */

#include "uart_cmd.h"
#include "voltage_ctrl.h"
#include "pid.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ======================================================================== */
/*  전역 상태                                                                 */
/* ======================================================================== */

volatile bool g_stream_enabled = false; /**< STREAM ON/OFF */

/* ======================================================================== */
/*  내부 송신 버퍼 및 헬퍼                                                   */
/* ======================================================================== */

static char s_tx[UCMD_TX_BUF_SIZE];

/**
 * @brief  UART3 폴링 송신 (FreeRTOS 태스크 컨텍스트 전용)
 */
static void tx(const char *str)
{
    uint16_t len = (uint16_t)strlen(str);
    HAL_UART_Transmit(&huart3, (const uint8_t *)str, len, 200);
}

/**
 * @brief  문자열에서 앞뒤 공백 및 \r\n 제거 (in-place)
 */
static void trim(char *s)
{
    /* 뒤에서 제거 */
    int len = (int)strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' ||
                       s[len - 1] == ' '  || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
    /* 앞에서 제거 */
    char *p = s;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

/**
 * @brief  문자열을 대문자로 변환 (in-place, 최대 n바이트)
 */
static void to_upper(char *s, size_t n)
{
    for (size_t i = 0; i < n && s[i]; i++) {
        s[i] = (char)toupper((unsigned char)s[i]);
    }
}

/* ======================================================================== */
/*  개별 명령 처리 함수                                                       */
/* ======================================================================== */

/** SET <ch> <volt> */
static UCmdResult_t cmd_set(const char *args)
{
    unsigned int ch;
    double volt;

    if (sscanf(args, "%u %lf", &ch, &volt) != 2) {
        return UCMD_ERR_PARAM;
    }
    if (ch >= VCTRL_NUM_CHANNELS) {
        return UCMD_ERR_CH_RANGE;
    }
    if (volt < 0.0 || volt > 3.3) {
        return UCMD_ERR_PARAM;
    }

    VCtrl_SetTarget((uint8_t)ch, (float)volt);

    snprintf(s_tx, sizeof(s_tx),
             "OK: SET %u %.3fV\r\n", ch, volt);
    tx(s_tx);

    /* 즉시 채널 상태 응답 */
    UCmd_PrintChannelStatus((uint8_t)ch);
    return UCMD_OK;
}

/** GET <ch | ALL> */
static UCmdResult_t cmd_get(const char *args)
{
    char token[8];
    if (sscanf(args, "%7s", token) != 1) {
        return UCMD_ERR_PARAM;
    }
    to_upper(token, sizeof(token));

    if (strcmp(token, "ALL") == 0) {
        for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
            UCmd_PrintChannelStatus(i);
        }
        return UCMD_OK;
    }

    char *end;
    unsigned long ch = strtoul(token, &end, 10);
    if (*end != '\0' || ch >= VCTRL_NUM_CHANNELS) {
        return UCMD_ERR_CH_RANGE;
    }
    UCmd_PrintChannelStatus((uint8_t)ch);
    return UCMD_OK;
}

/** PID <ch> <Kp> <Ki> <Kd> */
static UCmdResult_t cmd_pid(const char *args)
{
    unsigned int ch;
    double kp, ki, kd;

    if (sscanf(args, "%u %lf %lf %lf", &ch, &kp, &ki, &kd) != 4) {
        return UCMD_ERR_PARAM;
    }
    if (ch >= VCTRL_NUM_CHANNELS) {
        return UCMD_ERR_CH_RANGE;
    }
    if (kp < 0.0 || ki < 0.0 || kd < 0.0) {
        return UCMD_ERR_PARAM;
    }

    /* voltage_ctrl 내부 채널 구조체에서 PID 파라미터 직접 갱신 */
    VCtrl_Channel_t info;
    VCtrl_GetChannelInfo((uint8_t)ch, &info);

    /* 뮤텍스 없이 직접 접근 — PID 파라미터는 float 단순 쓰기이므로 atomic */
    info.pid.Kp = (float)kp;
    info.pid.Ki = (float)ki;
    info.pid.Kd = (float)kd;
    PID_Reset(&info.pid);

    /*
     * NOTE: VCtrl_GetChannelInfo()는 복사본을 반환하므로,
     *       실제 반영을 위해 voltage_ctrl.c에 VCtrl_SetPID() API 추가 권장.
     *       여기서는 외부에서 직접 접근하는 방법 대신
     *       voltage_ctrl.h에 공개 API를 추가하는 설계를 보여준다.
     *       아래는 해당 API 호출 시뮬레이션이다.
     */
    VCtrl_SetPID((uint8_t)ch, (float)kp, (float)ki, (float)kd);

    snprintf(s_tx, sizeof(s_tx),
             "OK: PID %u Kp=%.2f Ki=%.2f Kd=%.2f\r\n",
             ch, kp, ki, kd);
    tx(s_tx);
    return UCMD_OK;
}

/** ENABLE / DISABLE <ch | ALL> */
static UCmdResult_t cmd_enable(const char *args, bool enable)
{
    char token[8];
    if (sscanf(args, "%7s", token) != 1) {
        return UCMD_ERR_PARAM;
    }
    to_upper(token, sizeof(token));

    if (strcmp(token, "ALL") == 0) {
        for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
            VCtrl_SetEnabled(i, enable);
        }
        snprintf(s_tx, sizeof(s_tx),
                 "OK: %s ALL\r\n", enable ? "ENABLE" : "DISABLE");
        tx(s_tx);
        return UCMD_OK;
    }

    char *end;
    unsigned long ch = strtoul(token, &end, 10);
    if (*end != '\0' || ch >= VCTRL_NUM_CHANNELS) {
        return UCMD_ERR_CH_RANGE;
    }
    VCtrl_SetEnabled((uint8_t)ch, enable);
    snprintf(s_tx, sizeof(s_tx),
             "OK: %s %lu\r\n", enable ? "ENABLE" : "DISABLE", ch);
    tx(s_tx);
    return UCMD_OK;
}

/** STREAM ON / OFF */
static UCmdResult_t cmd_stream(const char *args)
{
    char token[8];
    if (sscanf(args, "%7s", token) != 1) {
        return UCMD_ERR_PARAM;
    }
    to_upper(token, sizeof(token));

    if (strcmp(token, "ON") == 0) {
        g_stream_enabled = true;
        tx("OK: STREAM ON\r\n");
        return UCMD_OK;
    }
    if (strcmp(token, "OFF") == 0) {
        g_stream_enabled = false;
        tx("OK: STREAM OFF\r\n");
        return UCMD_OK;
    }
    return UCMD_ERR_PARAM;
}

/* ======================================================================== */
/*  공개 API 구현                                                             */
/* ======================================================================== */

void UCmd_Process(const char *line)
{
    /* 작업용 복사본 (원본 보존) */
    char buf[UCMD_LINE_MAX];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim(buf);

    if (buf[0] == '\0') return; /* 빈 줄 무시 */

    /* 명령어 키워드 추출 (최대 12자) */
    char cmd[13] = {0};
    const char *args = "";
    char *sp = strchr(buf, ' ');
    if (sp != NULL) {
        size_t cmd_len = (size_t)(sp - buf);
        if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
        memcpy(cmd, buf, cmd_len);
        args = sp + 1;
        while (*args == ' ') args++; /* 인수 앞 공백 제거 */
    } else {
        strncpy(cmd, buf, sizeof(cmd) - 1);
    }
    to_upper(cmd, sizeof(cmd));

    /* ---- 명령 분기 ---- */
    UCmdResult_t result = UCMD_OK;

    if (strcmp(cmd, "SET") == 0) {
        result = cmd_set(args);
    } else if (strcmp(cmd, "GET") == 0) {
        result = cmd_get(args);
    } else if (strcmp(cmd, "PID") == 0) {
        result = cmd_pid(args);
    } else if (strcmp(cmd, "ENABLE") == 0) {
        result = cmd_enable(args, true);
    } else if (strcmp(cmd, "DISABLE") == 0) {
        result = cmd_enable(args, false);
    } else if (strcmp(cmd, "STREAM") == 0) {
        result = cmd_stream(args);
    } else if (strcmp(cmd, "HELP") == 0) {
        UCmd_PrintHelp();
    } else {
        result = UCMD_ERR_UNKNOWN;
    }

    /* 오류 응답 */
    if (result == UCMD_ERR_UNKNOWN) {
        snprintf(s_tx, sizeof(s_tx), "ERR: Unknown command '%s'\r\n", cmd);
        tx(s_tx);
    } else if (result == UCMD_ERR_PARAM) {
        snprintf(s_tx, sizeof(s_tx), "ERR: Invalid parameter for '%s'\r\n", cmd);
        tx(s_tx);
    } else if (result == UCMD_ERR_CH_RANGE) {
        snprintf(s_tx, sizeof(s_tx),
                 "ERR: Channel out of range (0~%u)\r\n",
                 (unsigned)(VCTRL_NUM_CHANNELS - 1));
        tx(s_tx);
    }
}

void UCmd_PrintChannelStatus(uint8_t ch_idx)
{
    if (ch_idx >= VCTRL_NUM_CHANNELS) return;

    VCtrl_Channel_t info;

    if (xSemaphoreTake(xCtrlMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        VCtrl_GetChannelInfo(ch_idx, &info);
        xSemaphoreGive(xCtrlMutex);
    } else {
        tx("ERR: Mutex timeout\r\n");
        return;
    }

    snprintf(s_tx, sizeof(s_tx),
             "#CH%u,SET=%.3f,FB=%.3f,ERR=%+.3f,OUT=%4u,TYPE=%s\r\n",
             ch_idx,
             (double)info.setpoint_v,
             (double)info.feedback_v,
             (double)info.error_v,
             (unsigned)(uint32_t)info.output_raw,
             (info.out_type == VCTRL_OUT_DAC) ? "DAC" : "PWM");
    tx(s_tx);
}

void UCmd_PrintStream(void)
{
    VCtrl_Channel_t info[VCTRL_NUM_CHANNELS];

    if (xSemaphoreTake(xCtrlMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (uint8_t i = 0; i < VCTRL_NUM_CHANNELS; i++) {
            VCtrl_GetChannelInfo(i, &info[i]);
        }
        xSemaphoreGive(xCtrlMutex);
    } else {
        return;
    }

    /*
     * 스트림 형식:
     *   @<tick_ms>,<fb0>,<fb1>,<fb2>,<fb3>,<out0>,<out1>,<out2>,<out3>
     *
     * 예) @12345,1.000,1.500,2.001,2.499,1241,1861,2483,3101
     *
     * 이 형식은 Python/Matlab 등에서 쉽게 파싱 가능하다.
     */
    uint32_t tick = HAL_GetTick();

    snprintf(s_tx, sizeof(s_tx),
             "@%lu,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%u\r\n",
             (unsigned long)tick,
             (double)info[0].feedback_v,
             (double)info[1].feedback_v,
             (double)info[2].feedback_v,
             (double)info[3].feedback_v,
             (unsigned)(uint32_t)info[0].output_raw,
             (unsigned)(uint32_t)info[1].output_raw,
             (unsigned)(uint32_t)info[2].output_raw,
             (unsigned)(uint32_t)info[3].output_raw);
    tx(s_tx);
}

void UCmd_PrintHelp(void)
{
    tx("\r\n");
    tx("=== STM32H5 Voltage Controller Commands ===\r\n");
    tx("  SET <ch> <volt>          Set channel target voltage (0.0~3.3V)\r\n");
    tx("    ex) SET 0 2.500\r\n");
    tx("  GET <ch|ALL>             Query channel status\r\n");
    tx("    ex) GET 2  /  GET ALL\r\n");
    tx("  PID <ch> <Kp> <Ki> <Kd> Update PID gains\r\n");
    tx("    ex) PID 0 200.0 50.0 5.0\r\n");
    tx("  ENABLE  <ch|ALL>         Enable channel PID control\r\n");
    tx("  DISABLE <ch|ALL>         Disable channel (output=0)\r\n");
    tx("  STREAM ON                Start periodic stream output\r\n");
    tx("  STREAM OFF               Stop  periodic stream output\r\n");
    tx("  HELP                     Show this message\r\n");
    tx("\r\n");
    tx("Response format:\r\n");
    tx("  OK: <cmd>                Command accepted\r\n");
    tx("  ERR: <reason>            Command failed\r\n");
    tx("  #CH<n>,SET=V,FB=V,ERR=V,OUT=raw,TYPE=DAC|PWM\r\n");
    tx("  @<tick_ms>,fb0,fb1,fb2,fb3,out0,out1,out2,out3  (stream)\r\n");
    tx("===========================================\r\n");
}
