/**
 * pc_comm.h
 *
 * PC와의 시리얼 커맨드/데이터 인터페이스. USART3(HW UART)와 USB(CDC, Virtual COM)를
 * 동시에 입력 채널로 받고, 출력(설정 응답 + 측정값 미러)은 두 채널 모두로 보낸다.
 */
#ifndef PC_COMM_H
#define PC_COMM_H

#include <stdint.h>
#include "usart.h"
#include "net_config_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USART3 idle-DMA 수신 시작 등 초기화. initial_cfg는 부팅 시 플래시에서 읽은(혹은 기본값)
 *        현재 설정으로, GET CONFIG/STATUS 응답 및 SET 커맨드의 베이스가 된다.
 */
void PcComm_Init(const NetConfig_t *initial_cfg);

/** HAL_UARTEx_RxEventCallback에서 호출 (내부에서 USART3 인스턴스인지 확인). */
void PcComm_OnUartRxEvent(UART_HandleTypeDef *huart, uint16_t pos);

/** USB_DEVICE/App/usbd_cdc_if.c의 CDC_Receive_FS()에서 호출 (USER CODE BEGIN 6). */
void PC_Comm_FeedUSB(uint8_t *buf, uint32_t len);

/** PCComm_Task 본체 (app_freertos.c에서 osThreadNew로 생성). */
void PcComm_Task(void *argument);

/**
 * @brief 널 종단 문자열 line(개행 미포함)을 "\r\n"을 붙여 USART3와 USB CDC 양쪽으로 전송.
 *        다른 태스크(FPGA_Task 등)에서도 측정값 미러 출력을 위해 호출한다.
 */
void PcComm_BroadcastLine(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* PC_COMM_H */
