/*
 * v2g_sdp.c - SECC Discovery Protocol (UDP 15118), server and client
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include "v2g_sdp.h"
#include "v2g_conf.h"
#include "v2gtp.h"
#include "v2g_netif.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "plc_util.h"

#define LOG(...) PLC_LOG("[SDP] " __VA_ARGS__)

/* ---- codec --------------------------------------------------------------------- */

uint16_t v2g_sdp_build_req(uint8_t *b, uint8_t security, uint8_t transport)
{
    v2gtp_write_header(b, V2GTP_TYPE_SDP_REQ, V2G_SDP_REQ_LEN);
    b[8] = security;
    b[9] = transport;
    return V2GTP_HDR_LEN + V2G_SDP_REQ_LEN;
}

uint16_t v2g_sdp_build_res(uint8_t *b, const ip6_addr_t *addr, uint16_t port, uint8_t security, uint8_t transport)
{
    int i;
    v2gtp_write_header(b, V2GTP_TYPE_SDP_RES, V2G_SDP_RES_LEN);
    for (i = 0; i < 4; i++) {               /* addr words are in network order */
        uint32_t w = lwip_ntohl(addr->addr[i]);
        b[8 + 4 * i] = (uint8_t)(w >> 24);
        b[9 + 4 * i] = (uint8_t)(w >> 16);
        b[10 + 4 * i] = (uint8_t)(w >> 8);
        b[11 + 4 * i] = (uint8_t)w;
    }
    b[24] = (uint8_t)(port >> 8);
    b[25] = (uint8_t)port;
    b[26] = security;
    b[27] = transport;
    return V2GTP_HDR_LEN + V2G_SDP_RES_LEN;
}

int v2g_sdp_parse_req(const uint8_t *b, uint16_t len, uint8_t *security, uint8_t *transport)
{
    uint16_t type;
    uint32_t plen;
    if (v2gtp_parse_header(b, len, &type, &plen) != 1 || type != V2GTP_TYPE_SDP_REQ ||
        plen != V2G_SDP_REQ_LEN || len < V2GTP_HDR_LEN + V2G_SDP_REQ_LEN)
        return -1;
    *security = b[8];
    *transport = b[9];
    return 0;
}

int v2g_sdp_parse_res(const uint8_t *b, uint16_t len, ip6_addr_t *addr, uint16_t *port,
                      uint8_t *security, uint8_t *transport)
{
    uint16_t type;
    uint32_t plen;
    int i;
    if (v2gtp_parse_header(b, len, &type, &plen) != 1 || type != V2GTP_TYPE_SDP_RES ||
        plen != V2G_SDP_RES_LEN || len < V2GTP_HDR_LEN + V2G_SDP_RES_LEN)
        return -1;
    for (i = 0; i < 4; i++)
        addr->addr[i] = lwip_htonl(((uint32_t)b[8 + 4 * i] << 24) | ((uint32_t)b[9 + 4 * i] << 16) |
                                   ((uint32_t)b[10 + 4 * i] << 8) | b[11 + 4 * i]);
    ip6_addr_clear_zone(addr);
    *port = (uint16_t)((b[24] << 8) | b[25]);
    *security = b[26];
    *transport = b[27];
    return 0;
}

/* ---- server (charger) ------------------------------------------------------------ */

static struct udp_pcb *s_server;
static uint16_t        s_tcp_port;

static void server_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    uint8_t req[16], res[V2GTP_HDR_LEN + V2G_SDP_RES_LEN];
    uint8_t sec, tr;
    uint16_t n;
    struct pbuf *out;
    (void)arg;

    n = pbuf_copy_partial(p, req, sizeof(req), 0);
    pbuf_free(p);
    if (v2g_sdp_parse_req(req, n, &sec, &tr) != 0)
        return;
    LOG("<- request from %s (security 0x%02X, transport 0x%02X)\r\n", ipaddr_ntoa(addr), sec, tr);
    if (tr != V2G_SDP_TRANSPORT_TCP)
        return;
    /* TLS is not implemented: always offer an unsecured TCP connection */
    n = v2g_sdp_build_res(res, v2g_netif_ll_addr(), s_tcp_port, V2G_SDP_SECURITY_NONE, V2G_SDP_TRANSPORT_TCP);
    out = pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM);
    if (!out)
        return;
    pbuf_take(out, res, n);
    udp_sendto(pcb, out, addr, port);
    pbuf_free(out);
}

int v2g_sdp_server_start(uint16_t tcp_port)
{
    s_tcp_port = tcp_port;
    if (s_server)
        return 0;
    s_server = udp_new_ip_type(IPADDR_TYPE_V6);
    if (!s_server)
        return -1;
    if (udp_bind(s_server, IP6_ADDR_ANY, V2G_SDP_PORT) != ERR_OK) {
        udp_remove(s_server);
        s_server = NULL;
        return -1;
    }
    udp_recv(s_server, server_recv, NULL);
    LOG("server listening on UDP %u, SECC TCP port %u\r\n", V2G_SDP_PORT, tcp_port);
    return 0;
}

void v2g_sdp_server_stop(void)
{
    if (s_server) {
        udp_remove(s_server);
        s_server = NULL;
    }
}

/* ---- client (vehicle) ------------------------------------------------------------ */

static struct udp_pcb *s_client;

static void client_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    v2g_sdp_client_t *c = (v2g_sdp_client_t *)arg;
    uint8_t buf[V2GTP_HDR_LEN + V2G_SDP_RES_LEN];
    uint16_t n;
    (void)pcb; (void)addr; (void)port;

    n = pbuf_copy_partial(p, buf, sizeof(buf), 0);
    pbuf_free(p);
    if (c->state != V2G_SDP_SEARCHING)
        return;
    if (v2g_sdp_parse_res(buf, n, &c->secc_addr, &c->secc_port, &c->security, &c->transport) != 0)
        return;
    c->state = V2G_SDP_FOUND;
    LOG("<- response: SECC [%s]:%u security 0x%02X\r\n", ip6addr_ntoa(&c->secc_addr), c->secc_port, c->security);
}

void v2g_sdp_client_start(v2g_sdp_client_t *c, uint32_t now)
{
    memset(c, 0, sizeof(*c));
    if (!s_client) {
        s_client = udp_new_ip_type(IPADDR_TYPE_V6);
        if (!s_client || udp_bind(s_client, IP6_ADDR_ANY, 0) != ERR_OK) {
            c->state = V2G_SDP_FAILED;
            return;
        }
    }
    udp_recv(s_client, client_recv, c);
    c->state = V2G_SDP_SEARCHING;
    c->t_next = now;
}

void v2g_sdp_client_stop(v2g_sdp_client_t *c)
{
    if (s_client) {
        udp_remove(s_client);
        s_client = NULL;
    }
    c->state = V2G_SDP_IDLE;
}

void v2g_sdp_client_process(v2g_sdp_client_t *c, uint32_t now)
{
    uint8_t req[V2GTP_HDR_LEN + V2G_SDP_REQ_LEN];
    ip_addr_t dst;
    struct pbuf *p;
    uint16_t n;

    if (c->state != V2G_SDP_SEARCHING || !plc_time_after_eq(now, c->t_next))
        return;
    if (!v2g_netif_ready()) {               /* wait for DAD of the link-local address */
        c->t_next = now + 50u;
        return;
    }
    if (c->tries >= V2G_SDP_MAX_TRIES) {
        LOG("no SECC found after %u requests\r\n", c->tries);
        c->state = V2G_SDP_FAILED;
        return;
    }
    c->tries++;
    c->t_next = now + V2G_SDP_RETRY_MS;

    n = v2g_sdp_build_req(req, V2G_SDP_SECURITY_NONE, V2G_SDP_TRANSPORT_TCP);
    p = pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM);
    if (!p)
        return;
    pbuf_take(p, req, n);
    ip6_addr_set_allnodes_linklocal(ip_2_ip6(&dst));   /* ff02::1 */
    if (c->tries == 1)
        LOG("-> SECC Discovery Request to ff02::1:%u\r\n", V2G_SDP_PORT);
    udp_sendto(s_client, p, &dst, V2G_SDP_PORT);
    pbuf_free(p);
}
