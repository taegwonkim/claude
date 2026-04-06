/**
 * STM32L552 USART1 DMA Normal Mode - STX/ETX Protocol Reception
 *
 * 프로토콜 형식:
 *   [STX(1)] [LEN(1)] [DATA(N)] [ETX(1)]
 *
 *   - STX : 0x02 (패킷 시작)
 *   - LEN : DATA 필드의 바이트 수 (0~250)
 *   - DATA: 실제 페이로드 (LEN 바이트)
 *   - ETX : 0x03 (패킷 종료)
 *   - 총 패킷 크기 = LEN + 3 (STX + LEN + DATA + ETX)
 *
 * 수신 전략 (DMA Normal 모드, 2단계 수신):
 *   1단계: DMA로 헤더 2바이트(STX+LEN) 수신
 *   2단계: LEN 파싱 후 DMA로 나머지(DATA+ETX) 수신
 *   → 가변 길이 패킷을 정확한 크기만큼만 DMA로 수신
 *
 * USART1 핀: PA9(TX), PA10(RX) AF7
 * DMA: DMA1 Channel 1 + DMAMUX (USART1_RX)
 * Baud rate: 115200, 8N1
 * FreeRTOS 사용
 */

#include "stm32l5xx.h"
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ---- 프로토콜 정의 ---- */
#define PROTO_STX          0x02
#define PROTO_ETX          0x03
#define PROTO_HEADER_SIZE  2          /* STX + LEN */
#define PROTO_MAX_DATA     250        /* DATA 필드 최대 크기 */
#define PROTO_MAX_PACKET   (PROTO_MAX_DATA + 3)  /* STX + LEN + DATA + ETX */

/* ---- 설정 ---- */
#define USART1_BAUDRATE    115200
#define MSI_FREQ           4000000UL

#define MSG_QUEUE_LEN      4
#define TASK_RX_STACK      256
#define TASK_RX_PRIO       (tskIDLE_PRIORITY + 2)
#define TASK_PROC_STACK    256
#define TASK_PROC_PRIO     (tskIDLE_PRIORITY + 1)

/* ---- 수신 상태 ---- */
typedef enum {
    RX_STATE_HEADER,     /* STX + LEN 수신 대기 */
    RX_STATE_PAYLOAD     /* DATA + ETX 수신 대기 */
} RxState_t;

/* ---- 수신 메시지 구조체 ---- */
typedef struct {
    uint8_t  data[PROTO_MAX_DATA];
    uint8_t  len;
} ProtoMessage_t;

/* ---- 버퍼 & 핸들 ---- */
static uint8_t dma_rx_buf[PROTO_MAX_PACKET];  /* DMA 수신 버퍼 */

static volatile RxState_t rx_state = RX_STATE_HEADER;
static volatile uint8_t   expected_data_len = 0;

static SemaphoreHandle_t  xDmaComplete = NULL;  /* DMA TC 알림 */
static QueueHandle_t      xMsgQueue    = NULL;  /* 파싱 완료 메시지 */

/* ---- 함수 선언 ---- */
static void GPIO_Init(void);
static void USART1_Init(void);
static void DMA1_Ch1_Setup(uint8_t *buf, uint16_t size);
static void DMA1_Ch1_Start(uint8_t *buf, uint16_t size);
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
 * USART1 초기화 - 115200, 8N1, DMA 수신 활성화
 * ================================================================ */
static void USART1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    USART1->CR1 = 0;
    USART1->BRR = MSI_FREQ / USART1_BAUDRATE;

    /* DMA 수신 활성화 */
    USART1->CR3 |= USART_CR3_DMAR;

    /* TE, RE, UE */
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/* ================================================================
 * DMA1 Channel 1 초기 설정 (DMAMUX 연결)
 * Normal 모드 - CIRC 비트 설정하지 않음
 * ================================================================ */
static void DMA1_Ch1_Setup(uint8_t *buf, uint16_t size)
{
    /* DMA1, DMAMUX1 클럭 활성화 */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN;

    /* 채널 비활성화 */
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;

    /* DMAMUX: USART1_RX = Request 24 */
    DMAMUX1_Channel0->CCR = 0;
    DMAMUX1_Channel0->CCR = (24UL << DMAMUX_CxCR_DMAREQ_ID_Pos);

    /*
     * CCR 설정 (Normal 모드):
     *  - MINC : 메모리 주소 증가
     *  - TCIE : Transfer Complete 인터럽트
     *  - CIRC 없음 → Normal 모드 (지정된 크기만큼 수신 후 자동 정지)
     */
    DMA1_Channel1->CCR = DMA_CCR_MINC
                       | DMA_CCR_TCIE;

    /* 주변장치 주소: USART1 RDR */
    DMA1_Channel1->CPAR = (uint32_t)&USART1->RDR;

    /* 메모리 주소 & 전송 크기 */
    DMA1_Channel1->CMAR  = (uint32_t)buf;
    DMA1_Channel1->CNDTR = size;

    /* NVIC */
    NVIC_SetPriority(DMA1_Channel1_IRQn, 6);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    /* 채널 활성화 */
    DMA1_Channel1->CCR |= DMA_CCR_EN;
}

/* ================================================================
 * DMA 재시작 - 새로운 버퍼/크기로 Normal 전송 시작
 *
 * Normal 모드에서는 전송 완료 후 DMA가 자동 정지하므로,
 * 다음 수신을 위해 매번 재설정이 필요합니다.
 * ================================================================ */
static void DMA1_Ch1_Start(uint8_t *buf, uint16_t size)
{
    /* 채널 비활성화 (설정 변경 전 필수) */
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;

    /* 플래그 클리어 */
    DMA1->IFCR = DMA_IFCR_CGIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1;

    /* 새 버퍼 주소 & 전송 크기 설정 */
    DMA1_Channel1->CMAR  = (uint32_t)buf;
    DMA1_Channel1->CNDTR = size;

    /* 채널 활성화 → DMA가 지정된 크기만큼 수신 후 자동 정지 */
    DMA1_Channel1->CCR |= DMA_CCR_EN;
}

/* ================================================================
 * USART1 송신 (응답/디버그용)
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
 * DMA1 Channel 1 인터럽트 핸들러
 *
 * Normal 모드에서 지정된 바이트 수신 완료 시 호출됩니다.
 * 세마포어를 통해 수신 태스크를 깨웁니다.
 * ================================================================ */
void DMA1_Channel1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Transfer Complete */
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        DMA1->IFCR = DMA_IFCR_CTCIF1;

        if (xDmaComplete != NULL) {
            xSemaphoreGiveFromISR(xDmaComplete, &xHigherPriorityTaskWoken);
        }
    }

    /* Transfer Error */
    if (DMA1->ISR & DMA_ISR_TEIF1) {
        DMA1->IFCR = DMA_IFCR_CTEIF1;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ================================================================
 * USART 수신 태스크 - 2단계 DMA Normal 수신
 *
 *  ┌─────────────────────────────────────────────┐
 *  │ 1단계: DMA로 헤더 2바이트 수신 (STX + LEN)   │
 *  │        ↓ TC 인터럽트 → 세마포어 해제          │
 *  │ 검증: STX == 0x02?                           │
 *  │        ↓ Yes                                 │
 *  │ 2단계: DMA로 DATA(LEN) + ETX(1) 수신         │
 *  │        ↓ TC 인터럽트 → 세마포어 해제          │
 *  │ 검증: ETX == 0x03?                           │
 *  │        ↓ Yes                                 │
 *  │ 큐에 메시지 전달 → 1단계로 복귀               │
 *  └─────────────────────────────────────────────┘
 * ================================================================ */
static void vTaskUartRx(void *pvParameters)
{
    (void)pvParameters;

    ProtoMessage_t msg;

    /* 1단계: 헤더 수신으로 시작 */
    rx_state = RX_STATE_HEADER;
    DMA1_Ch1_Setup(dma_rx_buf, PROTO_HEADER_SIZE);

    for (;;) {
        /* DMA Transfer Complete 대기 */
        if (xSemaphoreTake(xDmaComplete, portMAX_DELAY) != pdPASS) {
            continue;
        }

        switch (rx_state) {

        /* ---- 1단계: 헤더(STX + LEN) 수신 완료 ---- */
        case RX_STATE_HEADER:
        {
            uint8_t stx = dma_rx_buf[0];
            uint8_t len = dma_rx_buf[1];

            if (stx != PROTO_STX) {
                /*
                 * STX 불일치 → 동기화 오류
                 * 1바이트씩 DMA로 수신하여 STX를 찾습니다.
                 */
                DMA1_Ch1_Start(dma_rx_buf, 1);
                /* 1바이트 수신 후 다시 검사 (STATE는 HEADER 유지) */

                /* 방금 받은 버퍼에서 STX를 재확인 */
                if (dma_rx_buf[0] == PROTO_STX && len <= PROTO_MAX_DATA) {
                    /* 우연히 두 번째 바이트가 유효한 LEN일 수 있음 → 무시하고 재동기화 */
                }
                /* 헤더를 다시 수신 */
                rx_state = RX_STATE_HEADER;
                DMA1_Ch1_Start(dma_rx_buf, PROTO_HEADER_SIZE);
                break;
            }

            if (len == 0 || len > PROTO_MAX_DATA) {
                /* 잘못된 길이 → 헤더 다시 수신 */
                rx_state = RX_STATE_HEADER;
                DMA1_Ch1_Start(dma_rx_buf, PROTO_HEADER_SIZE);
                break;
            }

            /* 유효한 헤더 → 2단계: DATA(len) + ETX(1) 수신 */
            expected_data_len = len;
            rx_state = RX_STATE_PAYLOAD;
            DMA1_Ch1_Start(&dma_rx_buf[PROTO_HEADER_SIZE], len + 1);  /* DATA + ETX */
            break;
        }

        /* ---- 2단계: DATA + ETX 수신 완료 ---- */
        case RX_STATE_PAYLOAD:
        {
            uint8_t etx = dma_rx_buf[PROTO_HEADER_SIZE + expected_data_len];

            if (etx != PROTO_ETX) {
                /* ETX 불일치 → 패킷 폐기, 헤더부터 다시 수신 */
                rx_state = RX_STATE_HEADER;
                DMA1_Ch1_Start(dma_rx_buf, PROTO_HEADER_SIZE);
                break;
            }

            /* 유효한 패킷 → 메시지 구성 후 큐에 전달 */
            msg.len = expected_data_len;
            memcpy(msg.data, &dma_rx_buf[PROTO_HEADER_SIZE], expected_data_len);

            xQueueSend(xMsgQueue, &msg, pdMS_TO_TICKS(10));

            /* 다음 패킷: 헤더 수신으로 복귀 */
            rx_state = RX_STATE_HEADER;
            DMA1_Ch1_Start(dma_rx_buf, PROTO_HEADER_SIZE);
            break;
        }

        } /* switch */
    }
}

/* ================================================================
 * 데이터 처리 태스크
 *
 * 큐에서 파싱 완료된 메시지를 받아 처리합니다.
 * ================================================================ */
static void vTaskProcess(void *pvParameters)
{
    (void)pvParameters;

    ProtoMessage_t msg;

    for (;;) {
        if (xQueueReceive(xMsgQueue, &msg, portMAX_DELAY) == pdPASS) {
            /*
             * 수신 완료된 패킷 처리
             * msg.data[0..msg.len-1] 에 페이로드가 들어있습니다.
             *
             * 예: 명령 파싱, 응답 생성 등
             */

            /* 예: ACK 응답 전송 (STX + 0x06 + ETX) */
            uint8_t ack[] = { PROTO_STX, 0x01, 0x06, PROTO_ETX };
            USART1_Transmit(ack, sizeof(ack));
        }
    }
}

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    GPIO_Init();
    USART1_Init();

    /* FreeRTOS 동기화 객체 생성 */
    xDmaComplete = xSemaphoreCreateBinary();
    xMsgQueue    = xQueueCreate(MSG_QUEUE_LEN, sizeof(ProtoMessage_t));
    configASSERT(xDmaComplete != NULL);
    configASSERT(xMsgQueue != NULL);

    /* 태스크 생성 */
    xTaskCreate(vTaskUartRx,   "UART_RX",  TASK_RX_STACK,   NULL, TASK_RX_PRIO,   NULL);
    xTaskCreate(vTaskProcess,  "PROCESS",  TASK_PROC_STACK, NULL, TASK_PROC_PRIO,  NULL);

    vTaskStartScheduler();

    for (;;) {}
}
