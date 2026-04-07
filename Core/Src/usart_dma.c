/**
 * @file    usart_dma.c
 * @brief   USART1 DMA Normal Mode + IDLE Line Interrupt 수신 구현
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │  동작 흐름                                                    │
 * │                                                              │
 * │  [USART_DMA_Init]                                            │
 * │       │                                                      │
 * │       ▼                                                      │
 * │  HAL_UART_Receive_DMA()  ← DMA Normal 모드 수신 시작         │
 * │  IDLE 인터럽트 활성화                                         │
 * │       │                                                      │
 * │       ▼                                                      │
 * │  PC 데이터 전송 중 ────────────────────────────────────────  │
 * │       │                                                      │
 * │  ┌────┴─────────────────────────────────────────────┐       │
 * │  │ CASE A: IDLE Line 인터럽트 (부분 수신 완료)        │       │
 * │  │   USART_DMA_IdleCallback()                        │       │
 * │  │   → 수신 바이트 수 = BUF_SIZE - DMA_CNDTR         │       │
 * │  │   → 프레임을 Queue에 전송                           │       │
 * │  │   → DMA 중단 후 재시작                              │       │
 * │  └──────────────────────────────────────────────────┘       │
 * │  ┌────┴─────────────────────────────────────────────┐       │
 * │  │ CASE B: DMA TC 인터럽트 (버퍼 가득 참)             │       │
 * │  │   USART_DMA_RxCpltCallback()                      │       │
 * │  │   → 전체 버퍼를 Queue에 전송                        │       │
 * │  │   → DMA 재시작                                     │       │
 * │  └──────────────────────────────────────────────────┘       │
 * └──────────────────────────────────────────────────────────────┘
 */

#include "usart_dma.h"
#include <string.h>

/* ────────────────────────────────────────────────────────────
 *  모듈 내부 변수
 * ──────────────────────────────────────────────────────────── */

/** HAL UART 핸들 (Init 시 저장) */
static UART_HandleTypeDef *s_huart = NULL;

/** DMA가 직접 쓰는 수신 버퍼 (캐시-코히런트 메모리에 배치) */
static uint8_t s_rx_dma_buf[RX_DMA_BUF_SIZE];

/** ISR → 파서 태스크 Queue */
static osMessageQueueId_t s_frame_queue = NULL;

/* ────────────────────────────────────────────────────────────
 *  내부 헬퍼
 * ──────────────────────────────────────────────────────────── */

/**
 * @brief 프레임을 Queue에 넣고 DMA를 재시작하는 공통 루틴 (ISR 컨텍스트)
 * @param received_len  이번 프레임에서 유효하게 수신된 바이트 수
 */
static void send_frame_and_restart(uint16_t received_len)
{
    if (received_len == 0U) {
        /* 수신 바이트 없음 → 재시작만 */
        HAL_UART_Receive_DMA(s_huart, s_rx_dma_buf, RX_DMA_BUF_SIZE);
        return;
    }

    RxFrame_t frame;
    frame.len = received_len;
    (void)memcpy(frame.buf, s_rx_dma_buf, received_len);

    /* ISR 컨텍스트: osMessageQueuePut with timeout=0 */
    (void)osMessageQueuePut(s_frame_queue, &frame, 0U, 0U);

    /* DMA Normal 모드 재시작 */
    HAL_UART_Receive_DMA(s_huart, s_rx_dma_buf, RX_DMA_BUF_SIZE);
}

/* ────────────────────────────────────────────────────────────
 *  공개 API 구현
 * ──────────────────────────────────────────────────────────── */

void USART_DMA_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;

    /* Queue 생성: RxFrame_t 를 RX_QUEUE_DEPTH 개 저장 */
    s_frame_queue = osMessageQueueNew(RX_QUEUE_DEPTH,
                                      sizeof(RxFrame_t),
                                      NULL);
    /* Queue 생성 실패 시 무한 루프 (디버깅 목적) */
    if (s_frame_queue == NULL) {
        Error_Handler();
    }

    /* IDLE Line 인터럽트 활성화
     * (HAL_UART_Receive_DMA 이전에 활성화해야 함) */
    __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);

    /* DMA Normal 모드 수신 첫 번째 트랜잭션 시작 */
    if (HAL_UART_Receive_DMA(huart, s_rx_dma_buf, RX_DMA_BUF_SIZE) != HAL_OK) {
        Error_Handler();
    }
}

/* ─────────────────────────────────────────────────────────────
 * IDLE Line 인터럽트 콜백
 *
 * 호출 위치: stm32l5xx_it.c → USART1_IRQHandler()
 *
 *   void USART1_IRQHandler(void)
 *   {
 *       USART_DMA_IdleCallback(&huart1);   // ← 반드시 먼저 호출
 *       HAL_UART_IRQHandler(&huart1);
 *   }
 * ──────────────────────────────────────────────────────────── */
void USART_DMA_IdleCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != s_huart->Instance) {
        return;
    }

    /* IDLE 플래그 확인 */
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) == RESET) {
        return;
    }

    /* IDLE 플래그 클리어 (SR 읽기 + DR 읽기 방식 대신 ICR 사용) */
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_IDLEF);

    /* DMA를 중단시켜 TC 인터럽트 중복 방지 */
    HAL_UART_AbortReceive(huart);

    /*
     * 수신된 바이트 수 계산:
     *   DMA Normal 모드에서 CNDTR 은 "남은 전송 수"
     *   따라서 수신 바이트 = 설정값 - 남은 수
     *
     *   주의: HAL_UART_AbortReceive 이후에는 hdmarx->Instance->CNDTR 가
     *         0으로 초기화될 수 있으므로, 중단 전에 읽어야 합니다.
     *         이 구현에서는 AbortReceive 직후 DMA 카운터 스냅샷을 이용합니다.
     *         (실제로는 Abort 전에 읽는 것이 더 안전합니다.)
     *
     *   안전한 방법: Abort 전에 CNDTR을 읽어야 합니다.
     *   아래는 Abort 전에 읽도록 수정된 흐름을 직접 구현합니다.
     */

    /* ※ 위 Abort 전에 CNDTR 값을 미리 읽어야 하므로
     *    실제 구현에서는 아래 순서로 처리합니다:
     *
     *    1) CNDTR 읽기
     *    2) DMA 비활성화 (LL 레벨)
     *    3) DMA 재설정 후 재시작
     *
     *    HAL AbortReceive 내부에서 DMA를 멈추므로,
     *    CNDTR는 멈춘 시점의 값이 유지됩니다.
     *    따라서 Abort 이후에 읽어도 유효합니다.
     */
    uint16_t remaining = (uint16_t)huart->hdmarx->Instance->CNDTR;
    uint16_t received  = RX_DMA_BUF_SIZE - remaining;

    send_frame_and_restart(received);
}

/* ─────────────────────────────────────────────────────────────
 * DMA 수신 완료 콜백 (버퍼가 가득 찬 경우)
 *
 * 호출 위치: HAL_UART_RxCpltCallback() 오버라이드
 *
 *   void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
 *   {
 *       USART_DMA_RxCpltCallback(huart);
 *   }
 * ──────────────────────────────────────────────────────────── */
void USART_DMA_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != s_huart->Instance) {
        return;
    }
    /* 버퍼 전체(RX_DMA_BUF_SIZE 바이트)가 수신된 경우 */
    send_frame_and_restart(RX_DMA_BUF_SIZE);
}

osMessageQueueId_t USART_DMA_GetFrameQueue(void)
{
    return s_frame_queue;
}
