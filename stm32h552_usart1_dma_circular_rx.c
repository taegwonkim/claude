/**
 * STM32H552 USART1 DMA Circular Buffer Reception Example
 *
 * USART1 핀 설정:
 *   - PA9  : USART1_TX (AF7)
 *   - PA10 : USART1_RX (AF7)
 *
 * DMA: GPDMA1 Channel 0 (USART1_RX)
 * Baud rate: 115200
 *
 * STM32H552는 GPDMA (General Purpose DMA)를 사용합니다.
 */

#include "stm32h5xx.h"
#include <stdint.h>
#include <string.h>

/* ---- 설정 ---- */
#define DMA_BUF_SIZE     256    /* DMA 써큘러 버퍼 크기 (2의 거듭제곱 권장) */
#define APP_BUF_SIZE     512    /* 애플리케이션 처리 버퍼 크기 */
#define USART1_BAUDRATE  115200
#define HSI_FREQ         64000000UL  /* STM32H552 기본 HSI 클럭 */

/* ---- 버퍼 ---- */
static uint8_t dma_rx_buf[DMA_BUF_SIZE];   /* DMA가 직접 쓰는 써큘러 버퍼 */
static uint8_t app_rx_buf[APP_BUF_SIZE];   /* 애플리케이션 처리용 버퍼 */
static volatile uint32_t old_pos = 0;       /* 마지막으로 읽은 위치 */

/* ---- 함수 선언 ---- */
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void USART1_Init(void);
static void GPDMA1_Ch0_Init(void);
static uint32_t USART1_DMA_ReadAvailable(uint8_t *dst, uint32_t dst_size);

/* ================================================================
 * 시스템 클럭 설정 (HSI 64 MHz 사용)
 * ================================================================ */
static void SystemClock_Config(void)
{
    /* STM32H552 기본 HSI = 64 MHz, 별도 PLL 설정 없이 사용 */
    /* 필요 시 PLL/HSE 설정 추가 */
}

/* ================================================================
 * GPIO 초기화 - USART1 TX(PA9), RX(PA10) AF7
 * ================================================================ */
static void GPIO_Init(void)
{
    /* GPIOA 클럭 활성화 */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /* PA9 (TX): AF7, Push-Pull, High Speed */
    GPIOA->MODER   &= ~(3UL << (9 * 2));
    GPIOA->MODER   |=  (2UL << (9 * 2));   /* Alternate Function */
    GPIOA->OSPEEDR |=  (3UL << (9 * 2));   /* Very High Speed */
    GPIOA->AFR[1]  &= ~(0xFUL << ((9 - 8) * 4));
    GPIOA->AFR[1]  |=  (7UL << ((9 - 8) * 4));  /* AF7 = USART1 */

    /* PA10 (RX): AF7, Push-Pull, High Speed */
    GPIOA->MODER   &= ~(3UL << (10 * 2));
    GPIOA->MODER   |=  (2UL << (10 * 2));  /* Alternate Function */
    GPIOA->OSPEEDR |=  (3UL << (10 * 2));
    GPIOA->PUPDR   &= ~(3UL << (10 * 2));
    GPIOA->PUPDR   |=  (1UL << (10 * 2));  /* Pull-up */
    GPIOA->AFR[1]  &= ~(0xFUL << ((10 - 8) * 4));
    GPIOA->AFR[1]  |=  (7UL << ((10 - 8) * 4)); /* AF7 = USART1 */
}

/* ================================================================
 * USART1 초기화 - 115200, 8N1, DMA 수신 활성화
 * ================================================================ */
static void USART1_Init(void)
{
    /* USART1 클럭 활성화 */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* USART 비활성화 후 설정 */
    USART1->CR1 = 0;

    /* Baud rate 설정: BRR = fck / baudrate */
    USART1->BRR = HSI_FREQ / USART1_BAUDRATE;

    /* CR3: DMA 수신 활성화 */
    USART1->CR3 |= USART_CR3_DMAR;

    /* IDLE 인터럽트 활성화 (수신 완료 감지용) */
    USART1->CR1 |= USART_CR1_IDLEIE;

    /* USART 활성화: TE, RE, UE */
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    /* NVIC에서 USART1 인터럽트 활성화 */
    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);
}

/* ================================================================
 * GPDMA1 Channel 0 초기화 - USART1_RX 써큘러 모드
 *
 * STM32H552의 GPDMA는 링크드 리스트 기반이며,
 * 채널별로 독립적인 설정 레지스터를 가집니다.
 * ================================================================ */
static void GPDMA1_Ch0_Init(void)
{
    /* GPDMA1 클럭 활성화 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPDMA1EN;

    /* 채널 비활성화 */
    GPDMA1_Channel0->CCR &= ~DMA_CCR_EN;

    /* 전송 완료/에러 플래그 클리어 */
    GPDMA1_Channel0->CFCR = 0x00003F00UL;

    /*
     * CTR1 설정:
     *  - Source: 바이트(8비트), 고정 주소 (USART1->RDR)
     *  - Destination: 바이트(8비트), 증가 주소 (메모리)
     */
    GPDMA1_Channel0->CTR1 = 0;
    GPDMA1_Channel0->CTR1 |= (0UL << DMA_CTR1_SDW_LOG2_Pos)   /* Source 데이터 폭: 바이트 */
                            | (0UL << DMA_CTR1_DDW_LOG2_Pos)   /* Dest 데이터 폭: 바이트 */
                            | DMA_CTR1_DINC;                    /* Dest 주소 증가 */
    /* Source(Peripheral)는 고정 주소이므로 SINC 비트 설정하지 않음 */

    /*
     * CTR2 설정:
     *  - 요청 소스: USART1_RX (GPDMA request number)
     *  - TRIGM: DMA request 당 단일 전송
     */
    GPDMA1_Channel0->CTR2 = 0;
    /* USART1_RX DMA request 번호는 RM 참조 (일반적으로 request mux에서 설정) */
    GPDMA1_Channel0->CTR2 |= (24UL << DMA_CTR2_REQSEL_Pos)    /* USART1_RX request */
                            | (0UL << DMA_CTR2_TRIGM_Pos);     /* 블록 레벨 트리거 */

    /* Source 주소: USART1 RDR */
    GPDMA1_Channel0->CSAR = (uint32_t)&USART1->RDR;

    /* Destination 주소: DMA 수신 버퍼 */
    GPDMA1_Channel0->CDAR = (uint32_t)dma_rx_buf;

    /*
     * CBR1: 전송 바이트 수 설정
     * BNDT 필드에 버퍼 크기 설정
     */
    GPDMA1_Channel0->CBR1 = DMA_BUF_SIZE;

    /*
     * CLLR (Linked List Register):
     *  써큘러 모드 구현 - 자기 자신을 가리키는 링크드 리스트로 구현
     *  STM32H5 GPDMA에서는 CLLR의 LA 필드로 다음 LLI 주소를 설정하거나,
     *  UDA, USA, UB1, UCTR1, UCTR2 비트를 설정하여 레지스터를 갱신합니다.
     *
     *  써큘러 동작: CDAR(dest addr)과 CBR1(byte count)을 갱신하도록
     *  자기 자신의 CLLR 주소(0)로 링크합니다.
     */

    /*
     * 링크드 리스트 아이템(LLI) 구조체를 사용한 써큘러 구현
     */
    static __attribute__((aligned(32))) uint32_t lli_node[4];

    /* LLI 노드에 갱신할 레지스터 값 배열 */
    lli_node[0] = (uint32_t)dma_rx_buf;   /* CDAR: 목적지 주소 재설정 */
    lli_node[1] = DMA_BUF_SIZE;           /* CBR1: 전송 크기 재설정 */
    lli_node[2] = 0;                       /* 예약 */
    /* lli_node[3]: CLLR - 자기 자신을 가리켜서 써큘러 동작 */
    lli_node[3] = ((uint32_t)lli_node & 0xFFFFUL) >> 2  /* LA: 링크 주소 (하위 16비트, 워드 정렬) */
                | DMA_CLLR_UDA                           /* CDAR 갱신 */
                | DMA_CLLR_UB1;                          /* CBR1 갱신 */

    /* CLLR에 첫 LLI 노드 주소 설정 */
    GPDMA1_Channel0->CLLR = ((uint32_t)lli_node & 0xFFFFUL) >> 2
                           | DMA_CLLR_UDA
                           | DMA_CLLR_UB1;

    /*
     * Half Transfer / Transfer Complete 인터럽트 활성화
     * 써큘러 버퍼에서 HT와 TC를 활용하여 데이터 처리 타이밍을 잡습니다.
     */
    GPDMA1_Channel0->CCR |= DMA_CCR_HTIE   /* Half Transfer 인터럽트 */
                          | DMA_CCR_TCIE;   /* Transfer Complete 인터럽트 */

    /* NVIC에서 GPDMA1 Channel 0 인터럽트 활성화 */
    NVIC_SetPriority(GPDMA1_Channel0_IRQn, 0);
    NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);

    /* DMA 채널 활성화 */
    GPDMA1_Channel0->CCR |= DMA_CCR_EN;
}

/* ================================================================
 * DMA 써큘러 버퍼에서 새로 수신된 데이터를 읽어오는 함수
 *
 * DMA의 현재 쓰기 위치(CBNDT)와 이전 읽기 위치(old_pos)를 비교하여
 * 새로 도착한 데이터를 애플리케이션 버퍼에 복사합니다.
 *
 * 반환값: 복사된 바이트 수
 * ================================================================ */
static uint32_t USART1_DMA_ReadAvailable(uint8_t *dst, uint32_t dst_size)
{
    uint32_t new_pos;
    uint32_t length = 0;

    /*
     * CBNDT (CBR1의 BNDT 필드): 남은 전송 바이트 수
     * 현재 쓰기 위치 = 버퍼크기 - 남은바이트수
     */
    new_pos = DMA_BUF_SIZE - (GPDMA1_Channel0->CBR1 & 0xFFFFUL);

    if (new_pos != old_pos) {
        if (new_pos > old_pos) {
            /* 연속 영역: old_pos ~ new_pos */
            length = new_pos - old_pos;
            if (length > dst_size) {
                length = dst_size;
            }
            memcpy(dst, &dma_rx_buf[old_pos], length);
        } else {
            /* 랩어라운드: old_pos ~ 끝, 0 ~ new_pos */
            uint32_t len_to_end = DMA_BUF_SIZE - old_pos;
            uint32_t len_from_start = new_pos;
            length = len_to_end + len_from_start;

            if (length > dst_size) {
                length = dst_size;
            }

            if (len_to_end <= dst_size) {
                memcpy(dst, &dma_rx_buf[old_pos], len_to_end);
                uint32_t remaining = (length > len_to_end) ? (length - len_to_end) : 0;
                if (remaining > 0) {
                    memcpy(&dst[len_to_end], &dma_rx_buf[0], remaining);
                }
            }
        }
        old_pos = new_pos;
        if (old_pos >= DMA_BUF_SIZE) {
            old_pos = 0;
        }
    }

    return length;
}

/* ================================================================
 * USART1 IDLE 인터럽트 핸들러
 *
 * 수신 데이터 후 버스가 IDLE 상태가 되면 호출됩니다.
 * 가변 길이 데이터 수신 완료를 감지하는 데 사용합니다.
 * ================================================================ */
void USART1_IRQHandler(void)
{
    if (USART1->ISR & USART_ISR_IDLE) {
        /* IDLE 플래그 클리어 */
        USART1->ICR = USART_ICR_IDLECF;

        /* 수신된 데이터 처리 */
        uint32_t len = USART1_DMA_ReadAvailable(app_rx_buf, APP_BUF_SIZE);
        if (len > 0) {
            /*
             * 여기서 수신된 데이터를 처리합니다.
             * app_rx_buf에 len 바이트의 새 데이터가 들어있습니다.
             *
             * 예: 프로토콜 파싱, 링 버퍼에 저장, 플래그 설정 등
             */
            (void)len;  /* 사용자 처리 코드 추가 */
        }
    }
}

/* ================================================================
 * GPDMA1 Channel 0 인터럽트 핸들러
 *
 * Half Transfer(HT)와 Transfer Complete(TC)에서 호출됩니다.
 * 대량 연속 데이터 수신 시에도 데이터 유실 없이 처리합니다.
 * ================================================================ */
void GPDMA1_Channel0_IRQHandler(void)
{
    uint32_t flags = GPDMA1_Channel0->CSR;

    /* Half Transfer 완료 */
    if (flags & DMA_CSR_HTF) {
        GPDMA1_Channel0->CFCR = DMA_CFCR_HTF;  /* 플래그 클리어 */

        uint32_t len = USART1_DMA_ReadAvailable(app_rx_buf, APP_BUF_SIZE);
        if (len > 0) {
            /* 수신 데이터 처리 */
            (void)len;
        }
    }

    /* Transfer Complete */
    if (flags & DMA_CSR_TCF) {
        GPDMA1_Channel0->CFCR = DMA_CFCR_TCF;  /* 플래그 클리어 */

        uint32_t len = USART1_DMA_ReadAvailable(app_rx_buf, APP_BUF_SIZE);
        if (len > 0) {
            /* 수신 데이터 처리 */
            (void)len;
        }
    }

    /* Transfer Error */
    if (flags & DMA_CSR_DTEF) {
        GPDMA1_Channel0->CFCR = DMA_CFCR_DTEF;
        /* 에러 처리 */
    }
}

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    /* 시스템 초기화 */
    SystemClock_Config();
    GPIO_Init();
    USART1_Init();
    GPDMA1_Ch0_Init();

    while (1) {
        /*
         * 폴링 방식으로도 데이터를 확인할 수 있습니다.
         * 인터럽트와 병행 사용 시 동기화에 주의하세요.
         *
         * uint32_t len = USART1_DMA_ReadAvailable(app_rx_buf, APP_BUF_SIZE);
         * if (len > 0) {
         *     // 데이터 처리
         * }
         */

        __WFI();  /* 인터럽트 대기 (저전력) */
    }
}
