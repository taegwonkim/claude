/**
  ******************************************************************************
  * @file    fpga_link.c
  * @brief   STM32L562 <-> FPGA UART 링크 (UART4 + DMA1 CH1/CH2 + DMAMUX)
  *
  *  [수신 흐름]
  *   UART4_RX ─DMA(Circular)─▶ rx_dma_buf[512]
  *        HT(절반) / TC(끝) / IDLE(라인 1문자 동안 조용) 이벤트마다
  *        HAL_UARTEx_RxEventCallback(pos) → 새로 들어온 구간만 FPGA_Proto_Feed()
  *        → 완성된 프레임을 패킷 큐(8개)에 넣음 → 메인루프 FPGA_Link_GetPacket()
  *
  *  [송신 흐름]
  *   FPGA_Link_SendCommand() → 명령 큐(8개) → FPGA_Link_Process() 가
  *   이전 송신 완료 + FPGA_CMD_MIN_GAP_MS 경과 시 HAL_UART_Transmit_DMA()
  ******************************************************************************
  */
#include "fpga_link.h"
#include <string.h>

UART_HandleTypeDef hfpga_uart;
static DMA_HandleTypeDef hdma_fpga_rx;
static DMA_HandleTypeDef hdma_fpga_tx;

static uint8_t        rx_dma_buf[FPGA_RX_DMA_BUF_SIZE];
static uint16_t       rx_old_pos;
static FPGA_RxParser  rx_parser;
static FPGA_PktQueue  rx_queue;
static FPGA_LinkStats stats;

/* 송신 명령 큐 : main 에서만 넣고 main(Process) 에서만 꺼냄 */
typedef struct { uint16_t len; uint8_t data[FPGA_CMD_MAX_LEN]; } FPGA_TxFrame;
static FPGA_TxFrame   tx_queue[FPGA_CMD_QUEUE_LEN];
static uint32_t       tx_head, tx_tail;
static uint8_t        tx_dma_buf[FPGA_CMD_MAX_LEN];  /* DMA 가 읽는 동안 바뀌지 않는 버퍼 */
static volatile uint8_t  tx_busy;
static volatile uint32_t tx_done_tick;

static HAL_StatusTypeDef rx_start(void);

/* -------------------------------------------------------------------------- */
static void gpio_init(void)
{
  GPIO_InitTypeDef g = {0};

  FPGA_UART_GPIO_CLK_ENABLE();

  g.Pin       = FPGA_UART_TX_PIN | FPGA_UART_RX_PIN;
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = GPIO_PULLUP;            /* FPGA 컨피그 중(핀 Hi-Z)에도 RX 가 High(IDLE) 유지 */
  g.Speed     = GPIO_SPEED_FREQ_LOW;    /* 115200bps 에 충분, 링잉/EMI 감소 */
  g.Alternate = FPGA_UART_AF;
  HAL_GPIO_Init(FPGA_UART_GPIO_PORT, &g);

#if FPGA_USE_HANDSHAKE
  FPGA_TXREQ_GPIO_CLK_ENABLE();
  g.Pin       = FPGA_TXREQ_PIN;
  g.Mode      = GPIO_MODE_INPUT;
  g.Pull      = GPIO_PULLUP;
  g.Alternate = 0;
  HAL_GPIO_Init(FPGA_TXREQ_GPIO_PORT, &g);

  HAL_GPIO_WritePin(FPGA_READY_GPIO_PORT, FPGA_READY_PIN, GPIO_PIN_SET);
  g.Pin       = FPGA_READY_PIN;
  g.Mode      = GPIO_MODE_OUTPUT_PP;
  g.Pull      = GPIO_NOPULL;
  HAL_GPIO_Init(FPGA_READY_GPIO_PORT, &g);
#endif
}

static HAL_StatusTypeDef dma_init(void)
{
  FPGA_DMA_CLK_ENABLE();

  /* RX : Circular - 한 번 시작하면 멈추지 않고 계속 받는다 */
  hdma_fpga_rx.Instance                 = FPGA_DMA_RX_CHANNEL;
  hdma_fpga_rx.Init.Request             = FPGA_DMA_RX_REQUEST;
  hdma_fpga_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  hdma_fpga_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_fpga_rx.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_fpga_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_fpga_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  hdma_fpga_rx.Init.Mode                = DMA_CIRCULAR;
  hdma_fpga_rx.Init.Priority            = DMA_PRIORITY_HIGH;
  if (HAL_DMA_Init(&hdma_fpga_rx) != HAL_OK) return HAL_ERROR;
  __HAL_LINKDMA(&hfpga_uart, hdmarx, hdma_fpga_rx);

  /* TX : Normal - 프레임 1개씩 */
  hdma_fpga_tx.Instance                 = FPGA_DMA_TX_CHANNEL;
  hdma_fpga_tx.Init.Request             = FPGA_DMA_TX_REQUEST;
  hdma_fpga_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdma_fpga_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_fpga_tx.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_fpga_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_fpga_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  hdma_fpga_tx.Init.Mode                = DMA_NORMAL;
  hdma_fpga_tx.Init.Priority            = DMA_PRIORITY_MEDIUM;
  if (HAL_DMA_Init(&hdma_fpga_tx) != HAL_OK) return HAL_ERROR;
  __HAL_LINKDMA(&hfpga_uart, hdmatx, hdma_fpga_tx);

  HAL_NVIC_SetPriority(FPGA_DMA_RX_IRQn, FPGA_IRQ_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(FPGA_DMA_RX_IRQn);
  HAL_NVIC_SetPriority(FPGA_DMA_TX_IRQn, FPGA_IRQ_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(FPGA_DMA_TX_IRQn);
  return HAL_OK;
}

HAL_StatusTypeDef FPGA_Link_Init(void)
{
  RCC_PeriphCLKInitTypeDef clk = {0};

  memset(&stats, 0, sizeof(stats));
  FPGA_Proto_ParserReset(&rx_parser);
  rx_queue.head = rx_queue.tail = 0U;
  tx_head = tx_tail = 0U;
  tx_busy = 0U;
  tx_done_tick = HAL_GetTick();

  /* UART 커널 클럭 = HSI16 (16MHz / 115200 = 138.9 → BRR 139, 오차 -0.08%) */
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == 0U)
  {
    __HAL_RCC_HSI_ENABLE();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == 0U) {}
  }
  clk.PeriphClockSelection   = FPGA_UART_PERIPHCLK;
  clk.FPGA_UART_CLKSEL_FIELD = FPGA_UART_CLKSOURCE;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) return HAL_ERROR;
  FPGA_UART_CLK_ENABLE();

  gpio_init();

  hfpga_uart.Instance                    = FPGA_UART_INSTANCE;
  hfpga_uart.Init.BaudRate               = FPGA_UART_BAUDRATE;
  hfpga_uart.Init.WordLength             = UART_WORDLENGTH_8B;
  hfpga_uart.Init.StopBits               = UART_STOPBITS_1;
  hfpga_uart.Init.Parity                 = UART_PARITY_NONE;
  hfpga_uart.Init.Mode                   = UART_MODE_TX_RX;
  hfpga_uart.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
  hfpga_uart.Init.OverSampling           = UART_OVERSAMPLING_16;
  hfpga_uart.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;   /* 3샘플 다수결 + 노이즈 검출 */
  hfpga_uart.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
  hfpga_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hfpga_uart) != HAL_OK) return HAL_ERROR;       /* MspInit 은 UART4 를 건드리지 않음 */
  if (HAL_UARTEx_DisableFifoMode(&hfpga_uart) != HAL_OK) return HAL_ERROR;

  if (dma_init() != HAL_OK) return HAL_ERROR;

  HAL_NVIC_SetPriority(FPGA_UART_IRQn, FPGA_IRQ_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(FPGA_UART_IRQn);

  return rx_start();
}

/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef rx_start(void)
{
  rx_old_pos = 0U;
  FPGA_Proto_ParserReset(&rx_parser);
  stats.rx_restarts++;
  /* Circular DMA + IDLE : 반환 후에는 HT/TC/IDLE 마다 RxEventCallback 호출 */
  return HAL_UARTEx_ReceiveToIdle_DMA(&hfpga_uart, rx_dma_buf, FPGA_RX_DMA_BUF_SIZE);
}

void FPGA_Link_RxEventHandler(UART_HandleTypeDef *huart, uint16_t pos)
{
  if (huart != &hfpga_uart) return;

  /* pos = DMA 가 지금까지 쓴 위치 (1..SIZE). 지난 위치부터 새 구간만 처리 */
  if (pos != rx_old_pos)
  {
    if (pos > rx_old_pos)
    {
      FPGA_Proto_Feed(&rx_parser, &rx_queue, &rx_dma_buf[rx_old_pos], pos - rx_old_pos, &stats);
    }
    else
    {
      FPGA_Proto_Feed(&rx_parser, &rx_queue, &rx_dma_buf[rx_old_pos],
                      FPGA_RX_DMA_BUF_SIZE - rx_old_pos, &stats);
      FPGA_Proto_Feed(&rx_parser, &rx_queue, &rx_dma_buf[0], pos, &stats);
    }
    rx_old_pos = (pos >= FPGA_RX_DMA_BUF_SIZE) ? 0U : pos;
  }
}

void FPGA_Link_ErrorHandler(UART_HandleTypeDef *huart)
{
  if (huart != &hfpga_uart) return;

  stats.uart_errors++;
  /* FE/NE 는 DMA 수신이 계속되지만, ORE/DMA 에러는 HAL 이 수신을 중단시킨다 → 재시작 */
  if (huart->RxState == HAL_UART_STATE_READY)
  {
    (void)rx_start();
  }
}

void FPGA_Link_TxCpltHandler(UART_HandleTypeDef *huart)
{
  if (huart != &hfpga_uart) return;

  tx_done_tick = HAL_GetTick();       /* TC = 마지막 STOP 비트까지 핀에서 나간 시점 */
  tx_busy = 0U;
}

/* -------------------------------------------------------------------------- */
int FPGA_Link_GetPacket(FPGA_Packet *pkt)
{
  return FPGA_Proto_QueuePop(&rx_queue, pkt);
}

int FPGA_Link_SendCommand(const char *cmd)
{
  uint32_t next = (tx_head + 1U) & (FPGA_CMD_QUEUE_LEN - 1U);
  int n;

  if (next == tx_tail)
  {
    stats.tx_queue_full++;
    return -1;
  }
  n = FPGA_Proto_BuildCommand(tx_queue[tx_head].data, FPGA_CMD_MAX_LEN, cmd);
  if (n < 0) return -1;
  tx_queue[tx_head].len = (uint16_t)n;
  tx_head = next;

  FPGA_Link_Process();                  /* 보낼 수 있으면 바로 시작 */
  return 0;
}

int FPGA_Link_TxIdle(void)
{
  return (tx_head == tx_tail) && (tx_busy == 0U);
}

void FPGA_Link_Process(void)
{
  /* 1) 수신이 어떤 이유로 멈춰 있으면 재시작 (안전망) */
  if (hfpga_uart.RxState == HAL_UART_STATE_READY)
  {
    (void)rx_start();
  }

  /* 2) 송신 큐 : 이전 프레임 완료 + 최소 간격 경과 시 다음 프레임 */
  if ((tx_busy == 0U) && (tx_head != tx_tail) &&
      ((HAL_GetTick() - tx_done_tick) >= FPGA_CMD_MIN_GAP_MS))
  {
    uint16_t len = tx_queue[tx_tail].len;

    memcpy(tx_dma_buf, tx_queue[tx_tail].data, len);
    tx_tail = (tx_tail + 1U) & (FPGA_CMD_QUEUE_LEN - 1U);
    tx_busy = 1U;
    if (HAL_UART_Transmit_DMA(&hfpga_uart, tx_dma_buf, len) != HAL_OK)
    {
      tx_busy = 0U;
    }
    else
    {
      stats.tx_frames++;
    }
  }
}

uint8_t FPGA_Link_IsFpgaSending(void)
{
#if FPGA_USE_HANDSHAKE
  return (HAL_GPIO_ReadPin(FPGA_TXREQ_GPIO_PORT, FPGA_TXREQ_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
#else
  return 0U;
#endif
}

const FPGA_LinkStats *FPGA_Link_GetStats(void)
{
  return &stats;
}

/* ============================ IRQ / HAL 콜백 ============================= */
#if FPGA_LINK_DEFINE_IRQ_HANDLERS
void FPGA_UART_IRQHandler(void)    { HAL_UART_IRQHandler(&hfpga_uart); }
void FPGA_DMA_RX_IRQHandler(void)  { HAL_DMA_IRQHandler(&hdma_fpga_rx); }
void FPGA_DMA_TX_IRQHandler(void)  { HAL_DMA_IRQHandler(&hdma_fpga_tx); }
#endif

#if FPGA_LINK_DEFINE_HAL_CALLBACKS
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  FPGA_Link_RxEventHandler(huart, Size);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  FPGA_Link_TxCpltHandler(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  FPGA_Link_ErrorHandler(huart);
}
#endif
