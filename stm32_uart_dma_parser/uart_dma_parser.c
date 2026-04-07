/**
 ******************************************************************************
 * @file    uart_dma_parser.c
 * @brief   STM32L552 + FreeRTOS USART1 DMA(Normal) + IDLE Line 수신/파서 구현
 *
 *  동작 개요
 *  ----------
 *   1) HAL_UART_Receive_DMA() 로 DMA Normal 모드 수신 시작
 *   2) USART1 IDLE 인터럽트를 수동으로 Enable
 *   3) 수신 중 라인이 일정 시간(1 byte time) 이상 IDLE 이 되면
 *      - DMA 중단 -> 지금까지 수신된 바이트 수 계산
 *      - raw 바이트를 링버퍼에 push 하고 Parser Task 에 알림
 *      - DMA 재시작 (Normal Mode 이므로 매번 재시작 해야 함)
 *   4) Parser Task 는 링버퍼에서 바이트를 꺼내 STX/ETX 상태머신으로
 *      프레임을 조립 -> Queue 로 전달 -> UART_OnFrameReceived() 호출
 ******************************************************************************
 */
#include "uart_dma_parser.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "stream_buffer.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  내부 버퍼 / 동기화 객체                                              */
/* ------------------------------------------------------------------ */
static uint8_t  s_dmaRxBuf[UART_RX_DMA_BUF_SIZE];
static StreamBufferHandle_t s_rxStream = NULL;    /* ISR -> Task 바이트 스트림 */

osMessageQueueId_t uartRxQueueHandle = NULL;      /* 완성 프레임 전달용 */

/* ------------------------------------------------------------------ */
/*  Weak 기본 콜백                                                      */
/* ------------------------------------------------------------------ */
__weak void UART_OnFrameReceived(const UartFrame_t *frame)
{
    (void)frame;
    /* 사용자 구현 */
}

/* ------------------------------------------------------------------ */
/*  수신 시작                                                          */
/* ------------------------------------------------------------------ */
void UART_DmaIdle_Start(void)
{
    /* 바이트 스트림 버퍼 (ISR -> Task) */
    s_rxStream = xStreamBufferCreate(UART_RX_DMA_BUF_SIZE * 2U, 1U);
    configASSERT(s_rxStream != NULL);

    /* 완성 프레임 메시지 큐 */
    const osMessageQueueAttr_t qAttr = { .name = "uartRxQ" };
    uartRxQueueHandle = osMessageQueueNew(UART_RX_QUEUE_LEN,
                                          sizeof(UartFrame_t),
                                          &qAttr);
    configASSERT(uartRxQueueHandle != NULL);

    /* IDLE 인터럽트 Enable (HAL 에는 기본 활성화 API 없음) */
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    /* DMA Normal 모드 수신 시작 */
    if (HAL_UART_Receive_DMA(&huart1, s_dmaRxBuf, UART_RX_DMA_BUF_SIZE) != HAL_OK) {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------ */
/*  USART1_IRQHandler 안에서 호출할 IDLE 처리 루틴                      */
/* ------------------------------------------------------------------ */
void UART_DmaIdle_IRQHandler(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) {
        return;
    }

    /* IDLE flag 확인 */
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) == RESET) {
        return;
    }
    __HAL_UART_CLEAR_IDLEFLAG(huart);

    /* 지금까지 DMA 로 수신된 바이트 수 계산
       DMA Normal 모드 : 남은 카운트 = NDTR                                */
    uint32_t remaining = __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
    uint32_t received  = UART_RX_DMA_BUF_SIZE - remaining;

    /* DMA 를 일단 중단해야 NDTR 과 버퍼 상태가 고정된다 */
    HAL_UART_DMAStop(huart);

    if (received > 0U && s_rxStream != NULL) {
        BaseType_t hpw = pdFALSE;
        (void)xStreamBufferSendFromISR(s_rxStream,
                                       s_dmaRxBuf,
                                       received,
                                       &hpw);
        portYIELD_FROM_ISR(hpw);
    }

    /* Normal 모드이므로 매번 재시작 */
    (void)HAL_UART_Receive_DMA(huart, s_dmaRxBuf, UART_RX_DMA_BUF_SIZE);
    __HAL_UART_CLEAR_IDLEFLAG(huart);
    __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);
}

/* ------------------------------------------------------------------ */
/*  DMA 버퍼가 IDLE 없이 꽉 찼을 때 (RxCpltCallback)                    */
/* ------------------------------------------------------------------ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) {
        return;
    }

    BaseType_t hpw = pdFALSE;
    (void)xStreamBufferSendFromISR(s_rxStream,
                                   s_dmaRxBuf,
                                   UART_RX_DMA_BUF_SIZE,
                                   &hpw);

    /* Normal 모드 재시작 */
    (void)HAL_UART_Receive_DMA(huart, s_dmaRxBuf, UART_RX_DMA_BUF_SIZE);
    __HAL_UART_CLEAR_IDLEFLAG(huart);
    __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);

    portYIELD_FROM_ISR(hpw);
}

/* ------------------------------------------------------------------ */
/*  파서 상태머신                                                      */
/* ------------------------------------------------------------------ */
typedef enum {
    PS_WAIT_STX = 0,
    PS_COLLECT
} ParseState_t;

static void Parser_Feed(uint8_t byte,
                        ParseState_t *state,
                        UartFrame_t  *frame)
{
    switch (*state) {
    case PS_WAIT_STX:
        if (byte == STX_BYTE) {
            frame->length = 0U;
            *state = PS_COLLECT;
        }
        break;

    case PS_COLLECT:
        if (byte == ETX_BYTE) {
            /* 한 프레임 완성 */
            (void)osMessageQueuePut(uartRxQueueHandle, frame, 0U, 0U);
            UART_OnFrameReceived(frame);
            *state = PS_WAIT_STX;
        } else if (byte == STX_BYTE) {
            /* 중간에 STX 재등장 -> 프레임 리셋 */
            frame->length = 0U;
        } else {
            if (frame->length < UART_RX_FRAME_MAX_SIZE) {
                frame->data[frame->length++] = byte;
            } else {
                /* 오버플로우 -> 버리고 재동기화 */
                *state = PS_WAIT_STX;
            }
        }
        break;

    default:
        *state = PS_WAIT_STX;
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  파서 태스크                                                        */
/* ------------------------------------------------------------------ */
void UART_ParserTask(void *argument)
{
    (void)argument;

    static UartFrame_t  frame;
    static ParseState_t state = PS_WAIT_STX;
    uint8_t chunk[32];

    for (;;) {
        size_t n = xStreamBufferReceive(s_rxStream,
                                        chunk,
                                        sizeof(chunk),
                                        portMAX_DELAY);
        for (size_t i = 0; i < n; ++i) {
            Parser_Feed(chunk[i], &state, &frame);
        }
    }
}
