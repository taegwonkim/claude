/**
 * STM32L552 USART1 DMA Circular Buffer Reception Example
 *
 * USART1 핀 설정:
 *   - PA9  : USART1_TX (AF7)
 *   - PA10 : USART1_RX (AF7)
 *
 * DMA: DMA1 Channel 1 + DMAMUX (USART1_RX)
 * Baud rate: 115200
 *
 * STM32L552는 DMAMUX가 있는 표준 DMA를 사용하며,
 * CCR 레지스터의 CIRC 비트로 네이티브 써큘러 모드를 지원합니다.
 */

#include "stm32l5xx.h"
#include <stdint.h>
#include <string.h>

/* ---- 설정 ---- */
#define DMA_BUF_SIZE     256    /* DMA 써큘러 버퍼 크기 (2의 거듭제곱 권장) */
#define APP_BUF_SIZE     512    /* 애플리케이션 처리 버퍼 크기 */
#define USART1_BAUDRATE  115200
#define MSI_FREQ         4000000UL  /* STM32L552 기본 MSI 클럭 4 MHz */

/* ---- 버퍼 ---- */
static uint8_t dma_rx_buf[DMA_BUF_SIZE];   /* DMA가 직접 쓰는 써큘러 버퍼 */
static uint8_t app_rx_buf[APP_BUF_SIZE];   /* 애플리케이션 처리용 버퍼 */
static volatile uint32_t old_pos = 0;       /* 마지막으로 읽은 위치 */

/* ---- 함수 선언 ---- */
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void USART1_Init(void);
static void DMA1_Ch1_Init(void);
static uint32_t USART1_DMA_ReadAvailable(uint8_t *dst, uint32_t dst_size);
static void ProcessReceivedData(uint8_t *data, uint32_t len);

/* ================================================================
 * 시스템 클럭 설정
 * 기본 MSI 4 MHz 사용 (필요 시 PLL로 최대 110 MHz 가능)
 * ================================================================ */
static void SystemClock_Config(void)
{
    /*
     * 고속 동작이 필요한 경우 아래와 같이 PLL 설정:
     *
     * // MSI를 4 MHz로 설정 (기본값)
     * RCC->CR |= RCC_CR_MSION;
     * while (!(RCC->CR & RCC_CR_MSIRDY));
     *
     * // PLL: MSI(4MHz) / 1 * 55 / 2 = 110 MHz
     * RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_MSI
     *              | (0 << RCC_PLLCFGR_PLLM_Pos)    // M = 1
     *              | (55 << RCC_PLLCFGR_PLLN_Pos)    // N = 55
     *              | (0 << RCC_PLLCFGR_PLLR_Pos)     // R = 2
     *              | RCC_PLLCFGR_PLLREN;
     *
     * RCC->CR |= RCC_CR_PLLON;
     * while (!(RCC->CR & RCC_CR_PLLRDY));
     *
     * // Flash latency 설정 후 SYSCLK = PLL
     * FLASH->ACR |= FLASH_ACR_LATENCY_5WS;
     * RCC->CFGR |= RCC_CFGR_SW_PLL;
     * while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
     */
}

/* ================================================================
 * GPIO 초기화 - USART1 TX(PA9), RX(PA10) AF7
 * ================================================================ */
static void GPIO_Init(void)
{
    /* GPIOA 클럭 활성화 */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /* PA9 (TX): AF7, Push-Pull, Very High Speed */
    GPIOA->MODER   &= ~(3UL << (9 * 2));
    GPIOA->MODER   |=  (2UL << (9 * 2));   /* Alternate Function */
    GPIOA->OSPEEDR |=  (3UL << (9 * 2));   /* Very High Speed */
    GPIOA->AFR[1]  &= ~(0xFUL << ((9 - 8) * 4));
    GPIOA->AFR[1]  |=  (7UL << ((9 - 8) * 4));  /* AF7 = USART1 */

    /* PA10 (RX): AF7, Pull-up */
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
    /* USART1 클럭 활성화 (APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* USART 비활성화 후 설정 */
    USART1->CR1 = 0;

    /* Baud rate: BRR = fck / baudrate */
    USART1->BRR = MSI_FREQ / USART1_BAUDRATE;

    /* CR3: DMA 수신 활성화 */
    USART1->CR3 |= USART_CR3_DMAR;

    /* IDLE 라인 인터럽트 활성화 (가변 길이 수신 완료 감지) */
    USART1->CR1 |= USART_CR1_IDLEIE;

    /* USART 활성화: TE, RE, UE */
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    /* NVIC */
    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);
}

/* ================================================================
 * DMA1 Channel 1 초기화 - USART1_RX, 써큘러 모드
 *
 * STM32L552의 DMA는 DMAMUX로 요청 소스를 연결하고,
 * CCR.CIRC 비트로 네이티브 써큘러 모드를 지원합니다.
 * (H5 시리즈의 GPDMA 링크드 리스트 방식과 다름)
 * ================================================================ */
static void DMA1_Ch1_Init(void)
{
    /* DMA1 및 DMAMUX1 클럭 활성화 */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN;

    /* 채널 비활성화 */
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;

    /*
     * DMAMUX: DMA1 Channel 1에 USART1_RX 요청 연결
     * USART1_RX = Request ID 24 (RM0438 Table 91 참조)
     */
    DMAMUX1_Channel0->CCR = 0;
    DMAMUX1_Channel0->CCR = (24UL << DMAMUX_CxCR_DMAREQ_ID_Pos);

    /*
     * DMA CCR 설정:
     *  - MINC  : 메모리 주소 증가
     *  - CIRC  : 써큘러 모드 ★ (버퍼 끝 도달 시 자동으로 처음으로 돌아감)
     *  - TCIE  : Transfer Complete 인터럽트
     *  - HTIE  : Half Transfer 인터럽트
     *  - DIR=0 : Peripheral → Memory (수신)
     *  - MSIZE=00, PSIZE=00 : 바이트(8비트) 전송
     */
    DMA1_Channel1->CCR = DMA_CCR_MINC     /* 메모리 주소 자동 증가 */
                       | DMA_CCR_CIRC     /* 써큘러 모드 */
                       | DMA_CCR_TCIE     /* Transfer Complete 인터럽트 */
                       | DMA_CCR_HTIE;    /* Half Transfer 인터럽트 */

    /* 전송 바이트 수 */
    DMA1_Channel1->CNDTR = DMA_BUF_SIZE;

    /* 주변장치 주소: USART1 RDR (수신 데이터 레지스터) */
    DMA1_Channel1->CPAR = (uint32_t)&USART1->RDR;

    /* 메모리 주소: DMA 수신 버퍼 */
    DMA1_Channel1->CMAR = (uint32_t)dma_rx_buf;

    /* NVIC: DMA1 Channel 1 인터럽트 */
    NVIC_SetPriority(DMA1_Channel1_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    /* DMA 채널 활성화 */
    DMA1_Channel1->CCR |= DMA_CCR_EN;
}

/* ================================================================
 * DMA 써큘러 버퍼에서 새로 수신된 데이터를 읽어오는 함수
 *
 * CNDTR(남은 전송 바이트 수)로 현재 DMA 쓰기 위치를 계산하고,
 * 이전 읽기 위치(old_pos)와 비교하여 새 데이터만 복사합니다.
 *
 * 반환값: 복사된 바이트 수
 * ================================================================ */
static uint32_t USART1_DMA_ReadAvailable(uint8_t *dst, uint32_t dst_size)
{
    uint32_t new_pos;
    uint32_t length = 0;

    /*
     * 현재 DMA 쓰기 위치 계산
     * CNDTR = 남은 전송 바이트 수 (써큘러 모드에서 자동 리로드됨)
     * 쓰기 위치 = 버퍼크기 - CNDTR
     */
    new_pos = DMA_BUF_SIZE - DMA1_Channel1->CNDTR;

    if (new_pos != old_pos) {
        if (new_pos > old_pos) {
            /* 연속 영역: old_pos ~ new_pos */
            length = new_pos - old_pos;
            if (length > dst_size) {
                length = dst_size;
            }
            memcpy(dst, &dma_rx_buf[old_pos], length);
        } else {
            /* 랩어라운드: old_pos ~ 끝 + 0 ~ new_pos */
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
 * 수신 데이터 처리 함수 (사용자 구현)
 * ================================================================ */
static void ProcessReceivedData(uint8_t *data, uint32_t len)
{
    /*
     * 여기에 수신 데이터 처리 로직을 구현합니다.
     * 예: 프로토콜 파싱, 링 버퍼 저장, 응답 전송 등
     */
    (void)data;
    (void)len;
}

/* ================================================================
 * USART1 IDLE 인터럽트 핸들러
 *
 * 데이터 수신 후 버스가 IDLE 상태가 되면 호출됩니다.
 * 가변 길이 패킷의 수신 완료를 감지하는 핵심 메커니즘입니다.
 * ================================================================ */
void USART1_IRQHandler(void)
{
    if (USART1->ISR & USART_ISR_IDLE) {
        /* IDLE 플래그 클리어 */
        USART1->ICR = USART_ICR_IDLECF;

        /* 새로 수신된 데이터 읽기 */
        uint32_t len = USART1_DMA_ReadAvailable(app_rx_buf, APP_BUF_SIZE);
        if (len > 0) {
            ProcessReceivedData(app_rx_buf, len);
        }
    }
}

/* ================================================================
 * DMA1 Channel 1 인터럽트 핸들러
 *
 * Half Transfer(HT): 버퍼 절반이 채워졌을 때
 * Transfer Complete(TC): 버퍼 전체가 채워졌을 때 (써큘러 → 자동 리셋)
 *
 * 대량 연속 데이터 수신 시 IDLE 인터럽트 없이도 처리 가능
 * ================================================================ */
void DMA1_Channel1_IRQHandler(void)
{
    /* Half Transfer */
    if (DMA1->ISR & DMA_ISR_HTIF1) {
        DMA1->IFCR = DMA_IFCR_CHTIF1;  /* 플래그 클리어 */

        uint32_t len = USART1_DMA_ReadAvailable(app_rx_buf, APP_BUF_SIZE);
        if (len > 0) {
            ProcessReceivedData(app_rx_buf, len);
        }
    }

    /* Transfer Complete */
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        DMA1->IFCR = DMA_IFCR_CTCIF1;  /* 플래그 클리어 */

        uint32_t len = USART1_DMA_ReadAvailable(app_rx_buf, APP_BUF_SIZE);
        if (len > 0) {
            ProcessReceivedData(app_rx_buf, len);
        }
    }

    /* Transfer Error */
    if (DMA1->ISR & DMA_ISR_TEIF1) {
        DMA1->IFCR = DMA_IFCR_CTEIF1;
        /* 에러 처리: DMA 재초기화 등 */
    }
}

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    SystemClock_Config();
    GPIO_Init();
    USART1_Init();
    DMA1_Ch1_Init();

    while (1) {
        /*
         * 메인 루프에서 폴링으로도 확인 가능:
         *
         * uint32_t len = USART1_DMA_ReadAvailable(app_rx_buf, APP_BUF_SIZE);
         * if (len > 0) {
         *     ProcessReceivedData(app_rx_buf, len);
         * }
         */

        __WFI();  /* 인터럽트 대기 (저전력) */
    }
}
