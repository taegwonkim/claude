/*
 * qca7000_netif.c - lwIP netif glue for QCA7000 (IPv6 link-local for ISO 15118)
 *
 * lwipopts.h: LWIP_IPV6=1, LWIP_IPV6_MLD=1, LWIP_ETHERNET=1, NO_SYS=0 (tcpip thread)
 */
#include <string.h>
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "qca7000.h"

static err_t plc_linkoutput(struct netif *netif, struct pbuf *p)
{
    uint8_t *buf;
    (void)netif;

    if (p->tot_len > QCA7000_ETH_MAX_LEN)
        return ERR_BUF;
    buf = qca7000_tx_alloc(10);
    if (!buf)
        return ERR_MEM;
    pbuf_copy_partial(p, buf, p->tot_len, 0);
    return qca7000_tx_commit(buf, p->tot_len) == 0 ? ERR_OK : ERR_IF;
}

/* netif_add(&plc_netif, NULL, NULL, NULL, host_mac, qca7000_netif_init, tcpip_input) */
err_t qca7000_netif_init(struct netif *netif)
{
    const uint8_t *mac = (const uint8_t *)netif->state;   /* host MAC passed as state */

    netif->name[0]    = 'p';
    netif->name[1]    = 'l';
    netif->linkoutput = plc_linkoutput;
#if LWIP_IPV4
    netif->output     = etharp_output;
#endif
    netif->output_ip6 = ethip6_output;
    netif->mtu        = 1500;
    netif->hwaddr_len = ETH_HWADDR_LEN;
    memcpy(netif->hwaddr, mac, ETH_HWADDR_LEN);
    netif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
                        NETIF_FLAG_ETHERNET | NETIF_FLAG_MLD6;
    return ERR_OK;
}

/* Call from the qca7000 rx callback for EtherType 0x86DD (and 0x0800/0x0806 if IPv4). */
void qca7000_netif_input(struct netif *netif, const uint8_t *frame, uint16_t len)
{
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!p)
        return;
    if (pbuf_take(p, frame, len) != ERR_OK || netif->input(p, netif) != ERR_OK)
        pbuf_free(p);
}
