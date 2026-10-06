/**
 * app_main.h
 *
 * RTOS 미사용(super-loop) 애플리케이션 진입점. app_freertos.h/App_FreeRTOS_Init()에 대응하며,
 * 4개의 FreeRTOS 태스크(FPGA_Task/ESP32_Task/PCComm_Task/Config_Task) 대신 각 모듈의
 * *_Poll() 함수를 main()의 while(1) 루프에서 매 반복 순차 호출하는 협력형(cooperative)
 * 스케줄링 모델을 사용한다.
 *
 * main()에서:
 *   App_Init();       // HAL/페리퍼럴 초기화 이후, 1회
 *   while (1) {
 *       App_Run();     // 매 반복
 *   }
 */
#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdbool.h>
#include "net_config_store.h"
#include "measurement_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ESP32 하드리셋+프로브까지 포함한 초기화. main()에서 페리퍼럴(HAL) 초기화 이후 1회 호출. */
void App_Init(void);

/** super-loop 매 반복 호출: FPGA/ESP32/PC통신/설정저장 폴링 + LED 하트비트. */
void App_Run(void);

/**
 * @brief FpgaLink_Poll()이 측정값을 Esp32_Poll()(서버 TCP 전송)에 전달하기 위한 큐.
 *        고정 크기(APP_MEAS_QUEUE_LEN) 원형 버퍼, 가득 차면 false(드롭) — RTOS 버전의
 *        osMessageQueuePut(..., timeout=0)과 동일한 정책.
 */
bool App_PushMeasurement(const MeasurementMsg_t *msg);

/**
 * @brief PcComm의 SAVE 처리가 Config_Poll()에 플래시 저장을 요청한다. 단일 슬롯이라
 *        이전 요청이 아직 처리되지 않았으면 false(RTOS 버전의 ERR,QUEUE_FULL에 대응).
 */
bool App_RequestConfigSave(const NetConfig_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
