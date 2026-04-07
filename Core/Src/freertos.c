/**
 * @file    freertos.c
 * @brief   FreeRTOS 태스크 정의
 *
 * 태스크 구조:
 *
 *   ┌─────────────────────────────────────────────────────────┐
 *   │                                                         │
 *   │  [ISR] USART1 IDLE / DMA TC                             │
 *   │        │                                                │
 *   │        │ RxFrame_t (Queue)                              │
 *   │        ▼                                                │
 *   │  [CommRxTask] 프레임 수신 & 파싱                         │
 *   │        │                                                │
 *   │        │ Packet_t (Queue)                               │
 *   │        ▼                                                │
 *   │  [AppTask]  패킷 처리 & 응용 로직                        │
 *   │                                                         │
 *   └─────────────────────────────────────────────────────────┘
 *
 * CubeMX FreeRTOS 설정 (CMSIS-RTOS v2):
 *   - CommRxTask : Priority=osPriorityAboveNormal, Stack=512
 *   - AppTask    : Priority=osPriorityNormal,      Stack=512
 */

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "usart_dma.h"
#include "packet_parser.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────
 *  외부 선언 (CubeMX 생성 HAL 핸들)
 * ───────────────────────────────────────────────────────── */
extern UART_HandleTypeDef huart1;

/* ─────────────────────────────────────────────────────────
 *  태스크 핸들 (CubeMX에서 생성하거나 직접 생성)
 * ───────────────────────────────────────────────────────── */
osThreadId_t CommRxTaskHandle;
osThreadId_t AppTaskHandle;

const osThreadAttr_t CommRxTask_attributes = {
    .name       = "CommRxTask",
    .stack_size = 512 * 4,
    .priority   = (osPriority_t)osPriorityAboveNormal,
};

const osThreadAttr_t AppTask_attributes = {
    .name       = "AppTask",
    .stack_size = 512 * 4,
    .priority   = (osPriority_t)osPriorityNormal,
};

/* ─────────────────────────────────────────────────────────
 *  태스크 함수 전방 선언
 * ───────────────────────────────────────────────────────── */
static void CommRxTask_Entry(void *argument);
static void AppTask_Entry(void *argument);

/* ─────────────────────────────────────────────────────────
 *  태스크 생성 (MX_FREERTOS_Init 에서 호출)
 *  CubeMX가 이 함수를 freertos.c에 자동 생성합니다.
 *  수동 생성 시 main.c의 osKernelStart() 전에 호출하십시오.
 * ───────────────────────────────────────────────────────── */
void MX_FREERTOS_Init(void)
{
    /* 1. USART DMA 수신 초기화 (Queue 포함) */
    USART_DMA_Init(&huart1);

    /* 2. 태스크 생성 */
    CommRxTaskHandle = osThreadNew(CommRxTask_Entry, NULL, &CommRxTask_attributes);
    AppTaskHandle    = osThreadNew(AppTask_Entry,    NULL, &AppTask_attributes);

    if (CommRxTaskHandle == NULL || AppTaskHandle == NULL) {
        Error_Handler();
    }
}

/* ═════════════════════════════════════════════════════════
 *  CommRxTask: 프레임 수신 → STX/ETX 파싱
 *
 *  우선순위를 AppTask보다 높게 설정하여 수신 프레임을
 *  빠르게 처리하고 Queue overflow를 방지합니다.
 * ═════════════════════════════════════════════════════════ */
static void CommRxTask_Entry(void *argument)
{
    (void)argument;

    /* 파서 컨텍스트 초기화
     * out_queue = NULL → 내부에서 s_packet_queue 자동 생성 */
    ParserCtx_t parser;
    parser_init(&parser, NULL);

    osMessageQueueId_t frame_q  = USART_DMA_GetFrameQueue();
    RxFrame_t          frame;

    for (;;) {
        /* 프레임이 도착할 때까지 대기 (무기한) */
        if (osMessageQueueGet(frame_q, &frame, NULL, osWaitForever) == osOK) {
            /* 원시 바이트를 파서에 공급 */
            parser_feed(&parser, frame.buf, frame.len);
        }
    }
}

/* ═════════════════════════════════════════════════════════
 *  AppTask: 완성된 패킷 처리 (애플리케이션 로직)
 *
 *  이 태스크를 사용자 요구에 맞게 수정하십시오.
 *  예: 명령어 파싱, 센서 제어, 데이터 저장 등
 * ═════════════════════════════════════════════════════════ */
static void AppTask_Entry(void *argument)
{
    (void)argument;

    /* CommRxTask가 먼저 Queue를 생성하도록 잠시 양보 */
    osDelay(10);

    osMessageQueueId_t pkt_q = parser_get_packet_queue();
    Packet_t           pkt;

    for (;;) {
        if (osMessageQueueGet(pkt_q, &pkt, NULL, osWaitForever) == osOK) {
            /* ──────────────────────────────────────────────────
             *  패킷 처리 예시
             *
             *  pkt.data : 수신된 데이터 (STX, ETX 제외)
             *  pkt.len  : 데이터 바이트 수
             * ────────────────────────────────────────────────── */
            app_process_packet(&pkt);
        }
    }
}

/* ═════════════════════════════════════════════════════════
 *  app_process_packet: 사용자 정의 패킷 처리 함수
 *
 *  프로토콜 예시:
 *    [CMD(1)] [LENGTH(1)] [PAYLOAD(N)]
 *
 *    CMD 0x01 → LED 제어: PAYLOAD[0] = 0:OFF, 1:ON
 *    CMD 0x02 → ECHO:     수신 데이터를 그대로 UART 송신
 * ═════════════════════════════════════════════════════════ */
void app_process_packet(const Packet_t *pkt)
{
    if (pkt == NULL || pkt->len == 0U) {
        return;
    }

    uint8_t cmd = pkt->data[0];

    switch (cmd) {

    case 0x01: /* LED 제어 */
        if (pkt->len >= 2U) {
            if (pkt->data[1] == 0x01U) {
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
            } else {
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
            }
        }
        break;

    case 0x02: /* ECHO: STX + data + ETX 그대로 응답 */
    {
        uint8_t echo_buf[MAX_PACKET_DATA_LEN + 2U];
        echo_buf[0] = STX;
        (void)memcpy(&echo_buf[1], pkt->data, pkt->len);
        echo_buf[pkt->len + 1U] = ETX;

        /* Non-blocking 송신 (DMA 또는 Interrupt 방식 권장) */
        HAL_UART_Transmit(&huart1, echo_buf, (uint16_t)(pkt->len + 2U), 100U);
        break;
    }

    default:
        /* 알 수 없는 CMD → 무시 또는 NACK 전송 */
        break;
    }
}
