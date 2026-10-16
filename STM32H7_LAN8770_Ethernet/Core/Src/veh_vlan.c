/**
  ******************************************************************************
  * @file    veh_vlan.c
  * @brief   VLAN 태그 송수신 + 네트워크 모니터
  ******************************************************************************
  * 모니터는 아무것도 송신하지 않고, 받은 프레임의 VLAN ID / EtherType /
  * 송신자 MAC 을 세어서 주기적으로 보여줍니다. ECU 를 모를 때 첫 단계로 씁니다.
  ******************************************************************************
  */

#include "main.h"
#include "veh_vlan.h"
#include "veh_frame.h"
#include "veh_log.h"
#include "vehicle_net_conf.h"
#include "lwip_hooks.h"
#include "ethernetif.h"
#include "veh_ecu.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ethernet.h"
#include <string.h>

typedef struct { uint16_t key; uint32_t count; } kv16_t;
typedef struct { uint8_t mac[6]; uint16_t vid; uint32_t count; } mac_entry_t;

static veh_vlan_counters_t cnt;
static uint32_t    mon_tick;
#if APP_NETMON_ENABLE
static kv16_t      vlans[APP_NETMON_MAX_VLANS];
static kv16_t      etypes[APP_NETMON_MAX_ETHERTYPES];
static mac_entry_t macs[APP_NETMON_MAX_MACS];
static uint32_t    vlan_overflow, etype_overflow, mac_overflow;
#endif

/* ---------------------------------------------------------------------------
 *  lwIP 훅
 * ------------------------------------------------------------------------ */
s32_t veh_vlan_set_hook(struct netif *netif, struct pbuf *p,
                        const struct eth_addr *src, const struct eth_addr *dst,
                        u16_t eth_type)
{
  LWIP_UNUSED_ARG(netif);
  LWIP_UNUSED_ARG(p);
  LWIP_UNUSED_ARG(src);
  LWIP_UNUSED_ARG(dst);
  LWIP_UNUSED_ARG(eth_type);
#if APP_VLAN_ENABLE
  return (s32_t)veh_vlan_tci(APP_VLAN_PRIORITY, APP_VLAN_ID);
#else
  return -1;
#endif
}

int veh_vlan_check_hook(struct netif *netif, const struct eth_hdr *hdr,
                        const struct eth_vlan_hdr *vlan)
{
  LWIP_UNUSED_ARG(netif);
  LWIP_UNUSED_ARG(hdr);
#if APP_VLAN_ENABLE
  if (VLAN_ID(vlan) == APP_VLAN_ID)
  {
    return 1;
  }
#else
  LWIP_UNUSED_ARG(vlan);
#endif
  cnt.rx_vlan_dropped++;
  return 0;
}

/* ---------------------------------------------------------------------------
 *  통계
 * ------------------------------------------------------------------------ */
#if APP_NETMON_ENABLE
static void kv_add(kv16_t *tab, uint32_t n, uint16_t key, uint32_t *overflow)
{
  uint32_t i;

  for (i = 0; i < n; i++)
  {
    if ((tab[i].count != 0U) && (tab[i].key == key))
    {
      tab[i].count++;
      return;
    }
  }
  for (i = 0; i < n; i++)
  {
    if (tab[i].count == 0U)
    {
      tab[i].key   = key;
      tab[i].count = 1U;
      return;
    }
  }
  (*overflow)++;
}

static void mac_add(const uint8_t *mac, uint16_t vid)
{
  uint32_t i;

  for (i = 0; i < APP_NETMON_MAX_MACS; i++)
  {
    if ((macs[i].count != 0U) && (memcmp(macs[i].mac, mac, 6) == 0))
    {
      macs[i].count++;
      macs[i].vid = vid;
      return;
    }
  }
  for (i = 0; i < APP_NETMON_MAX_MACS; i++)
  {
    if (macs[i].count == 0U)
    {
      memcpy(macs[i].mac, mac, 6);
      macs[i].vid   = vid;
      macs[i].count = 1U;
      return;
    }
  }
  mac_overflow++;
}
#endif /* APP_NETMON_ENABLE */

/* ---------------------------------------------------------------------------
 *  ethernetif.c 훅 (ethernetif.c 의 __weak 기본 구현을 대체)
 * ------------------------------------------------------------------------ */
/* 모든 수신 프레임이 lwIP 로 가기 전에 호출됨. 0 을 돌려주면 버림 */
uint8_t ethernetif_rx_hook(struct pbuf *p)
{
  veh_frame_info_t fi;

  if (veh_frame_parse((const uint8_t *)p->payload, p->len, &fi) != 0)
  {
    return 1U;   /* lwIP 가 알아서 버림 */
  }

  cnt.rx_frames++;
  if (fi.tagged)
  {
    cnt.rx_tagged++;
  }

#if APP_NETMON_ENABLE
  kv_add(vlans, APP_NETMON_MAX_VLANS, fi.vid, &vlan_overflow);
  kv_add(etypes, APP_NETMON_MAX_ETHERTYPES, fi.ethertype, &etype_overflow);
  mac_add(fi.src, fi.vid);
#endif

#if APP_ECU_TABLE_ENABLE
  if ((fi.ethertype == VEH_ETHTYPE_IPV4) && (p->len > fi.header_len))
  {
    veh_ipv4_info_t ip;

    if (veh_ipv4_parse((const uint8_t *)p->payload + fi.header_len,
                       (uint32_t)p->len - fi.header_len, &ip) == 0)
    {
      veh_ecu_on_frame(&fi, &ip, veh_time_us());
    }
  }
#endif

#if APP_VLAN_ENABLE && !APP_VLAN_ACCEPT_UNTAGGED
  if (!fi.tagged)
  {
    cnt.rx_untagged_dropped++;
    return 0U;
  }
#endif
  return 1U;
}

/* HAL_ETH_Init 직후 호출됨 : MAC 수신 필터 설정 */
void ethernetif_post_init_hook(ETH_HandleTypeDef *heth)
{
  ETH_MACFilterConfigTypeDef f;

  if (HAL_ETH_GetMACFilterConfig(heth, &f) != HAL_OK)
  {
    return;
  }
#if APP_SOMEIP_ENABLE
  f.PassAllMulticast = ENABLE;     /* SOME/IP-SD 멀티캐스트 수신 */
#endif
#if APP_NETMON_PROMISCUOUS
  f.PromiscuousMode  = ENABLE;     /* 다른 MAC 으로 가는 프레임도 수신 */
#endif
  (void)HAL_ETH_SetMACFilterConfig(heth, &f);
}

/* ---------------------------------------------------------------------------
 *  공개 함수
 * ------------------------------------------------------------------------ */
void veh_vlan_init(void)
{
#if APP_VLAN_ENABLE
  VLOG("[VLAN] tagging ON : VID=%u PCP=%u, untagged rx %s\r\n",
       (unsigned)APP_VLAN_ID, (unsigned)APP_VLAN_PRIORITY,
       APP_VLAN_ACCEPT_UNTAGGED ? "accepted" : "dropped");
#else
  VLOG("[VLAN] tagging OFF (tagged frames are counted but not processed)\r\n");
#endif
#if APP_NETMON_ENABLE
  VLOG("[MON ] monitor ON, promiscuous=%u, report every %lus\r\n",
       (unsigned)APP_NETMON_PROMISCUOUS, (unsigned long)(APP_NETMON_PERIOD_MS / 1000U));
#endif
  mon_tick = HAL_GetTick();
}

const veh_vlan_counters_t *veh_vlan_counters(void)
{
  return &cnt;
}

#if APP_NETMON_ENABLE
static void print_vid(uint16_t vid)
{
  if (vid == VEH_VLAN_NONE)
  {
    VLOG("untagged");
  }
  else
  {
    VLOG("VID %4u", (unsigned)vid);
  }
}

#endif

void veh_netmon_print(void)
{
#if APP_NETMON_ENABLE
  uint32_t i;
#endif

  VLOG("[MON ] frames=%lu tagged=%lu vlan_drop=%lu untag_drop=%lu\r\n",
       (unsigned long)cnt.rx_frames, (unsigned long)cnt.rx_tagged,
       (unsigned long)cnt.rx_vlan_dropped, (unsigned long)cnt.rx_untagged_dropped);

#if APP_NETMON_ENABLE
  for (i = 0; i < APP_NETMON_MAX_VLANS; i++)
  {
    if (vlans[i].count != 0U)
    {
      VLOG("[MON ]   ");
      print_vid(vlans[i].key);
      VLOG("  : %lu\r\n", (unsigned long)vlans[i].count);
    }
  }
  for (i = 0; i < APP_NETMON_MAX_ETHERTYPES; i++)
  {
    if (etypes[i].count != 0U)
    {
      VLOG("[MON ]   type 0x%04X %-10s : %lu\r\n", (unsigned)etypes[i].key,
           veh_ethertype_name(etypes[i].key), (unsigned long)etypes[i].count);
    }
  }
  for (i = 0; i < APP_NETMON_MAX_MACS; i++)
  {
    if (macs[i].count != 0U)
    {
      const uint8_t *m = macs[i].mac;
      VLOG("[MON ]   src %02X:%02X:%02X:%02X:%02X:%02X (", m[0], m[1], m[2], m[3], m[4], m[5]);
      print_vid(macs[i].vid);
      VLOG(") : %lu\r\n", (unsigned long)macs[i].count);
    }
  }
  if ((vlan_overflow | etype_overflow | mac_overflow) != 0U)
  {
    VLOG("[MON ]   (table full: vlan+%lu type+%lu mac+%lu)\r\n", (unsigned long)vlan_overflow,
         (unsigned long)etype_overflow, (unsigned long)mac_overflow);
  }
#endif
}

void veh_netmon_process(void)
{
#if APP_NETMON_ENABLE
  if ((HAL_GetTick() - mon_tick) >= APP_NETMON_PERIOD_MS)
  {
    mon_tick = HAL_GetTick();
    veh_netmon_print();
  }
#endif
}
