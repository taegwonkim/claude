/**
 * @file    usart_dma.h
 * @brief   USART1 DMA Normal Mode + IDLE Line Interrupt 수신 관리
 *
 * 동작 원리:
 *   1. HAL_UART_Receive_DMA()로 DMA 수신 시작 (RX_DMA_BUF_SIZE 크기)
 *   2. PC가 데이터 전송을 멈추면 IDLE Line 인터럽트 발생
 *   3. ISR에서 수신된 바이트 수 계산 → RxFrame을 Queue에 전송
 *   4. DMA를 재시작하여 다음 프레임 수신 대기
 *   5. 파서 태스크가 Queue에서 프레임을 꺼내 STX/ETX 파싱 수행
 */

#ifndef __USART_DMA_H
#define __USART_DMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>

/* ────────────────────────────────────────────────────────────
 *  프로토콜 / 버퍼 상수
 * ──────────────────────────────────────────────────────────── */
#define STX                 0x02U   /**< Start of Text       */
#define ETX                 0x03U   /**< End of Text         */

/** DMA 수신 원형 버퍼 크기 (한 번의 DMA 트랜잭션 최대 바이트) */
#define RX_DMA_BUF_SIZE     256U

/** 파서로 넘기는 프레임의 최대 크기 (DMA 버퍼와 같거나 크게) */
#define RX_FRAME_BUF_SIZE   RX_DMA_BUF_SIZE

/** 애플리케이션 레벨 패킷 데이터 최대 크기 (STX/ETX 제외) */
#define MAX_PACKET_DATA_LEN 128U

/** ISR → 파서 태스크 간 Queue 깊이 */
#define RX_QUEUE_DEPTH      8U

/* ────────────────────────────────────────────────────────────
 *  데이터 구조체
 * ──────────────────────────────────────────────────────────── */

/**
 * @brief ISR에서 파서 태스크로 전달되는 원시(raw) 프레임
 *
 * Queue에 값 복사(copy-by-value)로 전달되므로
 * 버퍼가 내부에 포함되어 있습니다.
 */
typedef struct {
    uint8_t  buf[RX_FRAME_BUF_SIZE];
    uint16_t len;
} RxFrame_t;

/**
 * @brief 파싱 완료된 애플리케이션 패킷
 *
 * STX + data + ETX 에서 data 부분만 저장합니다.
 */
typedef struct {
    uint8_t  data[MAX_PACKET_DATA_LEN];
    uint16_t len;
} Packet_t;

/* ────────────────────────────────────────────────────────────
 *  공개 API
 * ──────────────────────────────────────────────────────────── */

/**
 * @brief USART DMA 수신 초기화 및 첫 번째 DMA 트랜잭션 시작
 * @param huart  CubeMX가 생성한 UART 핸들 포인터 (예: &huart1)
 *
 * 반드시 FreeRTOS 스케줄러 시작 전에 호출하거나,
 * 태스크 내에서 한 번만 호출해야 합니다.
 */
void USART_DMA_Init(UART_HandleTypeDef *huart);

/**
 * @brief UART IDLE Line 인터럽트 콜백
 *
 * stm32l5xx_it.c 의 USARTx_IRQHandler() 안에서 호출하십시오.
 * (HAL_UART_IRQHandler 호출 전에 IDLE 플래그를 직접 체크합니다.)
 *
 * @param huart  인터럽트가 발생한 UART 핸들 포인터
 */
void USART_DMA_IdleCallback(UART_HandleTypeDef *huart);

/**
 * @brief HAL DMA 수신 완료 콜백 (HAL_UART_RxCpltCallback 내에서 호출)
 *
 * DMA Normal 모드에서 버퍼가 가득 찼을 때 호출됩니다.
 * DMA를 재시작하고 프레임을 Queue에 넣습니다.
 *
 * @param huart  완료된 UART 핸들 포인터
 */
void USART_DMA_RxCpltCallback(UART_HandleTypeDef *huart);

/**
 * @brief ISR → 파서 태스크 간 Queue 핸들 반환
 * @return osMessageQueueId_t  RxFrame_t 를 담는 Queue
 */
osMessageQueueId_t USART_DMA_GetFrameQueue(void);

#ifdef __cplusplus
}
#endif

#endif /* __USART_DMA_H */
