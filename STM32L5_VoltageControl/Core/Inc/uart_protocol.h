#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include "main.h"
#include "voltage_control.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * UART 프로토콜 모듈
 *
 * 명령어 형식 (ASCII):
 *   SET <ch> <voltage_mv>\r\n   → 채널 목표 전압 설정 (ch: 1~4)
 *   GET <ch>\r\n                → 채널 전압/전류 조회
 *   STATUS\r\n                  → 전체 채널 상태 출력
 *   ENABLE <ch> <0|1>\r\n       → 채널 활성화/비활성화
 *   RESET <ch>\r\n              → 채널 폴트 리셋
 *   RESET ALL\r\n               → 전체 리셋
 *   HELP\r\n                    → 도움말 출력
 *   VERSION\r\n                 → 펌웨어 버전 출력
 *
 * 응답 형식:
 *   OK <data>\r\n               → 성공
 *   ERR <code> <msg>\r\n        → 오류
 *   EVT <type> <data>\r\n       → 이벤트 알림 (폴트 등)
 * =========================================================*/

/* ----- UART 설정 ----- */
extern UART_HandleTypeDef huart1;
#define PROTO_UART             &huart1
#define PROTO_BAUDRATE         115200
#define PROTO_RX_BUF_SIZE      256
#define PROTO_TX_BUF_SIZE      512
#define PROTO_CMD_MAX_LEN      64

/* ----- 명령어 코드 ----- */
typedef enum {
    CMD_NONE    = 0,
    CMD_SET,         /* SET <ch> <mv>  */
    CMD_GET,         /* GET <ch>       */
    CMD_STATUS,      /* STATUS         */
    CMD_ENABLE,      /* ENABLE <ch> <0|1> */
    CMD_RESET,       /* RESET <ch|ALL> */
    CMD_HELP,        /* HELP           */
    CMD_VERSION,     /* VERSION        */
    CMD_UNKNOWN,
} CmdType_t;

/* ----- 에러 코드 ----- */
typedef enum {
    ERR_NONE        = 0,
    ERR_SYNTAX      = 1,    /* 명령 문법 오류 */
    ERR_PARAM       = 2,    /* 파라미터 범위 오류 */
    ERR_CH_INVALID  = 3,    /* 잘못된 채널 번호 */
    ERR_CH_FAULT    = 4,    /* 채널 폴트 상태 */
    ERR_TIMEOUT     = 5,    /* 타임아웃 */
} ErrCode_t;

/* ----- 파싱된 명령 구조체 ----- */
typedef struct {
    CmdType_t type;
    uint8_t   channel;     /* 채널 번호 (0-based: 0~3) */
    int32_t   value;       /* SET 명령의 전압값 (mV) 또는 ENABLE의 0/1 */
    bool      all_channels; /* RESET ALL 등 전체 채널 적용 여부 */
} ParsedCmd_t;

/* ----- 큐 아이템 구조체 ----- */
typedef struct {
    char data[PROTO_CMD_MAX_LEN];
    uint8_t len;
} CmdQueueItem_t;

typedef struct {
    char data[PROTO_TX_BUF_SIZE];
    uint16_t len;
} RespQueueItem_t;

/* ----- 펌웨어 정보 ----- */
#define FW_VERSION_MAJOR   1
#define FW_VERSION_MINOR   0
#define FW_VERSION_PATCH   0
#define FW_BUILD_DATE      __DATE__
#define FW_BUILD_TIME      __TIME__

/* ----- 함수 선언 ----- */
void      Proto_Init(void);
bool      Proto_ParseCmd(const char *raw, ParsedCmd_t *cmd);
void      Proto_SendOK(const char *msg);
void      Proto_SendOKf(const char *fmt, ...);
void      Proto_SendError(ErrCode_t err, const char *msg);
void      Proto_SendEvent(const char *type, const char *data);
void      Proto_SendStatus(VoltageCtrl_t *hctrl);
void      Proto_SendChannelInfo(VoltageCtrl_t *hctrl, uint8_t ch);
void      Proto_SendHelp(void);
void      Proto_SendVersion(void);
uint16_t  Proto_FormatResponse(char *buf, uint16_t maxlen,
                                VoltageCtrl_t *hctrl, uint8_t ch);

#endif /* UART_PROTOCOL_H */
