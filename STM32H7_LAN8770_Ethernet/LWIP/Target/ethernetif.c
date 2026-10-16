/**
  ******************************************************************************
  * @file    ethernetif.c
  * @brief   lwIP <-> STM32H7 ETH(RMII) <-> Microchip LAN8770 (100BASE-T1)
  ******************************************************************************
  * CubeMX 의 LAN8742 용 ethernetif.c 를 LAN8770 용으로 바꾼 파일입니다 (NO_SYS = 1).
  * ⚠ CubeMX 에서 코드를 다시 생성하면 이 파일이 LAN8742 버전으로 덮어써집니다.
  *   재생성 후에는  git checkout -- LWIP/Target/ethernetif.c  로 되돌리세요.
  *
  * - Rx : zero-copy. lwIP 메모리 풀(RX_POOL)의 버퍼를 ETH DMA 에 직접 줍니다.
  * - Tx : HAL_ETH_Transmit() 블로킹 전송 (전송 완료까지 대기)
  * - 링크 : 100BASE-T1 은 오토네고가 없으므로 링크 Up 이면 MAC 을 100M/Full 로 고정
  *
  * 메모리 배치 (D2 SRAM, 링커 스크립트의 .lwip_sec 와 MPU 설정이 필요)
  *   0x30000000  Rx DMA 디스크립터 (ETH_RX_DESC_CNT 16 x 24 B, 0x200 예약)
  *   0x30000200  Tx DMA 디스크립터 (ETH_TX_DESC_CNT  8 x 24 B, 0x200 예약)
  *   0x30000400  Rx 버퍼 풀 (ETH_RX_BUFFER_CNT 32 x 1568 B ≈ 49 KB)
  *   0x30010000  lwIP heap (LWIP_RAM_HEAP_POINTER, MEM_SIZE 32 KB, Tx 버퍼)
  *
  * 수신 손실 집계 : ethernetif_stats() (하드웨어 카운터 + 소프트웨어 카운터)
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
#define ETH_RX_BUFFER_CNT           (2U * ETH_RX_DESC_CNT)  /* 32 : 디스크립터 수 ~ 2배 */
#endif

#if (ETH_RX_BUFFER_CNT < ETH_RX_DESC_CNT) || (ETH_RX_BUFFER_CNT > (2U * ETH_RX_DESC_CNT))
#error "ETH_RX_BUFFER_CNT must be between ETH_RX_DESC_CNT and 2 * ETH_RX_DESC_CNT"
#endif
#if (ETH_RX_DESC_CNT * 24U) > 0x200U
#error "Rx descriptors exceed 0x200 bytes reserved at 0x30000000 (see linker .lwip_sec)"
#endif
#if (ETH_TX_DESC_CNT * 24U) > 0x200U
#error "Tx descriptors exceed 0x200 bytes reserved at 0x30000200 (see linker .lwip_sec)"
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
#pragma location=0x30000200
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];
#else /* GCC (STM32CubeIDE) */
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDecripSection")));
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDecripSection")));
#endif

LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Zero-copy RX PBUF pool");

#if defined ( __ICCARM__ )
#pragma location = 0x30000400
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

/* 송수신 / 손실 통계 */
static ethernetif_stats_t eth_stats;
static uint32_t mmc_crc_prev, mmc_align_prev;
static uint32_t phy_retry_tick;   /* 실패 시 재시도 타이머   */

static void pbuf_free_custom(struct pbuf *p);

/* ---------------------------------------------------------------------------
 *  확장 훅 (veh_vlan.c 가 강한 심볼로 대체. 없으면 아래 기본 동작)
 * ------------------------------------------------------------------------ */
/* 수신 프레임을 lwIP 로 넘기기 전에 호출. 0 이면 버림 */
__weak uint8_t ethernetif_rx_hook(struct pbuf *p)
{
  LWIP_UNUSED_ARG(p);
  return 1U;
}

/* HAL_ETH_Init 직후 호출 (MAC 필터 등 추가 설정) */
__weak void ethernetif_post_init_hook(ETH_HandleTypeDef *ethHandle)
{
  LWIP_UNUSED_ARG(ethHandle);
}

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
  ethernetif_post_init_hook(&heth);

  netif->hwaddr_len = ETH_HWADDR_LEN;
  memcpy(netif->hwaddr, macaddress, ETH_HWADDR_LEN);
  netif->mtu   = ETH_MAX_PAYLOAD;
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
#if LWIP_IGMP
  netif->flags |= NETIF_FLAG_IGMP;   /* SOME/IP-SD 멀티캐스트 그룹 가입 */
#endif

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
      eth_stats.tx_errors++;      /* pbuf 조각이 Tx 디스크립터보다 많음 */
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
    eth_stats.tx_errors++;
    return ERR_IF;
  }
  eth_stats.tx_frames++;
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
      eth_stats.rx_frames++;
      if (!ethernetif_rx_hook(p))
      {
        pbuf_free(p);
      }
      else if (netif->input(p, netif) != ERR_OK)
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
*/
void HAL_ETH_MspInit(ETH_HandleTypeDef *ethHandle)
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

void HAL_ETH_MspDeInit(ETH_HandleTypeDef *ethHandle)
{
  if (ethHandle->Instance != ETH)
  {
    return;
  }

  __HAL_RCC_ETH1MAC_CLK_DISABLE();
  __HAL_RCC_ETH1TX_CLK_DISABLE();
  __HAL_RCC_ETH1RX_CLK_DISABLE();

  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7);
  HAL_GPIO_DeInit(GPIOB, GPIO_PIN_13);
  HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
  HAL_GPIO_DeInit(GPIOG, GPIO_PIN_11 | GPIO_PIN_13);
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
/*******************************************************************************
                       송수신 / 손실 통계
*******************************************************************************/
/* 하드웨어 카운터를 읽어 누적 (약 100 ms 마다 ethernet_link_check_state 에서 호출)
 *  - DMACMFCR   : 빈 Rx 디스크립터가 없어 DMA 가 버린 프레임 (읽으면 0, 11 bit)
 *  - MTLRQMPOCR : Rx FIFO overflow / missed (읽으면 0, 11 bit)
 *  - MMC        : CRC / alignment 오류 (free-running 32 bit → 차이로 계산) */
void ethernetif_poll_hw_counters(void)
{
  uint32_t v, crc, align;

  v = ETH->DMACMFCR;
  eth_stats.rx_dma_missed += (v & ETH_DMACMFCR_MFC);
  if ((v & ETH_DMACMFCR_MFCO) != 0U)
  {
    eth_stats.counter_overflow = 1U;
  }

  v = ETH->MTLRQMPOCR;
  eth_stats.rx_fifo_overflow += (v & ETH_MTLRQMPOCR_OVFPKTCNT) >> ETH_MTLRQMPOCR_OVFPKTCNT_Pos;
  eth_stats.rx_fifo_missed   += (v & ETH_MTLRQMPOCR_MISPKTCNT) >> ETH_MTLRQMPOCR_MISPKTCNT_Pos;
  if ((v & (ETH_MTLRQMPOCR_OVFCNTOVF | ETH_MTLRQMPOCR_MISCNTOVF)) != 0U)
  {
    eth_stats.counter_overflow = 1U;
  }

  crc   = ETH->MMCRCRCEPR;
  align = ETH->MMCRAEPR;
  eth_stats.rx_crc_errors   += crc - mmc_crc_prev;
  eth_stats.rx_align_errors += align - mmc_align_prev;
  mmc_crc_prev   = crc;
  mmc_align_prev = align;
}

const ethernetif_stats_t *ethernetif_stats(void)
{
  return &eth_stats;
}

uint32_t ethernetif_stats_rx_lost(void)
{
  return eth_stats.rx_dma_missed + eth_stats.rx_fifo_overflow + eth_stats.rx_fifo_missed;
}

void ethernetif_stats_reset(void)
{
  (void)ETH->DMACMFCR;              /* 읽어서 하드웨어 카운터 비우기 */
  (void)ETH->MTLRQMPOCR;
  mmc_crc_prev   = ETH->MMCRCRCEPR;
  mmc_align_prev = ETH->MMCRAEPR;
  memset(&eth_stats, 0, sizeof(eth_stats));
}

void ethernet_link_check_state(struct netif *netif)
{
  ETH_MACConfigTypeDef MACConf = {0};
  int32_t PHYLinkState;

  ethernetif_poll_hw_counters();

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
    if (RxAllocStatus == RX_ALLOC_OK)
    {
      eth_stats.rx_pool_empty++;  /* lwIP 가 Rx 버퍼를 아직 돌려주지 않아 풀이 비었음 */
    }
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
