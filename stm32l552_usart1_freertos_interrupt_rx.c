/**
 * STM32L552 USART1 FreeRTOS Interrupt-based Reception Example
 *
 * USART1 핀 설정:
 *   - PA9  : USART1_TX (AF7)
 *   - PA10 : USART1_RX (AF7)
 *
 * Baud rate: 115200, 8N1
 *
 * RXNE 인터럽트로 바이트를 수신하여 ISR-safe 큐에 넣고,
 * 처리 태스크에서 큐를 통해 데이터를 소비합니다.
 *
 * 폴링 방식 대비 장점:
 *   - CPU 점유율 최소화 (ISR에서만 깨어남)
 *   - 바이트 유실 위험 감소 (즉시 처리)
 *   - 고속 보레이트에서도 안정적
 */

#include "stm32l5xx.h"
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "stream_buffer.h"

/* ---- 설정 ---- */
#define USART1_BAUDRATE    115200
#define MSI_FREQ           4000000UL

#define STREAM_BUF_SIZE    256    /* 스트림 버퍼 크기 (바이트) */
#define STREAM_TRIGGER     1     /* 1바이트 도착 시 태스크 깨움 */

#define APP_BUF_SIZE       128   /* 처리 태스크 수신 버퍼 */

#define TASK_PROC_STACK    256
#define TASK_PROC_PRIO     (tskIDLE_PRIORITY + 2)

/* ---- 핸들 ---- */
static StreamBufferHandle_t xRxStream = NULL;

/* ---- 함수 선언 ---- */
static void GPIO_Init(void);
static void USART1_Init(void);
static void USART1_Transmit(const uint8_t *data, uint32_t len);
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
 * USART1 초기화 - 115200, 8N1, RXNE + IDLE 인터럽트 활성화
 * ================================================================ */
static void USART1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    USART1->CR1 = 0;
    USART1->BRR = MSI_FREQ / USART1_BAUDRATE;

    /*
     * CR1 인터럽트 설정:
     *   RXNEIE : 수신 데이터 레지스터에 데이터가 들어오면 인터럽트
     *   IDLEIE : 수신 후 버스가 IDLE 상태가 되면 인터럽트 (패킷 경계 감지)
     */
    USART1->CR1 |= USART_CR1_RXNEIE
                 |  USART_CR1_IDLEIE;

    /* TE, RE, UE 활성화 */
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    /*
     * NVIC 설정
     * 주의: FreeRTOS에서 ISR-safe API를 사용하려면 인터럽트 우선순위가
     * configMAX_SYSCALL_INTERRUPT_PRIORITY 이상(숫자가 같거나 큰) 이어야 합니다.
     * 일반적으로 5 이상으로 설정합니다.
     */
    NVIC_SetPriority(USART1_IRQn, 6);
    NVIC_EnableIRQ(USART1_IRQn);
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
    while (!(USART1->ISR & USART_ISR_TC)) {
        taskYIELD();
    }
}

/* ================================================================
 * USART1 인터럽트 핸들러
 *
 * ISR에서 수신된 바이트를 FreeRTOS StreamBuffer에 넣습니다.
 * StreamBuffer는 ISR → Task 간 바이트 스트림 전달에 최적화되어 있습니다.
 *
 * - RXNE: 바이트 단위로 수신하여 스트림 버퍼에 저장
 * - IDLE: 패킷 경계 감지 (선택적 활용)
 * ================================================================ */
void USART1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* ---- RXNE: 수신 데이터 있음 ---- */
    if ((USART1->ISR & USART_ISR_RXNE_RXFNE) && (USART1->CR1 & USART_CR1_RXNEIE)) {
        uint8_t byte = (uint8_t)(USART1->RDR & 0xFF);  /* RDR 읽기 → RXNE 자동 클리어 */

        if (xRxStream != NULL) {
            xStreamBufferSendFromISR(xRxStream,
                                     &byte,
                                     1,
                                     &xHigherPriorityTaskWoken);
        }
    }

    /* ---- IDLE: 수신 후 버스 유휴 상태 ---- */
    if (USART1->ISR & USART_ISR_IDLE) {
        USART1->ICR = USART_ICR_IDLECF;  /* 플래그 클리어 */

        /*
         * IDLE 감지 시 추가 처리가 필요하면 여기에 구현합니다.
         * 예: 이벤트 그룹 비트 설정, 세마포어 해제 등으로
         *     처리 태스크에 "패킷 수신 완료"를 알릴 수 있습니다.
         */
    }

    /* ---- Overrun 에러 처리 ---- */
    if (USART1->ISR & USART_ISR_ORE) {
        USART1->ICR = USART_ICR_ORECF;
    }

    /* ---- Framing 에러 처리 ---- */
    if (USART1->ISR & USART_ISR_FE) {
        USART1->ICR = USART_ICR_FECF;
    }

    /* ---- Noise 에러 처리 ---- */
    if (USART1->ISR & USART_ISR_NE) {
        USART1->ICR = USART_ICR_NECF;
    }

    /* 컨텍스트 스위치 필요 시 수행 */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ================================================================
 * 데이터 처리 태스크
 *
 * StreamBuffer에서 수신 데이터를 블록 대기로 읽어 처리합니다.
 * 데이터가 없으면 태스크는 자동으로 Blocked 상태가 되어
 * CPU를 소비하지 않습니다.
 * ================================================================ */
static void vTaskProcess(void *pvParameters)
{
    (void)pvParameters;

    uint8_t  rx_buf[APP_BUF_SIZE];
    size_t   received;

    for (;;) {
        /*
         * StreamBuffer에서 데이터 수신 대기
         * - 최소 STREAM_TRIGGER(1) 바이트가 도착하면 깨어남
         * - 100ms 타임아웃 → 주기적으로 깨어나서 다른 작업 가능
         * - portMAX_DELAY로 변경하면 데이터 올 때까지 무한 대기
         */
        received = xStreamBufferReceive(xRxStream,
                                        rx_buf,
                                        APP_BUF_SIZE,
                                        pdMS_TO_TICKS(100));

        if (received > 0) {
            /*
             * 수신 데이터 처리
             * 여기에 프로토콜 파싱, 명령 해석 등을 구현합니다.
             */

            /* 예: 수신된 데이터 에코 */
            USART1_Transmit(rx_buf, received);
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

    /* 스트림 버퍼 생성: ISR → Task 간 바이트 스트림 전달 */
    xRxStream = xStreamBufferCreate(STREAM_BUF_SIZE, STREAM_TRIGGER);
    configASSERT(xRxStream != NULL);

    /* 처리 태스크 생성 */
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
