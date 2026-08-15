/**
 * status_led.h
 *
 * LED_RUN(PC13, 동작 하트비트)과 LED_WIFI(PC14, 서버 TCP 연결 표시) 제어.
 * CubeMX 핀 라벨 "LED_RUN"/"LED_WIFI"로 main.h에 생성되는 GPIO_Port/Pin 매크로를 사용한다.
 */
#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED_RUN 하트비트 갱신. 내부적으로 경과 시간(HAL_GetTick())을 확인해
 *        약 500ms 간격(1Hz 토글)으로만 실제 토글한다. 호출 주기가 그보다 짧아도 무방하므로
 *        main()의 super-loop(App_Run())에서 매 반복 호출하면 된다.
 */
void StatusLed_HeartbeatTick(void);

/**
 * @brief LED_WIFI를 갱신한다. on=true: 서버 TCP 연결됨(점등), false: 그 외(소등).
 *        Esp32_Poll()이 링크 상태 변화 시 호출한다.
 */
void StatusLed_SetWifi(bool on);

#ifdef __cplusplus
}
#endif

#endif /* STATUS_LED_H */
