/**
  ******************************************************************************
  * @file    usb_cdc.c
  * @brief   USB CDC(가상 COM 포트) 송수신 래퍼
  *
  *  [ 송신 버퍼를 두 개 쓰는 이유 ]
  *   CDC_Transmit_FS() 는 넘긴 버퍼를 복사하지 않고 주소만 USB 코어에 넘긴다.
  *   즉 함수가 리턴한 뒤에도 전송이 끝날 때까지 그 메모리를 건드리면 안 된다.
  *   64바이트 버퍼 두 개를 번갈아 쓰면, 다음 조각을 채우는 동안 직전 조각이
  *   전송 중이어도 서로 겹치지 않는다. (전송 중인 버퍼는 항상 최대 1개)
  *
  *  [ 연결이 없을 때 ]
  *   호스트가 장치를 열거하지 않았으면 송신을 즉시 포기한다. 그래야 USB
  *   케이블을 안 꽂은 상태에서도 RS485 쪽 동작이 느려지지 않는다.
  *
  *  [ 주기 리셋과 USB 의 궁합 ]
  *   소프트웨어 리셋이 걸리면 USB 장치가 사라졌다가 다시 나타난다. PC 의 COM
  *   포트도 사라졌다 다시 생기므로 대부분의 터미널은 포트를 닫아버린다.
  *   자동 재접속하는 터미널이나 스크립트를 쓰거나, 로그를 끊김 없이 받아야
  *   하면 RS485 쪽을 기준 채널로 쓰는 것이 낫다. (README 참고)
  ******************************************************************************
  */
#include "usb_cdc.h"

#if (USE_USB_CDC == 1U)

#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "usbd_core.h"
#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceFS;      /* USB_DEVICE/App/usb_device.c */
extern PCD_HandleTypeDef  hpcd_USB_OTG_FS;   /* USB_DEVICE/Target/usbd_conf.c */

#define USB_CDC_PACKET_SIZE   64U
#define USB_CDC_RX_BUF_SIZE   64U

/* 송신 : 64바이트 버퍼 2개를 번갈아 사용 */
static uint8_t  s_tx_buf[2][USB_CDC_PACKET_SIZE];
static uint8_t  s_tx_idx = 0U;

/* 수신 링버퍼 */
static volatile uint8_t  s_rx_buf[USB_CDC_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;

/* ------------------------------------------------------------------------- */

bool USB_CDC_IsReady(void)
{
  return (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED);
}

bool USB_CDC_WaitReady(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();

  while (!USB_CDC_IsReady())
  {
    if ((HAL_GetTick() - start) >= timeout_ms)
    {
      return false;              /* 케이블이 없거나 호스트가 응답하지 않음 */
    }
  }

  /* 열거가 끝나도 호스트 쪽에서 포트를 여는 데 시간이 조금 더 걸린다.
     이 여유가 없으면 부팅 배너 앞부분이 잘린다. */
  HAL_Delay(USB_CDC_READY_EXTRA_MS);
  return true;
}

void USB_CDC_Write(const uint8_t *data, uint16_t len)
{
  uint16_t offset = 0U;

  if ((data == NULL) || (len == 0U) || !USB_CDC_IsReady())
  {
    return;
  }

  while (offset < len)
  {
    uint16_t chunk = (uint16_t)(len - offset);
    uint8_t *buf;
    uint32_t start;

    if (chunk > USB_CDC_PACKET_SIZE)
    {
      chunk = USB_CDC_PACKET_SIZE;
    }

    /* 직전 조각이 아직 전송 중일 수 있으므로 다른 버퍼에 채운다 */
    buf = s_tx_buf[s_tx_idx];
    (void)memcpy(buf, &data[offset], chunk);
    s_tx_idx ^= 1U;

    /* 직전 전송이 끝나면 USBD_BUSY 가 아니게 된다 */
    start = HAL_GetTick();
    while (CDC_Transmit_FS(buf, chunk) == USBD_BUSY)
    {
      if (!USB_CDC_IsReady())
      {
        return;                  /* 도중에 케이블이 빠졌다 */
      }
      if ((HAL_GetTick() - start) >= USB_CDC_TX_TIMEOUT_MS)
      {
        return;                  /* 호스트가 안 읽어간다 - 버리고 진행 */
      }
    }

    offset = (uint16_t)(offset + chunk);
  }
}

bool USB_CDC_GetChar(uint8_t *ch)
{
  if (s_rx_head == s_rx_tail)
  {
    return false;
  }
  *ch = s_rx_buf[s_rx_tail];
  s_rx_tail = (uint16_t)((s_rx_tail + 1U) % USB_CDC_RX_BUF_SIZE);
  return true;
}

/**
  * @brief  usbd_cdc_if.c 의 CDC_Receive_FS() 에서 호출한다.
  * @note   USB 인터럽트 컨텍스트에서 불린다. 링버퍼에 넣기만 할 것.
  */
void USB_CDC_RxHandler(const uint8_t *data, uint32_t len)
{
  uint32_t i;

  for (i = 0U; i < len; i++)
  {
    uint16_t next = (uint16_t)((s_rx_head + 1U) % USB_CDC_RX_BUF_SIZE);
    if (next == s_rx_tail)
    {
      break;                     /* 가득 차면 버린다 */
    }
    s_rx_buf[s_rx_head] = data[i];
    s_rx_head = next;
  }
}

void USB_CDC_Flush(void)
{
  if (USB_CDC_IsReady())
  {
    HAL_Delay(USB_CDC_FLUSH_MS);
  }
}

void USB_CDC_Detach(void)
{
  (void)USBD_Stop(&hUsbDeviceFS);
  HAL_Delay(USB_CDC_DETACH_MS);
}

/**
  * @brief  USB OTG FS 글로벌 인터럽트
  * @note   CubeMX 가 생성한 stm32l5xx_it.c 에 같은 핸들러가 있으면 중복 정의가
  *         되므로 둘 중 하나만 남길 것. 이 프로젝트는 자체 stm32l5xx_it.c 를
  *         쓰므로 여기 둔다.
  *         핸들러 이름이 다르면 main.h 의 USB_CDC_IRQ_HANDLER 만 고치면 된다.
  */
void USB_CDC_IRQ_HANDLER(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

#endif /* USE_USB_CDC */
