/**
 * app_wifi.h — ESP32-C3-WROOM AT 명령 제어 태스크
 * USART1(TTL)로 AT 명령을 보내 WiFi AP 접속 + 서버(TCP) 연결을 유지하고,
 * FPGA 서지 데이터를 서버로 전송한다. 연결이 끊기면 자동 재시도한다.
 */
#ifndef APP_INC_APP_WIFI_H_
#define APP_INC_APP_WIFI_H_

#include "cmsis_os2.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* fpgaTask가 서버 전송용 데이터를 넣는 큐 (App_WiFi_Task가 소비) */
extern osMessageQueueId_t qFpgaToWifi;
/* pcCommTask 등이 재접속/재설정을 지시하는 큐 */
extern osMessageQueueId_t qWifiCmd;

extern osMutexId_t   mutexWifiUart;
extern osSemaphoreId_t semUart1Event; /* USART1 IDLE/RxEvent 콜백에서 Release */

/* wifiTask 및 관련 RTOS 오브젝트 생성. app_config_init() 이후에 호출할 것 */
void App_WiFi_Init(void);

/* USART1 RxEventCallback(IDLE 라인 등)에서 호출: 수신 길이 저장 + 세마포어 Release */
void App_WiFi_OnUartRxEvent(uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* APP_INC_APP_WIFI_H_ */
