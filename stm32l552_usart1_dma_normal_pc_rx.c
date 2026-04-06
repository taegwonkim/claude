/**
 * STM32L552 USART1 DMA Normal Mode - PC로부터 STX/DATA/ETX 패킷 수신
 *
 * 패킷 형식 (PC → MCU):
 *   [STX(1)] [CMD(1)] [LEN(1)] [DATA(0~255)] [ETX(1)]
 *
 *   - STX  : 0x02 (패킷 시작)
 *   - CMD  : 명령 코드 (0x00~0xFF)
 *   - LEN  : DATA 필드 바이트 수 (0이면 DATA 없음)
 *   - DATA : 페이로드 (LEN 바이트, 최대 255)
 *   - ETX  : 0x03 (패킷 종료)
 *
 * 수신 전략 (DMA Normal + IDLE 인터럽트 병행):
 *   - USART1 RXNE 인터럽트로 1바이트씩 STX를 탐색
 *   - STX 감지 후 RXNE OFF → DMA Normal로 나머지(CMD+LEN) 2바이트 수신
 *   - LEN 파싱 후 DMA Normal로 DATA(LEN)+ETX(1) 수신
 *   - ETX 검증 후 콜백 호출
 *
 * 핀: PA9(TX AF7), PA10(RX AF7)
 * Baud: 115200, 8N1
 * Bare-metal (FreeRTOS 미사용)
 */

#include "stm32l5xx.h"
#include <stdint.h>
#include <string.h>

/* ---- 프로토콜 정의 ---- */
#define PROTO_STX           0x02
#define PROTO_ETX           0x03
#define PROTO_CMD_LEN_SIZE  2       /* CMD + LEN */
#define PROTO_MAX_DATA      255
#define PROTO_MAX_PACKET    (1 + 1 + 1 + PROTO_MAX_DATA + 1)  /* STX+CMD+LEN+DATA+ETX */

/* ---- 하드웨어 설정 ---- */
#define USART1_BAUDRATE     115200
#define MSI_FREQ            4000000UL

/* ---- 수신 상태머신 ---- */
typedef enum {
    RX_WAIT_STX,        /* STX 바이트 탐색 (RXNE 인터럽트) */
    RX_RECV_CMD_LEN,    /* CMD+LEN 2바이트 DMA 수신 중 */
    RX_RECV_PAYLOAD,    /* DATA+ETX DMA 수신 중 */
} RxState_t;

/* ---- 파싱된 패킷 구조체 ---- */
typedef struct {
    uint8_t  cmd;
    uint8_t  data[PROTO_MAX_DATA];
    uint8_t  len;
    uint8_t  valid;     /* 1 = 새 패킷 도착 */
} Packet_t;

/* ---- 전역 변수 ---- */
static volatile RxState_t  g_rx_state = RX_WAIT_STX;
static uint8_t             g_dma_buf[PROTO_MAX_DATA + 1]; /* DMA 수신 버퍼 (최대: DATA+ETX) */
static volatile Packet_t   g_rx_packet;                    /* 수신 완료 패킷 */

/* 통계 (디버그용) */
static volatile uint32_t   g_rx_ok_count  = 0;  /* 정상 수신 패킷 수 */
static volatile uint32_t   g_rx_err_count = 0;  /* 에러 패킷 수 */

/* ---- 함수 선언 ---- */
static void GPIO_Init(void);
static void USART1_Init(void);
static void DMA1_Ch1_Init(void);
static void DMA1_Ch1_Start(uint8_t *buf, uint16_t size);
static void RxState_Reset(void);
static void USART1_TransmitByte(uint8_t byte);
static void USART1_Transmit(const uint8_t *data, uint32_t len);
static void SendAck(uint8_t cmd);
static void SendNak(uint8_t cmd, uint8_t err_code);

/* ================================================================
 * GPIO 초기화
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
 * USART1 초기화 - RXNE 인터럽트로 시작 (STX 탐색용)
 * ================================================================ */
static void USART1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    USART1->CR1 = 0;
    USART1->CR2 = 0;
    USART1->CR3 = 0;

    USART1->BRR = MSI_FREQ / USART1_BAUDRATE;

    /* DMA 수신 활성화 (DMA 시작 전까지는 동작하지 않음) */
    USART1->CR3 |= USART_CR3_DMAR;

    /* RXNE 인터럽트 활성화 (STX 탐색용) */
    USART1->CR1 |= USART_CR1_RXNEIE;

    /* TE, RE, UE */
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);
}

/* ================================================================
 * DMA1 Channel 1 초기 설정 (아직 활성화하지 않음)
 * ================================================================ */
static void DMA1_Ch1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN;

    DMA1_Channel1->CCR = 0;

    /* DMAMUX: USART1_RX = Request 24 */
    DMAMUX1_Channel0->CCR = (24UL << DMAMUX_CxCR_DMAREQ_ID_Pos);

    /* Normal 모드, 메모리 증가, TC 인터럽트 */
    DMA1_Channel1->CCR = DMA_CCR_MINC | DMA_CCR_TCIE;

    /* 주변장치 주소 고정 */
    DMA1_Channel1->CPAR = (uint32_t)&USART1->RDR;

    NVIC_SetPriority(DMA1_Channel1_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/* ================================================================
 * DMA Normal 전송 시작
 * ================================================================ */
static void DMA1_Ch1_Start(uint8_t *buf, uint16_t size)
{
    /* 채널 비활성화 */
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;

    /* 플래그 클리어 */
    DMA1->IFCR = DMA_IFCR_CGIF1;

    /* 버퍼 & 크기 설정 */
    DMA1_Channel1->CMAR  = (uint32_t)buf;
    DMA1_Channel1->CNDTR = size;

    /* 활성화 → 지정 크기만큼 수신 후 자동 정지 */
    DMA1_Channel1->CCR |= DMA_CCR_EN;
}

/* ================================================================
 * 수신 상태 리셋 → STX 탐색 모드로 복귀
 * ================================================================ */
static void RxState_Reset(void)
{
    /* DMA 정지 */
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF1;

    /* Overrun 등 에러 플래그 클리어 */
    USART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;

    /* RXNE에 남아있는 데이터 비우기 */
    if (USART1->ISR & USART_ISR_RXNE_RXFNE) {
        (void)USART1->RDR;
    }

    /* RXNE 인터럽트 다시 활성화 → STX 탐색 */
    USART1->CR1 |= USART_CR1_RXNEIE;
    g_rx_state = RX_WAIT_STX;
}

/* ================================================================
 * USART1 인터럽트 핸들러 - STX 탐색 전용
 *
 * STX를 찾으면 RXNE 인터럽트를 끄고 DMA로 전환합니다.
 * ================================================================ */
void USART1_IRQHandler(void)
{
    /* ---- RXNE: STX 탐색 ---- */
    if ((USART1->ISR & USART_ISR_RXNE_RXFNE) && (USART1->CR1 & USART_CR1_RXNEIE)) {
        uint8_t byte = (uint8_t)(USART1->RDR & 0xFF);

        if (g_rx_state == RX_WAIT_STX && byte == PROTO_STX) {
            /*
             * STX 발견!
             * RXNE 인터럽트 OFF → DMA로 CMD+LEN(2바이트) 수신
             */
            USART1->CR1 &= ~USART_CR1_RXNEIE;

            g_rx_state = RX_RECV_CMD_LEN;
            DMA1_Ch1_Start(g_dma_buf, PROTO_CMD_LEN_SIZE);
        }
        /* STX가 아닌 바이트는 무시 (자동 동기화) */
    }

    /* ---- Overrun 에러 ---- */
    if (USART1->ISR & USART_ISR_ORE) {
        USART1->ICR = USART_ICR_ORECF;
    }

    /* ---- Framing 에러 ---- */
    if (USART1->ISR & USART_ISR_FE) {
        USART1->ICR = USART_ICR_FECF;
    }
}

/* ================================================================
 * DMA1 Channel 1 인터럽트 핸들러
 *
 * 상태에 따라:
 *   RX_RECV_CMD_LEN  → CMD+LEN 수신 완료 → DATA+ETX DMA 시작
 *   RX_RECV_PAYLOAD  → DATA+ETX 수신 완료 → ETX 검증 → 패킷 완성
 * ================================================================ */
void DMA1_Channel1_IRQHandler(void)
{
    if (!(DMA1->ISR & DMA_ISR_TCIF1)) {
        /* Transfer Error 등 */
        DMA1->IFCR = DMA_IFCR_CGIF1;
        RxState_Reset();
        g_rx_err_count++;
        return;
    }

    DMA1->IFCR = DMA_IFCR_CTCIF1;

    switch (g_rx_state) {

    /* ---- CMD + LEN 수신 완료 ---- */
    case RX_RECV_CMD_LEN:
    {
        uint8_t cmd = g_dma_buf[0];
        uint8_t len = g_dma_buf[1];

        if (len == 0) {
            /*
             * DATA 없는 패킷: [STX][CMD][LEN=0][ETX]
             * ETX(1바이트)만 DMA로 수신
             */
            g_rx_state = RX_RECV_PAYLOAD;
            DMA1_Ch1_Start(&g_dma_buf[PROTO_CMD_LEN_SIZE], 1);
        } else if (len <= PROTO_MAX_DATA) {
            /*
             * DATA 있는 패킷: DATA(LEN) + ETX(1) 수신
             */
            g_rx_state = RX_RECV_PAYLOAD;
            DMA1_Ch1_Start(&g_dma_buf[PROTO_CMD_LEN_SIZE], len + 1);
        } else {
            /* 잘못된 LEN → 에러, 리셋 */
            g_rx_err_count++;
            RxState_Reset();
        }
        (void)cmd;  /* cmd는 나중에 g_dma_buf[0]에서 읽음 */
        break;
    }

    /* ---- DATA + ETX 수신 완료 ---- */
    case RX_RECV_PAYLOAD:
    {
        uint8_t cmd = g_dma_buf[0];
        uint8_t len = g_dma_buf[1];
        uint8_t etx = g_dma_buf[PROTO_CMD_LEN_SIZE + len];

        if (etx == PROTO_ETX) {
            /* 유효한 패킷 → 구조체에 저장 */
            g_rx_packet.cmd   = cmd;
            g_rx_packet.len   = len;
            if (len > 0) {
                memcpy((void *)g_rx_packet.data,
                       &g_dma_buf[PROTO_CMD_LEN_SIZE],
                       len);
            }
            g_rx_packet.valid = 1;
            g_rx_ok_count++;
        } else {
            /* ETX 불일치 → 패킷 폐기 */
            g_rx_err_count++;
        }

        /* 다음 패킷: STX 탐색으로 복귀 */
        RxState_Reset();
        break;
    }

    default:
        RxState_Reset();
        break;
    }
}

/* ================================================================
 * 송신 함수
 * ================================================================ */
static void USART1_TransmitByte(uint8_t byte)
{
    while (!(USART1->ISR & USART_ISR_TXE_TXFNF));
    USART1->TDR = byte;
}

static void USART1_Transmit(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        USART1_TransmitByte(data[i]);
    }
    while (!(USART1->ISR & USART_ISR_TC));
}

/* ================================================================
 * ACK/NAK 응답 전송 (PC에 응답)
 *
 * ACK: [STX][CMD][LEN=1][0x06][ETX]
 * NAK: [STX][CMD][LEN=2][0x15][ERR_CODE][ETX]
 * ================================================================ */
static void SendAck(uint8_t cmd)
{
    uint8_t pkt[] = { PROTO_STX, cmd, 0x01, 0x06, PROTO_ETX };
    USART1_Transmit(pkt, sizeof(pkt));
}

static void SendNak(uint8_t cmd, uint8_t err_code)
{
    uint8_t pkt[] = { PROTO_STX, cmd, 0x02, 0x15, err_code, PROTO_ETX };
    USART1_Transmit(pkt, sizeof(pkt));
}

/* ================================================================
 * 패킷 처리 함수 (main 루프에서 호출)
 * ================================================================ */
static void ProcessPacket(const Packet_t *pkt)
{
    switch (pkt->cmd) {
    case 0x01:  /* 예: LED 제어 */
        /* pkt->data[0] = LED 번호, pkt->data[1] = ON/OFF */
        SendAck(pkt->cmd);
        break;

    case 0x02:  /* 예: 센서 데이터 요청 */
    {
        /* 응답: STX + CMD + LEN + 센서값 + ETX */
        uint8_t resp[] = {
            PROTO_STX,
            pkt->cmd,
            0x04,           /* LEN = 4 */
            0x00, 0x01,     /* 센서1 값 (예시) */
            0x00, 0x64,     /* 센서2 값 (예시) */
            PROTO_ETX
        };
        USART1_Transmit(resp, sizeof(resp));
        break;
    }

    case 0x10:  /* 예: 펌웨어 버전 요청 */
    {
        uint8_t ver[] = { 0x01, 0x00, 0x03 };  /* v1.0.3 */
        uint8_t resp[] = {
            PROTO_STX, pkt->cmd, 0x03,
            ver[0], ver[1], ver[2],
            PROTO_ETX
        };
        USART1_Transmit(resp, sizeof(resp));
        break;
    }

    default:
        SendNak(pkt->cmd, 0x01);  /* 0x01 = 알 수 없는 명령 */
        break;
    }
}

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    GPIO_Init();
    DMA1_Ch1_Init();
    USART1_Init();

    /* 시작 메시지 전송 (PC에서 연결 확인용) */
    const char *hello = "STM32L552 READY\r\n";
    USART1_Transmit((const uint8_t *)hello, strlen(hello));

    while (1) {
        /* 수신 완료된 패킷이 있으면 처리 */
        if (g_rx_packet.valid) {
            Packet_t pkt;
            /* ISR과의 경쟁 방지: 인터럽트 잠시 비활성화 후 복사 */
            __disable_irq();
            pkt = *(const Packet_t *)&g_rx_packet;
            g_rx_packet.valid = 0;
            __enable_irq();

            ProcessPacket(&pkt);
        }

        __WFI();  /* 인터럽트 대기 (저전력) */
    }
}
