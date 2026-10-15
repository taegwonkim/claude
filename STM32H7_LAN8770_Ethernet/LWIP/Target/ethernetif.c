/**
  ******************************************************************************
  * @file    ethernetif.c
  * @brief   lwIP <-> STM32H7 ETH(RMII) <-> Microchip LAN8770 (100BASE-T1)
  ******************************************************************************
  * CubeMX 가 만든 LWIP/Target/ethernetif.c 를 이 파일로 교체합니다.
  * (CubeMX 의 LAN8742 전용 코드를 LAN8770 으로 바꾼 버전, NO_SYS = 1 기준)
  *
  * - Rx : zero-copy. lwIP 메모리 풀(RX_POOL)의 버퍼를 ETH DMA 에 직접 줍니다.
  * - Tx : HAL_ETH_Transmit() 블로킹 전송 (전송 완료까지 대기)
  * - 링크 : 100BASE-T1 은 오토네고가 없으므로 링크 Up 이면 MAC 을 100M/Full 로 고정
  *
  * 메모리 배치 (D2 SRAM, 링커 스크립트의 .lwip_sec 와 MPU 설정이 필요)
  *   0x30000000  Rx DMA 디스크립터
  *   0x30000080  Tx DMA 디스크립터
  *   0x30000100  Rx 버퍼 풀 (ETH_RX_BUFFER_CNT x 약 1.5KB)
  *   0x30004000  lwIP heap (LWIP_RAM_HEAP_POINTER, Tx 버퍼)
  ******************************************************************************
  */

#include "main.h"
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"
#include "ethernetif.h"
#include "lan8770_conf.h"
#include <string.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 *  설정
 * ------------------------------------------------------------------------ */
#define IFNAME0 's'
#define IFNAME1 't'

#define ETH_DMA_TRANSMIT_TIMEOUT    (20U)

#ifndef ETH_RX_BUFFER_SIZE
#define ETH_RX_BUFFER_SIZE          (1536U)  /* 32 의 배수, 최대 프레임 수용 */
#endif
#ifndef ETH_RX_BUFFER_CNT
#define ETH_RX_BUFFER_CNT           (8U)     /* ETH_RX_DESC_CNT ~ 2*ETH_RX_DESC_CNT */
#endif

/* ---------------------------------------------------------------------------
 *  Rx 버퍼 풀 / DMA 디스크립터
 * ------------------------------------------------------------------------ */
typedef enum
{
  RX_ALLOC_OK    = 0x00,
  RX_ALLOC_ERROR = 0x01
} RxAllocStatusTypeDef;

typedef struct
{
  struct pbuf_custom pbuf_custom;
  uint8_t buff[(ETH_RX_BUFFER_SIZE + 31U) & ~31U] __ALIGNED(32);
} RxBuff_t;

#if defined ( __ICCARM__ )
#pragma location=0x30000000
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
#pragma location=0x30000080
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];
#else /* GCC (STM32CubeIDE) */
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDescripSection")));
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDescripSection")));
#endif

LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Zero-copy RX PBUF pool");

#if defined ( __ICCARM__ )
#pragma location = 0x30000100
extern u8_t memp_memory_RX_POOL_base[];
#elif defined ( __ARMCC_VERSION )
__attribute__((section(".Rx_PoolSection"))) u8_t memp_memory_RX_POOL_base[];
#else /* GCC */
__attribute__((section(".Rx_PoolSection"))) extern u8_t memp_memory_RX_POOL_base[];
#endif

static volatile RxAllocStatusTypeDef RxAllocStatus;

/* ---------------------------------------------------------------------------
 *  핸들 / PHY 객체
 * ------------------------------------------------------------------------ */
ETH_HandleTypeDef heth;

static int32_t ETH_PHY_IO_Init(void);
static int32_t ETH_PHY_IO_DeInit(void);
static int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
static int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
static int32_t ETH_PHY_IO_GetTick(void);

lan8770_Object_t LAN8770;
static lan8770_IOCtx_t LAN8770_IOCtx =
{
  ETH_PHY_IO_Init,
  ETH_PHY_IO_DeInit,
  ETH_PHY_IO_WriteReg,
  ETH_PHY_IO_ReadReg,
  ETH_PHY_IO_GetTick
};

static uint8_t  phy_ready;        /* LAN8770_Init 성공 여부  */
static uint32_t phy_retry_tick;   /* 실패 시 재시도 타이머   */

static void pbuf_free_custom(struct pbuf *p);

/* ---------------------------------------------------------------------------
 *  PHY 하드웨어 리셋 (main.h 에 PHY_RST_Pin 이 있을 때만)
 * ------------------------------------------------------------------------ */
static void phy_hw_reset(void)
{
#if defined(PHY_RST_Pin) && defined(PHY_RST_GPIO_Port)
  HAL_GPIO_WritePin(PHY_RST_GPIO_Port, PHY_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(APP_PHY_RESET_LOW_MS);
  HAL_GPIO_WritePin(PHY_RST_GPIO_Port, PHY_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(APP_PHY_RESET_WAIT_MS);
#endif
}

static void phy_try_init(void)
{
  LAN8770.Is_Initialized = 0U;
  phy_ready = (LAN8770_Init(&LAN8770, APP_PHY_ADDR, APP_PHY_ROLE) == LAN8770_STATUS_OK) ? 1U : 0U;
  phy_retry_tick = HAL_GetTick();
}

uint8_t ethernetif_phy_ready(void)
{
  return phy_ready;
}

/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH)
*******************************************************************************/
static void low_level_init(struct netif *netif)
{
  static uint8_t macaddress[6] =
  {
    APP_MAC_ADDR0, APP_MAC_ADDR1, APP_MAC_ADDR2,
    APP_MAC_ADDR3, APP_MAC_ADDR4, APP_MAC_ADDR5
  };

  /* PHY 리셋을 먼저 : PHY 가 REF_CLK 를 출력하는 경우 HAL_ETH_Init 전에 클럭이 안정돼야 함 */
  phy_hw_reset();

  heth.Instance            = ETH;
  heth.Init.MACAddr        = macaddress;
  heth.Init.MediaInterface = HAL_ETH_RMII_MODE;   /* SYSCFG_PMCR.EPIS = RMII 도 HAL 이 설정 */
  heth.Init.RxDesc         = DMARxDscrTab;
  heth.Init.TxDesc         = DMATxDscrTab;
  heth.Init.RxBuffLen      = ETH_RX_BUFFER_SIZE;

  /* REF_CLK(50MHz) 가 들어오지 않으면 여기서 DMA 소프트 리셋 타임아웃으로 실패 */
  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }

  netif->hwaddr_len = ETH_HWADDR_LEN;
  memcpy(netif->hwaddr, macaddress, ETH_HWADDR_LEN);
  netif->mtu   = ETH_MAX_PAYLOAD;
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

  LWIP_MEMPOOL_INIT(RX_POOL);

  /* MDIO 버스 등록 → LAN8770 초기화 */
  LAN8770_RegisterBusIO(&LAN8770, &LAN8770_IOCtx);
  phy_try_init();

  if (phy_ready)
  {
    ethernet_link_check_state(netif);
  }
  else
  {
    /* PHY 를 못 찾아도 멈추지 않고, ethernet_link_check_state() 에서 재시도 */
    netif_set_link_down(netif);
    netif_set_down(netif);
  }
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  uint32_t i = 0U;
  struct pbuf *q;
  ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT];
  ETH_TxPacketConfigTypeDef TxConfig;

  LWIP_UNUSED_ARG(netif);

  memset(Txbuffer, 0, sizeof(Txbuffer));
  memset(&TxConfig, 0, sizeof(TxConfig));
  TxConfig.Attributes   = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl   = ETH_CRC_PAD_INSERT;

  for (q = p; q != NULL; q = q->next)
  {
    if (i >= ETH_TX_DESC_CNT)
    {
      return ERR_IF;
    }
    Txbuffer[i].buffer = q->payload;
    Txbuffer[i].len    = q->len;
    if (i > 0U)
    {
      Txbuffer[i - 1U].next = &Txbuffer[i];
    }
    i++;
  }

  TxConfig.Length   = p->tot_len;
  TxConfig.TxBuffer = Txbuffer;
  TxConfig.pData    = p;

  /* Tx 버퍼(lwIP heap)는 MPU 로 non-cacheable 이므로 캐시 clean 불필요.
   * heap 을 cacheable 영역에 둔다면 여기서 SCB_CleanDCache_by_Addr() 필요 */
  if (HAL_ETH_Transmit(&heth, &TxConfig, ETH_DMA_TRANSMIT_TIMEOUT) != HAL_OK)
  {
    return ERR_IF;
  }
  return ERR_OK;
}

static struct pbuf *low_level_input(struct netif *netif)
{
  struct pbuf *p = NULL;

  LWIP_UNUSED_ARG(netif);

  if (RxAllocStatus == RX_ALLOC_OK)
  {
    HAL_ETH_ReadData(&heth, (void **)&p);
  }
  return p;
}

/* main 루프(MX_LWIP_Process)에서 주기적으로 호출 */
void ethernetif_input(struct netif *netif)
{
  struct pbuf *p;

  do
  {
    p = low_level_input(netif);
    if (p != NULL)
    {
      if (netif->input(p, netif) != ERR_OK)
      {
        pbuf_free(p);
      }
    }
  } while (p != NULL);
}

err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  netif->hostname = "stm32h7-t1";
#endif

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;

#if LWIP_IPV4
  netif->output = etharp_output;
#endif
#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif
  netif->linkoutput = low_level_output;

  low_level_init(netif);

  return ERR_OK;
}

static void pbuf_free_custom(struct pbuf *p)
{
  struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;

  LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);

  /* 풀이 비어서 수신을 멈췄었다면 다시 허용 */
  if (RxAllocStatus == RX_ALLOC_ERROR)
  {
    RxAllocStatus = RX_ALLOC_OK;
  }
}

u32_t sys_now(void)
{
  return HAL_GetTick();
}

u32_t sys_jiffies(void)
{
  return HAL_GetTick();
}

/*******************************************************************************
                       Ethernet MSP (RMII 핀)
*******************************************************************************/
/*
   RMII_REF_CLK ----> PA1        RMII_TXD0  ----> PG13
   RMII_MDIO    ----> PA2        RMII_TXD1  ----> PB13
   RMII_CRS_DV  ----> PA7        RMII_TX_EN ----> PG11
   RMII_RXD0    ----> PC4        RMII_MDC   ----> PC1
   RMII_RXD1    ----> PC5

   CubeMX 가 stm32h7xx_hal_msp.c 에 HAL_ETH_MspInit 을 만들었다면 그쪽이
   우선합니다(여기는 __weak). 핀을 바꿨다면 CubeMX 쪽 또는 아래를 수정하세요.
*/
__weak void HAL_ETH_MspInit(ETH_HandleTypeDef *ethHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (ethHandle->Instance != ETH)
  {
    return;
  }

  __HAL_RCC_ETH1MAC_CLK_ENABLE();
  __HAL_RCC_ETH1TX_CLK_ENABLE();
  __HAL_RCC_ETH1RX_CLK_ENABLE();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;

  GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_13;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

/*******************************************************************************
                       PHY IO (MDIO / MDC)
*******************************************************************************/
static int32_t ETH_PHY_IO_Init(void)
{
  /* MDC 분주비를 HCLK 에 맞게 설정 (MDC <= 2.5MHz) */
  HAL_ETH_SetMDIOClockRange(&heth);
  return 0;
}

static int32_t ETH_PHY_IO_DeInit(void)
{
  return 0;
}

static int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
  return (HAL_ETH_ReadPHYRegister(&heth, DevAddr, RegAddr, pRegVal) == HAL_OK) ? 0 : -1;
}

static int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
  return (HAL_ETH_WritePHYRegister(&heth, DevAddr, RegAddr, RegVal) == HAL_OK) ? 0 : -1;
}

static int32_t ETH_PHY_IO_GetTick(void)
{
  return (int32_t)HAL_GetTick();
}

/*******************************************************************************
                       링크 상태 관리 (약 100ms 주기로 호출됨)
*******************************************************************************/
void ethernet_link_check_state(struct netif *netif)
{
  ETH_MACConfigTypeDef MACConf = {0};
  int32_t PHYLinkState;

  /* PHY 초기화가 아직 안 됐으면 주기적으로 재시도 */
  if (!phy_ready)
  {
    if ((HAL_GetTick() - phy_retry_tick) >= APP_PHY_RETRY_MS)
    {
      phy_try_init();
    }
    if (!phy_ready)
    {
      return;
    }
  }

  PHYLinkState = LAN8770_GetLinkState(&LAN8770);

  if (netif_is_link_up(netif) && (PHYLinkState <= LAN8770_STATUS_LINK_DOWN))
  {
    HAL_ETH_Stop(&heth);
    netif_set_down(netif);
    netif_set_link_down(netif);
  }
  else if (!netif_is_link_up(netif) && (PHYLinkState == LAN8770_STATUS_100MBITS_FULLDUPLEX))
  {
    /* 100BASE-T1 : 항상 100 Mbps / Full Duplex */
    HAL_ETH_GetMACConfig(&heth, &MACConf);
    MACConf.DuplexMode = ETH_FULLDUPLEX_MODE;
    MACConf.Speed      = ETH_SPEED_100M;
    HAL_ETH_SetMACConfig(&heth, &MACConf);
    HAL_ETH_Start(&heth);
    netif_set_up(netif);
    netif_set_link_up(netif);
  }
}

/*******************************************************************************
                       HAL ETH 콜백 (zero-copy Rx)
*******************************************************************************/
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
  struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);

  if (p != NULL)
  {
    *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
    p->custom_free_function = pbuf_free_custom;
    pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUFFER_SIZE);
  }
  else
  {
    RxAllocStatus = RX_ALLOC_ERROR;
    *buff = NULL;
  }
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
  struct pbuf **ppStart = (struct pbuf **)pStart;
  struct pbuf **ppEnd   = (struct pbuf **)pEnd;
  struct pbuf *p;

  p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
  p->next    = NULL;
  p->tot_len = 0;
  p->len     = Length;

  if (*ppStart == NULL)
  {
    *ppStart = p;
  }
  else
  {
    (*ppEnd)->next = p;
  }
  *ppEnd = p;

  for (p = *ppStart; p != NULL; p = p->next)
  {
    p->tot_len += Length;
  }

  /* Rx 버퍼를 cacheable 영역에 둔 경우를 위한 무효화 (non-cacheable 이면 무해) */
  SCB_InvalidateDCache_by_Addr((uint32_t *)buff, Length);
}

void HAL_ETH_TxFreeCallback(uint32_t *buff)
{
  /* 블로킹 전송(HAL_ETH_Transmit)만 사용하므로 HAL_ETH_ReleaseTxPacket()을
   * 부르지 않고, 따라서 이 콜백도 호출되지 않습니다. */
  LWIP_UNUSED_ARG(buff);
}
