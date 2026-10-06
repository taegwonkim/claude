/**
 * app_it_callbacks.c
 *
 * HAL 표준 weak 콜백(HAL_UARTEx_RxEventCallback, HAL_GPIO_EXTI_Callback,
 * HAL_RTCEx_WakeUpTimerEventCallback)의 유일한 프로젝트 전역 구현. 각 드라이버 모듈로
 * 이벤트를 분배(dispatch)한다. (프로젝트 전체에서 이 콜백들은 한 곳에만 정의될 수 있으므로,
 * 드라이버별로 중복 정의하지 않고 이 파일에서 모아 호출한다.)
 */
#include "esp32_at.h"
#include "fpga_link.h"
#include "pc_comm.h"
#include "rtc_wakeup.h"

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    Esp32_OnUartRxEvent(huart, Size);
    FpgaLink_OnUartRxEvent(huart, Size);
    PcComm_OnUartRxEvent(huart, Size);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    FpgaLink_OnExti(GPIO_Pin);
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    (void)hrtc;
    RtcWakeup_OnWakeupTimerEvent(); /* 반환하지 않음(NVIC_SystemReset()) */
}
