#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include "main.h"
#include "voltage_control.h"
#include <stdint.h>
#include <stdbool.h>

/* UART 설정 */
extern UART_HandleTypeDef huart1;
#define PROTO_UART          &huart1
#define PROTO_RX_BUF_SIZE   256    /* DMA 원형 버퍼 크기 */
#define PROTO_TX_BUF_SIZE   512
#define PROTO_CMD_MAX_LEN   64

/* TX 큐 (Non-RTOS: 단순 링 버퍼) */
#define TX_QUEUE_DEPTH      16
#define TX_ITEM_SIZE        PROTO_TX_BUF_SIZE

/* 명령 타입 */
typedef enum {
    CMD_NONE=0, CMD_SET, CMD_GET, CMD_STATUS,
    CMD_ENABLE, CMD_RESET, CMD_HELP, CMD_VERSION, CMD_UNKNOWN
} CmdType_t;

/* 에러 코드 */
typedef enum {
    ERR_NONE=0, ERR_SYNTAX, ERR_PARAM, ERR_CH_INVALID, ERR_CH_FAULT
} ErrCode_t;

/* 파싱 결과 */
typedef struct {
    CmdType_t type;
    uint8_t   channel;
    int32_t   value;
    bool      all_channels;
} ParsedCmd_t;

/* TX 링버퍼 */
typedef struct {
    char     buf[TX_QUEUE_DEPTH][TX_ITEM_SIZE];
    uint8_t  head;
    uint8_t  tail;
    uint8_t  count;
    bool     tx_busy;      /* DMA TX 진행 중 */
} TxQueue_t;

extern TxQueue_t g_txq;
extern uint8_t   g_uart_rx_dma_buf[PROTO_RX_BUF_SIZE];
extern uint16_t  g_uart_rx_prev_pos;

/* 펌웨어 버전 */
#define FW_VERSION  "1.0.0 (NoRTOS)"

void  Proto_Init(void);
bool  Proto_ParseCmd(const char *raw, ParsedCmd_t *out);
void  Proto_Send(const char *str);
void  Proto_Sendf(const char *fmt, ...);
void  Proto_SendOKf(const char *fmt, ...);
void  Proto_SendError(ErrCode_t err, const char *msg);
void  Proto_SendEvent(const char *type, const char *data);
void  Proto_SendStatus(VoltageCtrl_t *hctrl);
void  Proto_SendChannelInfo(VoltageCtrl_t *hctrl, uint8_t ch);
void  Proto_SendHelp(void);
void  Proto_SendVersion(void);
/* TX 큐 → DMA 전송 구동 (메인 루프에서 호출) */
void  Proto_TxPump(void);
/* DMA TX 완료 콜백에서 호출 */
void  Proto_TxDoneCallback(void);
/* UART RX 처리 (메인 루프에서 플래그 감지 후 호출) */
void  Proto_RxProcess(uint16_t new_pos);

#endif /* UART_PROTOCOL_H */
