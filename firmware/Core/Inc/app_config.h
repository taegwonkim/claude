/**
 * app_config.h
 *
 * 애플리케이션 전역 상수: FreeRTOS 태스크 우선순위/스택/큐 크기,
 * 프로토콜 파라미터, 핀 매핑. CubeMX가 생성한 main.h의 핀 정의(GPIO_Pin/GPIO_Port)와
 * 이름이 겹치지 않도록 APP_ 접두어를 사용한다. CubeMX .ioc에서 핀 라벨을
 * 아래 이름(FROM_FPGA, EEP_NSS, ESP32_NRST, LED_RUN, LED_WIFI 등)으로 지정해두면 main.h에 동일 이름 매크로가
 * 자동 생성되어 이 파일과 자연히 맞물린다 (docs/CubeMX_설정가이드.md 참고).
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "cmsis_os2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------- */
/* FreeRTOS 태스크 우선순위 / 스택 크기 (word 단위, CMSIS-RTOS2 osThreadNew 기준) */
/* ----------------------------------------------------------------------- */
#define APP_PRIO_FPGA_TASK      osPriorityRealtime
#define APP_PRIO_ESP32_TASK     osPriorityAboveNormal
#define APP_PRIO_PCCOMM_TASK    osPriorityNormal
#define APP_PRIO_CONFIG_TASK    osPriorityBelowNormal

#define APP_STACK_FPGA_TASK     (512U)
#define APP_STACK_ESP32_TASK    (1024U)
#define APP_STACK_PCCOMM_TASK   (768U)
#define APP_STACK_CONFIG_TASK   (512U)

/* ----------------------------------------------------------------------- */
/* 큐 / 버퍼 크기 */
/* ----------------------------------------------------------------------- */
#define APP_MEAS_QUEUE_LEN          (8U)   /* FPGA_Task -> ESP32_Task/PCComm_Task */
#define APP_CFG_EVENT_QUEUE_LEN     (4U)   /* PCComm_Task -> Config_Task */
#define APP_WIFI_EVENT_QUEUE_LEN    (4U)   /* Config_Task -> ESP32_Task */

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

/* ----------------------------------------------------------------------- */
/* W25Q40 플래시 상의 설정 저장 위치 (docs/프로토콜_명세.md §5) */
/* ----------------------------------------------------------------------- */
#define APP_FLASH_CONFIG_SECTOR_ADDR (0x000000U)
#define APP_FLASH_SECTOR_SIZE        (4096U)

/* ----------------------------------------------------------------------- */
/* RTC Wakeup Timer 기반 주기적 리셋 (docs/프로토콜_명세.md §6) */
/* ----------------------------------------------------------------------- */
/* RESET_W_ALL로 저장하는 설정의 플래시 위치: NetConfig(섹터0) 다음 섹터 */
#define APP_FLASH_RESET_CONFIG_SECTOR_ADDR (0x001000U)

/* RTC Wakeup Timer는 ck_spre(1Hz) + 16bit 모드 기준 최대 65536초까지 표현 가능
 * (WUTR 레지스터가 15bit라 (WUTR+1)초, WUTR 최대값 0xFFFF -> 65536초 ≈ 18.2시간).
 * 더 긴 주기가 필요하면 17bit 모드(RTC_WAKEUPCLOCK_CK_SPRE_17BITS, 최대 131072초)로
 * 확장 가능하나 본 구현 범위에서는 16bit 모드만 지원한다. */
#define APP_RESET_MIN_PERIOD_SEC     (1U)
#define APP_RESET_MAX_PERIOD_SEC     (65536U)
#define APP_RESET_DEFAULT_PERIOD_SEC (3600U) /* 기본 1시간, RESET_W_ALL로 변경 가능 */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
