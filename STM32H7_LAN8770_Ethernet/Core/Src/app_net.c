/**
  ******************************************************************************
  * @file    app_net.c
  * @brief   LAN8770 데모 애플리케이션
  *          - UDP 에코 서버 (포트 APP_UDP_ECHO_PORT) : 받은 데이터를 그대로 돌려줌
  *          - ICMP ping 응답 (lwIP 기본 기능)
  *          - 주기적 상태 로그 : PHY 주소/역할, 링크, SQI, IP 주소
  ******************************************************************************
  */

#include "main.h"
#include "app_net.h"
#include "lan8770_conf.h"
#include "ethernetif.h"
#include "veh_vlan.h"
#include "app_someip.h"
#include "veh_ecu.h"
#include "veh_log.h"
#include "vehicle_net_conf.h"
#include "lwip/netif.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include <stdio.h>

extern struct netif gnetif;          /* CubeMX 가 LWIP/App/lwip.c 에 정의 */

#if APP_USE_UART_LOG
extern UART_HandleTypeDef huart3;    /* CubeMX 가 main.c 에 정의 */

/* STM32CubeIDE 의 syscalls.c : _write() → __io_putchar() */
int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;
  HAL_UART_Transmit(&huart3, &c, 1U, 10U);
  return ch;
}
#define LOG(...)  printf(__VA_ARGS__)
#else
#define LOG(...)  ((void)0)
#endif

static struct udp_pcb *echo_pcb;
static uint32_t status_tick;
static uint8_t  last_link = 0xFFU;
static uint32_t rx_count;

/* ---------------------------------------------------------------------------
 *  UDP 에코
 * ------------------------------------------------------------------------ */
static void udp_echo_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                          const ip_addr_t *addr, u16_t port)
{
  LWIP_UNUSED_ARG(arg);

  if (p == NULL)
  {
    return;
  }
  rx_count++;
  udp_sendto(pcb, p, addr, port);
  pbuf_free(p);
}

/* ---------------------------------------------------------------------------
 *  상태 출력
 * ------------------------------------------------------------------------ */
static const char *role_str(void)
{
  lan8770_Role_t r;

  if (LAN8770_GetRole(&LAN8770, &r) != LAN8770_STATUS_OK)
  {
    return "?";
  }
  return (r == LAN8770_ROLE_MASTER) ? "MASTER" : "SLAVE";
}

static void print_status(void)
{
  uint8_t sqi = 0;
  uint8_t link = netif_is_link_up(&gnetif) ? 1U : 0U;

#if defined(LED_YELLOW_Pin) && defined(LED_YELLOW_GPIO_Port)
  /* 노란 LED : PHY 를 MDIO 로 찾지 못함 */
  HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin,
                    ethernetif_phy_ready() ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif

  if (!ethernetif_phy_ready())
  {
    LOG("[PHY] LAN8770 not found on MDIO (check power / MDC / MDIO / REF_CLK)\r\n");
    return;
  }

  if (link)
  {
    (void)LAN8770_GetSQI(&LAN8770, &sqi);
    LOG("[NET] link UP  100BASE-T1 %s  SQI=%u/7  IP=%s  udp_rx=%lu\r\n",
        role_str(), sqi, ipaddr_ntoa(netif_ip_addr4(&gnetif)), (unsigned long)rx_count);
  }
  else
  {
    LOG("[NET] link DOWN  (%s) - partner must be the opposite role\r\n", role_str());
  }
}

/* ---------------------------------------------------------------------------
 *  공개 함수
 * ------------------------------------------------------------------------ */
void APP_Net_Init(void)
{
  veh_time_init();
  veh_vlan_init();
  APP_SOMEIP_Init();

  LOG("\r\n=== STM32H7 + LAN8770 (100BASE-T1) ===\r\n");

  if (ethernetif_phy_ready())
  {
    LOG("[PHY] LAN8770 found: addr=%lu rev=%lu role=%s\r\n",
        (unsigned long)LAN8770.DevAddr, (unsigned long)LAN8770.PhyRevision, role_str());
  }

  echo_pcb = udp_new();
  if ((echo_pcb != NULL) && (udp_bind(echo_pcb, IP_ADDR_ANY, APP_UDP_ECHO_PORT) == ERR_OK))
  {
    udp_recv(echo_pcb, udp_echo_recv, NULL);
    LOG("[APP] UDP echo server on port %u\r\n", (unsigned)APP_UDP_ECHO_PORT);
  }
  else
  {
    LOG("[APP] UDP bind failed\r\n");
  }

  status_tick = HAL_GetTick();
}

void APP_Net_Process(void)
{
  uint8_t link = netif_is_link_up(&gnetif) ? 1U : 0U;

  uint32_t now_us = veh_time_us();   /* DWT 누적을 위해 매 루프 호출 */

  veh_netmon_process();
  APP_SOMEIP_Process();

#if APP_ECU_TABLE_ENABLE
  {
    static uint32_t ecu_tick, sum_tick;

    if ((HAL_GetTick() - ecu_tick) >= APP_ECU_PRINT_INTERVAL_MS)
    {
      ecu_tick = HAL_GetTick();
      (void)veh_ecu_print_next(now_us, APP_ECU_IDLE_MS * 1000U);
    }
    if ((HAL_GetTick() - sum_tick) >= APP_ECU_SUMMARY_PERIOD_MS)
    {
      sum_tick = HAL_GetTick();
      veh_ecu_print_summary(now_us, APP_ECU_IDLE_MS * 1000U);
    }
  }
#else
  (void)now_us;
#endif

  /* 링크 변화는 즉시 출력 */
  if (link != last_link)
  {
    last_link = link;
#if defined(LED_GREEN_Pin) && defined(LED_GREEN_GPIO_Port)
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, link ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
    print_status();
    status_tick = HAL_GetTick();
  }

  if ((HAL_GetTick() - status_tick) >= APP_STATUS_PERIOD_MS)
  {
    status_tick = HAL_GetTick();
    print_status();
  }
}
