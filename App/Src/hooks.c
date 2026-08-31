/**
  ******************************************************************************
  * @file    hooks.c
  * @brief   HAL 콜백 → 애플리케이션 라우팅
  *
  *  이 파일은 HAL 의 __weak 콜백을 재정의한다.
  *  CubeMX 가 생성한 Core/Src 파일에는 같은 이름의 함수를 만들지 말 것.
  *
  *  FreeRTOS 훅(vApplicationStackOverflowHook / vApplicationMallocFailedHook)은
  *  CubeMX 가 Core/Src/freertos.c 에 생성하므로 여기서는 정의하지 않는다.
  *  → Integration/freertos_usercode.c 의 스니펫을 참고해 채워 넣을 것.
  ******************************************************************************
  */
#include "app_types.h"
#include "uart_link.h"
#include "fpga_link.h"
#include "usb_bridge.h"
#include "main.h"

/* ---------------------------------------------------------------------------
 * UART
 * -------------------------------------------------------------------------*/

/* HAL_UARTEx_ReceiveToIdle_DMA() 사용 시 Half / Complete / Idle 마다 호출됨 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  uartlink_on_rx_event(huart, Size);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  uartlink_on_tx_done(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  uartlink_on_error(huart);
}

/* ---------------------------------------------------------------------------
 * EXTI — FPGA 트리거 (PA1, 하강 에지)
 * -------------------------------------------------------------------------*/
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == FPGA_TRIG_Pin)
  {
    fpga_on_trigger_isr();
  }
}
