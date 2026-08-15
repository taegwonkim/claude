/**
 * app_config.h
 *
 * 애플리케이션 전역 상수: 프로토콜 파라미터, 버퍼/큐 크기, 핀 매핑.
 * (FreeRTOS를 쓰지 않는 변형 — firmware/ 의 동명 파일과 달리 태스크 우선순위/스택 정의가 없다.)
 * CubeMX가 생성한 main.h의 핀 정의(GPIO_Pin/GPIO_Port)와 이름이 겹치지 않도록 APP_ 접두어를
 * 사용한다. CubeMX .ioc에서 핀 라벨을 아래 이름(FROM_FPGA, EEP_NSS, ESP32_NRST, LED_RUN,
 * LED_WIFI 등)으로 지정해두면 main.h에 동일 이름 매크로가 자동 생성되어 이 파일과 자연히
 * 맞물린다 (firmware/docs/CubeMX_설정가이드.md 참고, FreeRTOS §9만 제외).
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------- */
/* 큐(고정 크기 배열) / 버퍼 크기 — RTOS 없이 app_main.c가 직접 관리 */
/* ----------------------------------------------------------------------- */
#define APP_MEAS_QUEUE_LEN          (8U)   /* FpgaLink_Poll -> Esp32_Poll 측정값 큐 용량 */

#define APP_UART_RB_SIZE            (512U) /* USART1/USART3 RX 링버퍼 크기(byte) */
#define APP_PC_LINE_MAX             (128U) /* PC 커맨드 한 줄 최대 길이 */
#define APP_AT_LINE_MAX             (256U) /* ESP32 AT 응답 한 줄 최대 길이 */
#define APP_AT_RESP_TIMEOUT_MS      (5000U)
#define APP_AT_CIPSTART_TIMEOUT_MS  (10000U)
#define APP_AT_CWJAP_TIMEOUT_MS     (20000U)

/* ----------------------------------------------------------------------- */
/* FPGA 트리거 / ADC 프레임 */
/* ----------------------------------------------------------------------- */
#define FPGA_ADC_SAMPLE_COUNT       (6U)   /* 트리거 1회당 수신할 16bit 샘플 개수 (data1..data6) */

/* MCU -> FPGA 측정 개시 커맨드(USART2 TX, 부팅 후 1회 전송): STX + "START" + CR + LF */
#define FPGA_START_CMD              "\x02\"START\"\r\n"
#define FPGA_START_CMD_TX_TIMEOUT_MS (100U)

/* ----------------------------------------------------------------------- */
/* 네트워크 기본값 */
/* ----------------------------------------------------------------------- */
#define APP_DEFAULT_SERVER_PORT     (50001U)

/* ----------------------------------------------------------------------- */
/* PC로 보내는 ESP32 상태(STATUS,<num>) 주기적 브로드캐스트 간격 */
/* ----------------------------------------------------------------------- */
#define APP_ESP32_STATUS_BROADCAST_MS (2000U)

/* WiFi/TCP 끊김 감지 후 재접속 재시도 최소 간격 */
#define APP_ESP32_RECONNECT_RETRY_MS (5000U)

/* ----------------------------------------------------------------------- */
/* W25Q40 플래시 상의 설정 저장 위치 (docs/프로토콜_명세.md §5) */
/* ----------------------------------------------------------------------- */
#define APP_FLASH_CONFIG_SECTOR_ADDR (0x000000U)
#define APP_FLASH_SECTOR_SIZE        (4096U)

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
