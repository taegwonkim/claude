/**
  ******************************************************************************
  * @file    rs485.c
  * @brief   USART3 RS485(Driver Enable) 송수신
  *
  *  RS485 는 반이중(half-duplex) 이라 송신할 때만 드라이버를 켜야 한다.
  *
  *   - RS485_USE_HW_DE = 1 : USART3 의 DE 기능(HAL_RS485Ex_Init)을 쓴다.
  *     하드웨어가 첫 바이트 전에 DE 를 올리고 마지막 바이트 뒤에 내려준다.
  *     → 타이밍 오차가 없어 가장 안전하다. 핀은 PB14(USART3_DE, AF7).
  *
  *   - RS485_USE_HW_DE = 0 : 일반 GPIO 를 직접 토글한다.
  *     DE 핀이 USART3_DE 로 못 가는 보드(이미 다른 기능이 물린 경우)용.
  *     송신 전 HIGH, TC(Transmission Complete) 확인 후 LOW.
  *
  *  트랜시버 배선(예: MAX3485 / SP3485 / SN65HVD3082)
  *     RO  -> PB11 (USART3_RX)
  *     DI  -> PB10 (USART3_TX)
  *     DE  -> PB14
  *     /RE -> PB14 와 같이 묶으면 송신 중 자기 에코가 안 들어온다(권장)
  *            GND 로 내리면 항상 수신 = 자기 송신이 그대로 되돌아온다
  *            (아래 코드에서 송신 후 수신 버퍼를 비워 에코를 버린다)
  *     A/B 선로 양 끝단에는 120옴 종단 저항.
  ******************************************************************************
  */
#include "rs485.h"

#if (USE_RS485 == 1U)

#include <string.h>

UART_HandleTypeDef huart_rs485;

/* ---- 수신 링버퍼 ---------------------------------------------------------- */
#define RS485_RX_BUF_SIZE   64U

static volatile uint8_t  s_rx_buf[RS485_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;
static uint8_t           s_rx_byte = 0U;

/* ---- 내부 함수 ------------------------------------------------------------ */
#if (RS485_USE_HW_DE == 0U)
static inline void RS485_DriverEnable(void)
{
  HAL_GPIO_WritePin(RS485_DE_GPIO_PORT, RS485_DE_PIN, GPIO_PIN_SET);
}
static inline void RS485_DriverDisable(void)
{
  HAL_GPIO_WritePin(RS485_DE_GPIO_PORT, RS485_DE_PIN, GPIO_PIN_RESET);
}
#endif

/**
  * @brief  USART3 를 115200-8-N-1 RS485 모드로 초기화한다.
  * @note   GPIO/클럭/NVIC 는 HAL_UART_MspInit()(stm32l5xx_hal_msp.c) 에서 처리.
  */
void RS485_Init(void)
{
  huart_rs485.Instance                    = RS485_UART_INSTANCE;
  huart_rs485.Init.BaudRate               = RS485_BAUDRATE;
  huart_rs485.Init.WordLength             = UART_WORDLENGTH_8B;
  huart_rs485.Init.StopBits               = UART_STOPBITS_1;
  huart_rs485.Init.Parity                 = UART_PARITY_NONE;
  huart_rs485.Init.Mode                   = UART_MODE_TX_RX;
  huart_rs485.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
  huart_rs485.Init.OverSampling           = UART_OVERSAMPLING_16;
  huart_rs485.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
  huart_rs485.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
  huart_rs485.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

#if (RS485_USE_HW_DE == 1U)
  /* RS485 Driver Enable 모드로 초기화 (CR3.DEM = 1) */
  if (HAL_RS485Ex_Init(&huart_rs485,
                       UART_DE_POLARITY_HIGH,
                       RS485_DE_ASSERT_TIME,
                       RS485_DE_DEASSERT_TIME) != HAL_OK)
  {
    Error_Handler();
  }
#else
  if (HAL_UART_Init(&huart_rs485) != HAL_OK)
  {
    Error_Handler();
  }
  RS485_DriverDisable();      /* 기본은 수신 상태 */
#endif

  if (HAL_UARTEx_SetTxFifoThreshold(&huart_rs485, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart_rs485, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart_rs485) != HAL_OK)
  {
    Error_Handler();
  }

#if (USE_COMM_CMD == 1U)
  s_rx_head = 0U;
  s_rx_tail = 0U;
  (void)HAL_UART_Receive_IT(&huart_rs485, &s_rx_byte, 1U);
#endif
}

/**
  * @brief  마지막 바이트가 선로에 완전히 실릴 때까지 대기한다.
  * @note   리셋 직전에 반드시 호출 (안 하면 메시지 꼬리가 잘린다).
  */
void RS485_WaitTxDone(void)
{
  uint32_t start = HAL_GetTick();

  while (__HAL_UART_GET_FLAG(&huart_rs485, UART_FLAG_TC) == 0U)
  {
    if ((HAL_GetTick() - start) > RS485_TX_TIMEOUT_MS)
    {
      break;      /* 선로 이상 시 무한 대기 방지 */
    }
  }
}

void RS485_Write(const uint8_t *data, uint16_t len)
{
  if ((data == NULL) || (len == 0U))
  {
    return;
  }

#if (RS485_USE_HW_DE == 0U)
  RS485_DriverEnable();
#endif

  /* HAL_UART_Transmit 은 블로킹이며 마지막에 TC 를 기다린 뒤 리턴한다 */
  (void)HAL_UART_Transmit(&huart_rs485, (uint8_t *)data, len, RS485_TX_TIMEOUT_MS);
  RS485_WaitTxDone();

#if (RS485_USE_HW_DE == 0U)
  RS485_DriverDisable();
#endif

#if (USE_COMM_CMD == 1U)
  /* /RE 를 GND 로 고정한 배선이면 방금 보낸 내용이 그대로 되돌아온다.
     그 에코를 명령으로 오인하지 않도록 수신 버퍼를 비운다. */
  s_rx_head = s_rx_tail;
#endif
}

bool RS485_GetChar(uint8_t *ch)
{
#if (USE_COMM_CMD == 1U)
  if (s_rx_head == s_rx_tail)
  {
    return false;
  }
  *ch = s_rx_buf[s_rx_tail];
  s_rx_tail = (uint16_t)((s_rx_tail + 1U) % RS485_RX_BUF_SIZE);
  return true;
#else
  (void)ch;
  return false;
#endif
}

#if (USE_COMM_CMD == 1U)
/**
  * @brief  1바이트 수신 완료 콜백 (USART3_IRQHandler -> HAL_UART_IRQHandler 경유)
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint16_t next;

  if (huart->Instance != RS485_UART_INSTANCE)
  {
    return;
  }

  next = (uint16_t)((s_rx_head + 1U) % RS485_RX_BUF_SIZE);
  if (next != s_rx_tail)          /* 가득 차면 버린다 */
  {
    s_rx_buf[s_rx_head] = s_rx_byte;
    s_rx_head = next;
  }

  (void)HAL_UART_Receive_IT(&huart_rs485, &s_rx_byte, 1U);
}

/**
  * @brief  수신 에러(프레이밍/노이즈/오버런) 시 수신을 다시 살린다.
  * @note   RS485 선로는 아무도 송신하지 않을 때 플로팅이라 에러가 잘 난다.
  *         여기서 재무장하지 않으면 수신이 영구히 멈춘다.
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != RS485_UART_INSTANCE)
  {
    return;
  }

  __HAL_UART_CLEAR_OREFLAG(huart);
  __HAL_UART_CLEAR_NEFLAG(huart);
  __HAL_UART_CLEAR_FEFLAG(huart);
  __HAL_UART_CLEAR_PEFLAG(huart);

  (void)HAL_UART_Receive_IT(&huart_rs485, &s_rx_byte, 1U);
}
#endif /* USE_COMM_CMD */

#endif /* USE_RS485 */
