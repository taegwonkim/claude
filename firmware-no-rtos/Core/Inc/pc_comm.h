/**
 * pc_comm.h
 *
 * PC와의 시리얼 커맨드/데이터 인터페이스. USART3(HW UART)와 USB(CDC, Virtual COM)를
 * 동시에 입력 채널로 받고, 출력(설정 응답 + 측정값 미러)은 두 채널 모두로 보낸다.
 * 프레임 포맷: STX(0x02) + Data1,Data2,...,DataN + CR(0x0D) + LF(0x0A) (docs/프로토콜_명세.md §1).
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

/** super-loop 매 반복 호출(논블로킹): USART3/USB에 도착한 완성된 라인을 모두 처리한다. */
void PcComm_Poll(void);

/**
 * @brief csv_payload(콤마로 이미 join된 필드들, 예: "OK", "ERR,INVALID_IP", "DATA,1,1234,567")를
 *        STX + csv_payload + CR + LF로 감싸 USART3와 USB CDC 양쪽으로 전송한다.
 *        다른 모듈(fpga_link, esp32_at 등)에서도 측정값/이벤트 알림 출력을 위해 호출한다.
 */
void PcComm_BroadcastFrame(const char *csv_payload);

#ifdef __cplusplus
}
#endif

#endif /* PC_COMM_H */
