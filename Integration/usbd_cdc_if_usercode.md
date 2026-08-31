# `USB_DEVICE/App/usbd_cdc_if.c` USER CODE 스니펫

## 1) `USER CODE BEGIN INCLUDE`

```c
/* USER CODE BEGIN INCLUDE */
#include "usb_bridge.h"
/* USER CODE END INCLUDE */
```

## 2) `CDC_Receive_FS()` — `USER CODE BEGIN 6`

수신 바이트를 애플리케이션 큐(`qUsbRx`)로 넘기는 한 줄만 추가합니다.
이 함수는 **USB 인터럽트 컨텍스트**에서 호출되므로
`usbbridge_rx_from_isr()` 은 타임아웃 0 으로만 큐에 넣습니다.

```c
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  usbbridge_rx_from_isr(Buf, *Len);

  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
  /* USER CODE END 6 */
}
```

## 3) `CDC_Transmit_FS()`

수정하지 않습니다. `App/Src/usb_bridge.c` 의 `usb_tx_task()` 가
`USBD_BUSY` 를 보면 5 ms 간격으로 최대 20회 재시도합니다.

## 4) 버퍼 크기

`usbd_cdc_if.c` 상단의 아래 두 매크로를 확인하십시오 (CubeMX CDC 설정과 동일).

```c
#define APP_RX_DATA_SIZE  512
#define APP_TX_DATA_SIZE  512
```
