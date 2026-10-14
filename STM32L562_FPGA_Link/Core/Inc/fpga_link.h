/**
  ******************************************************************************
  * @file    fpga_link.h
  * @brief   STM32L562CEU6 <-> FPGA(EP4CE10, Nios II) UART(TTL 3.3V) 링크
  *
  *  수신 : UART + DMA 순환(Circular) 버퍼 + IDLE 라인 감지
  *         (HAL_UARTEx_ReceiveToIdle_DMA) → 바이트마다 CPU 개입 없음, 메인루프가
  *         수백 ms 멈춰도(Wi-Fi AT 대기 등) 데이터 손실 없음
  *  송신 : 명령 큐 + DMA (Normal). 명령 사이 최소 간격(FPGA_CMD_MIN_GAP_MS) 보장
  ******************************************************************************
  */
#ifndef FPGA_LINK_H
#define FPGA_LINK_H

#include "stm32l5xx_hal.h"
#include "fpga_proto.h"

/* ============================ 하드웨어 설정 ============================== */
/* UART4 : PA0 = TX (→ FPGA UART1_RX), PA1 = RX (← FPGA UART1_TX), AF8
 *   USART1(PA9/10, ESP32) / USART2(PA2/3, 디버그) 와 겹치지 않는 포트.
 *   다른 핀을 쓰려면 아래만 바꾸면 된다. 예) USART3 : PB10/PB11, GPIO_AF7_USART3,
 *   DMA_REQUEST_USART3_RX/TX, USART3_IRQn  (CubeMX 에서 핀 사용 가능 여부 확인)        */
#define FPGA_UART_INSTANCE          UART4
#define FPGA_UART_IRQn              UART4_IRQn
#define FPGA_UART_IRQHandler        UART4_IRQHandler
#define FPGA_UART_CLK_ENABLE()      __HAL_RCC_UART4_CLK_ENABLE()
#define FPGA_UART_PERIPHCLK         RCC_PERIPHCLK_UART4
#define FPGA_UART_CLKSEL_FIELD      Uart4ClockSelection
#define FPGA_UART_CLKSOURCE         RCC_UART4CLKSOURCE_HSI      /* HSI16 고정 → SYSCLK 바뀌어도 보레이트 일정 */
#define FPGA_UART_BAUDRATE          115200U                     /* FPGA Platform Designer uart1 과 동일하게 */

#define FPGA_UART_GPIO_PORT         GPIOA
#define FPGA_UART_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define FPGA_UART_TX_PIN            GPIO_PIN_0
#define FPGA_UART_RX_PIN            GPIO_PIN_1
#define FPGA_UART_AF                GPIO_AF8_UART4

#define FPGA_DMA_CLK_ENABLE()       do { __HAL_RCC_DMAMUX1_CLK_ENABLE(); __HAL_RCC_DMA1_CLK_ENABLE(); } while (0)
#define FPGA_DMA_RX_CHANNEL         DMA1_Channel1
#define FPGA_DMA_RX_REQUEST         DMA_REQUEST_UART4_RX
#define FPGA_DMA_RX_IRQn            DMA1_Channel1_IRQn
#define FPGA_DMA_RX_IRQHandler      DMA1_Channel1_IRQHandler
#define FPGA_DMA_TX_CHANNEL         DMA1_Channel2
#define FPGA_DMA_TX_REQUEST         DMA_REQUEST_UART4_TX
#define FPGA_DMA_TX_IRQn            DMA1_Channel2_IRQn
#define FPGA_DMA_TX_IRQHandler      DMA1_Channel2_IRQHandler

#define FPGA_IRQ_PRIORITY           5U          /* SysTick(보통 0~15 중 15 또는 0) 보다 중요도 낮게 둬도 됨 */

/* ---- 핸드셰이크 핀 (선택) ----
 *   FPGA top.v : MCU_RX (FPGA 출력) : Nios 가 uart1 송신 전에 Low, 송신 후 High
 *                MCU_TX (FPGA 입력) : Nios PIO mcu_tx (현재 FPGA 펌웨어에서 미사용, LED_WIFI 표시)
 *   DMA 수신을 쓰면 MCU 는 언제나 수신 가능하므로 핸드셰이크는 필수가 아니다.          */
#define FPGA_USE_HANDSHAKE          1U
#define FPGA_TXREQ_GPIO_PORT        GPIOB       /* ← FPGA MCU_RX  (Low = FPGA 송신 중) */
#define FPGA_TXREQ_PIN              GPIO_PIN_1
#define FPGA_TXREQ_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define FPGA_READY_GPIO_PORT        GPIOA       /* → FPGA MCU_TX  (High = MCU 준비됨)  */
#define FPGA_READY_PIN              GPIO_PIN_8

/* ============================ 동작 설정 ================================== */
#define FPGA_RX_DMA_BUF_SIZE        512U        /* DMA 순환 버퍼 (115200bps 에서 약 44ms 분량) */
#define FPGA_CMD_MIN_GAP_MS         5U          /* 명령 프레임 사이 최소 간격 (Nios 가 한 패킷을
                                                   처리할 시간. 수정된 FPGA 펌웨어면 0 도 가능) */

/* CubeMX 가 같은 IRQ 핸들러 / HAL 콜백을 이미 만들었다면 0 으로 하고
 * 그 안에서 HAL_xxx_IRQHandler() / FPGA_Link_xxxHandler() 를 호출할 것     */
#define FPGA_LINK_DEFINE_IRQ_HANDLERS   1U
#define FPGA_LINK_DEFINE_HAL_CALLBACKS  1U

/* ============================ API ======================================== */
extern UART_HandleTypeDef hfpga_uart;

HAL_StatusTypeDef FPGA_Link_Init(void);
void              FPGA_Link_Process(void);                         /* 메인루프에서 주기적으로 호출 */

int  FPGA_Link_GetPacket(FPGA_Packet *pkt);                        /* 1 = 패킷 있음            */
int  FPGA_Link_SendCommand(const char *cmd);                       /* "START" → STX START CR LF */
int  FPGA_Link_TxIdle(void);                                       /* 1 = 큐 비고 송신 완료      */
uint8_t FPGA_Link_IsFpgaSending(void);                             /* 핸드셰이크 MCU_RX == Low   */
const FPGA_LinkStats *FPGA_Link_GetStats(void);

/* HAL 콜백에서 호출 (FPGA_LINK_DEFINE_HAL_CALLBACKS == 0 인 경우) */
void FPGA_Link_RxEventHandler(UART_HandleTypeDef *huart, uint16_t pos);
void FPGA_Link_TxCpltHandler(UART_HandleTypeDef *huart);
void FPGA_Link_ErrorHandler(UART_HandleTypeDef *huart);

#endif /* FPGA_LINK_H */
