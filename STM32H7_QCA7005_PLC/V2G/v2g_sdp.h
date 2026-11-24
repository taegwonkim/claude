/*
 * v2g_sdp.h - SECC Discovery Protocol (UDP 15118)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *   EV  -> ff02::1 : V2GTP 0x9000 [security][transport]
 *   EVSE -> EV     : V2GTP 0x9001 [SECC IPv6 (16)][port (2)][security][transport]
 */
#ifndef V2G_SDP_H
#define V2G_SDP_H

#include <stdint.h>
#include "lwip/ip_addr.h"

#define V2G_SDP_REQ_LEN  2u
#define V2G_SDP_RES_LEN  20u

/* ---- charger side: answer requests ------------------------------------------- */
int  v2g_sdp_server_start(uint16_t tcp_port);
void v2g_sdp_server_stop(void);

/* ---- vehicle side: find the charger ------------------------------------------ */
typedef enum { V2G_SDP_IDLE = 0, V2G_SDP_SEARCHING, V2G_SDP_FOUND, V2G_SDP_FAILED } v2g_sdp_state_t;

typedef struct {
    v2g_sdp_state_t state;
    uint8_t   tries;
    uint32_t  t_next;
    ip6_addr_t secc_addr;
    uint16_t  secc_port;
    uint8_t   security, transport;
} v2g_sdp_client_t;

void v2g_sdp_client_start(v2g_sdp_client_t *c, uint32_t now);
void v2g_sdp_client_stop(v2g_sdp_client_t *c);
void v2g_sdp_client_process(v2g_sdp_client_t *c, uint32_t now);

/* encode / decode helpers (also used by the tests) */
uint16_t v2g_sdp_build_req(uint8_t *buf, uint8_t security, uint8_t transport);
uint16_t v2g_sdp_build_res(uint8_t *buf, const ip6_addr_t *addr, uint16_t port, uint8_t security, uint8_t transport);
int      v2g_sdp_parse_res(const uint8_t *buf, uint16_t len, ip6_addr_t *addr, uint16_t *port, uint8_t *security, uint8_t *transport);
int      v2g_sdp_parse_req(const uint8_t *buf, uint16_t len, uint8_t *security, uint8_t *transport);

#endif /* V2G_SDP_H */
