#include "status_led.h"
#include "main.h" /* LED_RUN_GPIO_Port/Pin, LED_WIFI_GPIO_Port/Pin */

#define STATUS_LED_HEARTBEAT_PERIOD_MS (500U) /* 1Hz 토글 = 500ms마다 상태 반전 */

static uint32_t s_lastToggleTick = 0U;

void StatusLed_HeartbeatTick(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - s_lastToggleTick) >= STATUS_LED_HEARTBEAT_PERIOD_MS) {
        s_lastToggleTick = now;
        HAL_GPIO_TogglePin(LED_RUN_GPIO_Port, LED_RUN_Pin);
    }
}

void StatusLed_SetWifi(bool on)
{
    HAL_GPIO_WritePin(LED_WIFI_GPIO_Port, LED_WIFI_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
