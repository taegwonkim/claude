/**
 * STM32L552 USART1 FreeRTOS Polling Reception Example
 *
 * USART1 핀 설정:
 *   - PA9  : USART1_TX (AF7)
 *   - PA10 : USART1_RX (AF7)
 *
 * Baud rate: 115200, 8N1
 *
 * FreeRTOS 태스크에서 USART1 RXNE 플래그를 폴링하여 데이터를 수신하고,
 * 수신된 데이터를 큐(Queue)를 통해 처리 태스크로 전달합니다.
 */

#include "stm32l5xx.h"
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ---- 설정 ---- */
#define USART1_BAUDRATE   115200
#define MSI_FREQ          4000000UL

#define RX_BUF_SIZE       128          /* 한 번에 전달할 최대 수신 크기 */
#define RX_QUEUE_LENGTH   8            /* 큐에 대기 가능한 메시지 수 */

#define TASK_RX_STACK     256          /* 수신 태스크 스택 (워드 단위) */
#define TASK_RX_PRIO      (tskIDLE_PRIORITY + 2)

#define TASK_PROC_STACK   256          /* 처리 태스크 스택 */
#define TASK_PROC_PRIO    (tskIDLE_PRIORITY + 1)

#define POLL_TIMEOUT_MS   1            /* 폴링 주기 (ms) */

/* ---- 수신 메시지 구조체 ---- */
typedef struct {
    uint8_t  data[RX_BUF_SIZE];
    uint16_t len;
} RxMessage_t;

/* ---- 핸들 ---- */
static QueueHandle_t xRxQueue = NULL;

/* ---- 함수 선언 ---- */
static void GPIO_Init(void);
static void USART1_Init(void);
static void USART1_Transmit(const uint8_t *data, uint32_t len);
static void vTaskUartRx(void *pvParameters);
static void vTaskProcess(void *pvParameters);

/* ================================================================
 * GPIO 초기화 - USART1 TX(PA9), RX(PA10) AF7
 * ================================================================ */
static void GPIO_Init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /* PA9 (TX): AF7 */
    GPIOA->MODER   &= ~(3UL << (9 * 2));
    GPIOA->MODER   |=  (2UL << (9 * 2));
    GPIOA->OSPEEDR |=  (3UL << (9 * 2));
    GPIOA->AFR[1]  &= ~(0xFUL << ((9 - 8) * 4));
    GPIOA->AFR[1]  |=  (7UL << ((9 - 8) * 4));

    /* PA10 (RX): AF7, Pull-up */
    GPIOA->MODER   &= ~(3UL << (10 * 2));
    GPIOA->MODER   |=  (2UL << (10 * 2));
    GPIOA->OSPEEDR |=  (3UL << (10 * 2));
    GPIOA->PUPDR   &= ~(3UL << (10 * 2));
    GPIOA->PUPDR   |=  (1UL << (10 * 2));
    GPIOA->AFR[1]  &= ~(0xFUL << ((10 - 8) * 4));
    GPIOA->AFR[1]  |=  (7UL << ((10 - 8) * 4));
}

/* ================================================================
 * USART1 초기화 - 115200, 8N1 (인터럽트/DMA 사용 안 함)
 * ================================================================ */
static void USART1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    USART1->CR1 = 0;
    USART1->BRR = MSI_FREQ / USART1_BAUDRATE;

    /* TE, RE, UE 활성화 (인터럽트 없음, 폴링 전용) */
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/* ================================================================
 * USART1 송신 (디버그/에코용)
 * ================================================================ */
static void USART1_Transmit(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        while (!(USART1->ISR & USART_ISR_TXE_TXFNF)) {
            taskYIELD();
        }
        USART1->TDR = data[i];
    }
    /* 마지막 바이트 전송 완료 대기 */
    while (!(USART1->ISR & USART_ISR_TC)) {
        taskYIELD();
    }
}

/* ================================================================
 * USART1 수신 태스크 (폴링 방식)
 *
 * RXNE(RXFNE) 플래그를 주기적으로 확인하여 바이트를 수신합니다.
 * 일정 시간 동안 새 바이트가 없으면 (타임아웃) 수신 완료로 판단하고
 * 큐로 전달합니다.
 * ================================================================ */
static void vTaskUartRx(void *pvParameters)
{
    (void)pvParameters;

    RxMessage_t msg;
    TickType_t  last_rx_tick;
    const TickType_t idle_timeout = pdMS_TO_TICKS(10);  /* 10ms 무수신 → 패킷 완료 */

    msg.len = 0;

    for (;;) {
        if (USART1->ISR & USART_ISR_RXNE_RXFNE) {
            /* 바이트 수신 */
            uint8_t byte = (uint8_t)(USART1->RDR & 0xFF);

            if (msg.len < RX_BUF_SIZE) {
                msg.data[msg.len++] = byte;
            }
            last_rx_tick = xTaskGetTickCount();

            /* Overrun 에러 클리어 */
            if (USART1->ISR & USART_ISR_ORE) {
                USART1->ICR = USART_ICR_ORECF;
            }
        } else if (msg.len > 0) {
            /*
             * 수신 중인 데이터가 있고, idle_timeout 동안 새 바이트 없음
             * → 수신 완료로 판단하여 큐에 전달
             */
            if ((xTaskGetTickCount() - last_rx_tick) >= idle_timeout) {
                xQueueSend(xRxQueue, &msg, portMAX_DELAY);
                msg.len = 0;
            }
        }

        /*
         * 에러 플래그 처리 (Framing, Noise, Overrun)
         */
        if (USART1->ISR & USART_ISR_FE) {
            USART1->ICR = USART_ICR_FECF;
        }
        if (USART1->ISR & USART_ISR_NE) {
            USART1->ICR = USART_ICR_NECF;
        }

        /*
         * 폴링 주기: 1ms 슬립으로 CPU 점유율 제어
         * 다른 태스크가 실행될 수 있도록 양보합니다.
         *
         * 더 빠른 응답이 필요하면 POLL_TIMEOUT_MS를 줄이거나,
         * 높은 보레이트에서는 taskYIELD()로 대체할 수 있습니다.
         */
        vTaskDelay(pdMS_TO_TICKS(POLL_TIMEOUT_MS));
    }
}

/* ================================================================
 * 데이터 처리 태스크
 *
 * 큐에서 수신 메시지를 받아 처리합니다.
 * 이 예제에서는 수신된 데이터를 에코(송신)합니다.
 * ================================================================ */
static void vTaskProcess(void *pvParameters)
{
    (void)pvParameters;

    RxMessage_t msg;

    for (;;) {
        /* 큐에서 메시지 대기 (무한 블록) */
        if (xQueueReceive(xRxQueue, &msg, portMAX_DELAY) == pdPASS) {
            /*
             * 수신 데이터 처리
             * 여기에 프로토콜 파싱, 명령 해석 등을 구현합니다.
             */

            /* 예: 수신된 데이터 에코 */
            USART1_Transmit(msg.data, msg.len);
        }
    }
}

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    /* 하드웨어 초기화 */
    GPIO_Init();
    USART1_Init();

    /* 큐 생성 */
    xRxQueue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(RxMessage_t));
    configASSERT(xRxQueue != NULL);

    /* 태스크 생성 */
    xTaskCreate(vTaskUartRx,
                "UART_RX",
                TASK_RX_STACK,
                NULL,
                TASK_RX_PRIO,
                NULL);

    xTaskCreate(vTaskProcess,
                "PROCESS",
                TASK_PROC_STACK,
                NULL,
                TASK_PROC_PRIO,
                NULL);

    /* 스케줄러 시작 */
    vTaskStartScheduler();

    /* 여기에 도달하면 힙 메모리 부족 등의 오류 */
    for (;;) {}
}
