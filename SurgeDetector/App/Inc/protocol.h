/**
 * protocol.h — SurgeDetector 공용 통신 프레임 정의
 * README.md 4장 참조.
 */
#ifndef APP_INC_PROTOCOL_H_
#define APP_INC_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- PC <-> MCU 설정 프로토콜 (USART3 / USB CDC 공용) --------- */

#define PC_FRAME_STX            0x02u
#define PC_FRAME_ETX            0x03u
#define PC_FRAME_MAX_PAYLOAD    250u
/* STX + LEN + CMD + PAYLOAD(max) + CRC16(2) + ETX */
#define PC_FRAME_MAX_SIZE       (1u + 1u + 1u + PC_FRAME_MAX_PAYLOAD + 2u + 1u)

typedef enum
{
    CMD_WRITE_WIFI_CFG   = 0x10,
    CMD_READ_WIFI_CFG    = 0x11,
    CMD_WRITE_SERVER_CFG = 0x12,
    CMD_READ_SERVER_CFG  = 0x13,
    CMD_WRITE_NET_CFG    = 0x14,
    CMD_READ_NET_CFG     = 0x15,
    CMD_READ_STATUS      = 0x20,
    CMD_DATA_PUSH         = 0x30,
    CMD_ACK               = 0x7E,
    CMD_NACK              = 0x7F,
} PcCmdId_t;

typedef enum
{
    ERR_NONE       = 0x00,
    ERR_BAD_CRC    = 0x01,
    ERR_BAD_LEN    = 0x02,
    ERR_UNKNOWN_CMD= 0x03,
    ERR_FLASH_FAIL = 0x04,
} PcErrCode_t;

/* 파싱된(디코딩 완료) PC 프레임 — 송/수신 공용 */
typedef struct
{
    uint8_t  cmd;
    uint8_t  len;                          /* payload 길이 */
    uint8_t  payload[PC_FRAME_MAX_PAYLOAD];
} PcFrame_t;

/* ---------------- FPGA <-> MCU 데이터 프레임 (USART2) ---------------------- */

#define FPGA_FRAME_SYNC0   0xAAu
#define FPGA_FRAME_SYNC1   0x55u
#define FPGA_FRAME_TAIL0   0x0Du
#define FPGA_FRAME_TAIL1   0x0Au
#define FPGA_ADC_CHANNELS  6u
/* SYNC(2) + SEQ(2) + CH*2(12) + CRC16(2) + TAIL(2) */
#define FPGA_FRAME_SIZE    (2u + 2u + (FPGA_ADC_CHANNELS * 2u) + 2u + 2u)

/* 파싱된 서지 측정 데이터 — 태스크 간 큐로 전달되는 형태 */
typedef struct
{
    uint32_t timestamp;                  /* MCU tick(ms) 캡처 시각 */
    uint16_t seq;                        /* FPGA 프레임 시퀀스 번호 */
    uint16_t channel[FPGA_ADC_CHANNELS]; /* ADC raw 값(6채널) */
} SurgeData_t;

/* WiFi 태스크에 재접속 등을 지시하는 내부 명령 큐 메시지 */
typedef enum
{
    WIFI_CMD_RECONFIGURE = 0,  /* 설정이 바뀌었으니 재접속 */
    WIFI_CMD_FORCE_RECONNECT,  /* 워치독이 끊김을 감지 */
} WifiCmdId_t;

typedef struct
{
    WifiCmdId_t id;
} WifiCmdMsg_t;

/* ---------------- 공용 유틸 ------------------------------------------------ */

uint16_t Protocol_CRC16(const uint8_t *data, uint32_t len);

/* frame 버퍼(raw bytes) -> PcFrame_t 디코딩. 성공 시 true, 실패 시 err_code에 사유 기록 */
bool Protocol_DecodePcFrame(const uint8_t *raw, uint32_t rawLen, PcFrame_t *out, PcErrCode_t *errCode);

/* PcFrame_t -> raw bytes 인코딩. 반환값: 생성된 바이트 수(0이면 실패) */
uint32_t Protocol_EncodePcFrame(const PcFrame_t *frame, uint8_t *out, uint32_t outCap);

#ifdef __cplusplus
}
#endif

#endif /* APP_INC_PROTOCOL_H_ */
