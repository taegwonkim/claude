/*
 * v2g_netif.h - lwIP network interface on top of the PLC link (QCA7005)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Ethernet frames that are not HomePlug MMEs (IPv6, ethertype 0x86DD) go from
 * the QCA7005 driver into lwIP, and lwIP sends its frames through the same
 * driver. The interface uses only an IPv6 link-local address (fe80::/64,
 * derived from the MAC), as ISO 15118 / DIN 70121 require.
 */
#ifndef V2G_NETIF_H
#define V2G_NETIF_H

#include <stdint.h>
#include "lwip/netif.h"

typedef int (*v2g_link_send_t)(void *ctx, const uint8_t *frame, uint16_t len);

void  v2g_netif_init(const uint8_t mac[6], v2g_link_send_t send, void *ctx);
/* feed one received ethernet frame (header + payload, no FCS) */
void  v2g_netif_input(const uint8_t *frame, uint16_t len);
/* lwIP timers; call every loop */
void  v2g_netif_poll(void);
/* link-local address passed duplicate address detection */
int   v2g_netif_ready(void);
const ip6_addr_t *v2g_netif_ll_addr(void);
struct netif *v2g_netif_get(void);

#endif /* V2G_NETIF_H */
