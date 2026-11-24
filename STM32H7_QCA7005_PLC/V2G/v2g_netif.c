/*
 * v2g_netif.c - lwIP network interface on top of the PLC link (QCA7005)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include "v2g_netif.h"
#include "lwip/init.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "lwip/ethip6.h"
#include "lwip/mld6.h"
#include "netif/ethernet.h"
#include "plc_util.h"

static struct netif     s_netif;
static v2g_link_send_t  s_send;
static void            *s_ctx;
static uint8_t          s_tx[1536];
static uint8_t          s_started;

static err_t linkoutput(struct netif *nif, struct pbuf *p)
{
    uint16_t len = p->tot_len;
    (void)nif;
    if (len > sizeof(s_tx))
        return ERR_BUF;
    pbuf_copy_partial(p, s_tx, len, 0);
    return s_send(s_ctx, s_tx, len) == 0 ? ERR_OK : ERR_IF;
}

static err_t netif_init_cb(struct netif *nif)
{
    nif->name[0] = 'p';
    nif->name[1] = 'l';
    nif->mtu = 1500;
    nif->mtu6 = 1500;
    nif->hwaddr_len = ETH_HWADDR_LEN;
    nif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHERNET | NETIF_FLAG_MLD6;
    nif->linkoutput = linkoutput;
    nif->output_ip6 = ethip6_output;
    return ERR_OK;
}

void v2g_netif_init(const uint8_t mac[6], v2g_link_send_t send, void *ctx)
{
    s_send = send;
    s_ctx = ctx;
    if (s_started)
        return;
    s_started = 1;

    lwip_init();
    memcpy(s_netif.hwaddr, mac, ETH_HWADDR_LEN);
    netif_add_noaddr(&s_netif, NULL, netif_init_cb, ethernet_input);
    netif_create_ip6_linklocal_address(&s_netif, 1);   /* fe80:: + modified EUI-64 */
    netif_set_default(&s_netif);
    netif_set_up(&s_netif);
    netif_set_link_up(&s_netif);
    PLC_LOG("[V2G] netif up, link-local %s\r\n", ip6addr_ntoa(netif_ip6_addr(&s_netif, 0)));
}

void v2g_netif_input(const uint8_t *frame, uint16_t len)
{
    struct pbuf *p;

    if (!s_started || len < 14)
        return;
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!p)
        return;
    pbuf_take(p, frame, len);
    if (s_netif.input(p, &s_netif) != ERR_OK)
        pbuf_free(p);
}

void v2g_netif_poll(void)
{
    if (s_started)
        sys_check_timeouts();
}

int v2g_netif_ready(void)
{
    return s_started && ip6_addr_ispreferred(netif_ip6_addr_state(&s_netif, 0));
}

const ip6_addr_t *v2g_netif_ll_addr(void)
{
    return netif_ip6_addr(&s_netif, 0);
}

struct netif *v2g_netif_get(void)
{
    return &s_netif;
}
