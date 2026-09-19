/**
 * app_freertos.h
 *
 * FreeRTOS 애플리케이션 진입점. 태스크/큐 생성 및 전역 큐 핸들 공유.
 * CubeMX가 생성한 freertos.c의 USER CODE Application 영역에서 App_FreeRTOS_Init()을
 * 1회 호출하면 된다 (docs/CubeMX_설정가이드.md §11 참고).
 */
#ifndef APP_FREERTOS_H
#define APP_FREERTOS_H

#include "cmsis_os2.h"
#include "net_config_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PCComm_Task -> Config_Task: SAVE 커맨드 시 NetConfig_t 전체를 큐에 담아 전달 */
extern osMessageQueueId_t g_cfgEventQueueId;

/* Config_Task -> ESP32_Task: 부팅 시 로드 완료 또는 저장 완료 후 WiFi (재)연결 요청 */
extern osMessageQueueId_t g_wifiEventQueueId;

/* FpgaLink_Task -> ESP32_Task: 측정값 서버 전송 요청 */
extern osMessageQueueId_t g_measQueueId;

void App_FreeRTOS_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FREERTOS_H */
