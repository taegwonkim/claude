/**
 * usbd_cdc_if_hooks.c — CubeMX가 생성하는 USB_DEVICE/App/usbd_cdc_if.c 에 병합할 코드.
 *
 * 아래 두 지점을 CubeMX가 생성한 usbd_cdc_if.c의 해당 USER CODE 영역에 붙여넣는다.
 *
 * 1) 파일 상단 include 영역:
 *      #include "app_pccomm.h"
 *
 * 2) static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len) 함수 본문,
 *    "USBD_CDC_SetRxBuffer(...)"와 "USBD_CDC_ReceivePacket(...)" 사이(또는 직후)에:
 *
 *      App_PcComm_OnUsbRx(Buf, *Len);
 *
 *    CDC_Receive_FS()는 USB 인터럽트 컨텍스트(또는 USB 태스크 컨텍스트)에서 호출되므로
 *    App_PcComm_OnUsbRx() 내부에서는 반드시 non-blocking(osMessageQueuePut(..., 0))
 *    방식으로만 큐에 데이터를 넣는다 (app_pccomm.c 참조).
 *
 * 아래는 참고용 예시 발췌본이다(CubeMX 생성 골격 기준, 실제 버전에 따라
 * 시그니처가 약간 다를 수 있으니 생성된 파일에 맞춰 반영할 것).
 */

#if 0 /* 참고용 예시 — 실제로는 CubeMX가 생성한 usbd_cdc_if.c에 병합할 것 */

#include "app_pccomm.h"

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    /* USER CODE BEGIN 6 */
    App_PcComm_OnUsbRx(Buf, *Len);

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (USBD_OK);
    /* USER CODE END 6 */
}

#endif
