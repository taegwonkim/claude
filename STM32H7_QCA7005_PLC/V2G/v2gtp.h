/*
 * v2gtp.h - V2G Transfer Protocol (ISO 15118-2 / DIN 70121) header and
 *           TCP stream reassembly
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *   byte 0   protocol version 0x01
 *   byte 1   inverse version   0xFE
 *   byte 2-3 payload type (big endian): 0x8001 EXI, 0x9000/0x9001 SDP
 *   byte 4-7 payload length (big endian)
 */
#ifndef V2GTP_H
#define V2GTP_H

#include <stdint.h>
#include "v2g_conf.h"

#define V2GTP_HDR_LEN          8u
#define V2GTP_VERSION          0x01u
#define V2GTP_TYPE_EXI         0x8001u
#define V2GTP_TYPE_SDP_REQ     0x9000u
#define V2GTP_TYPE_SDP_RES     0x9001u

void v2gtp_write_header(uint8_t *buf, uint16_t type, uint32_t len);
/* 1 = valid header (type, len set), 0 = need more bytes, -1 = invalid */
int  v2gtp_parse_header(const uint8_t *buf, uint32_t avail, uint16_t *type, uint32_t *len);

typedef struct {
    uint8_t  buf[V2G_MSG_MAX];
    uint32_t len;
} v2gtp_rx_t;

void v2gtp_rx_reset(v2gtp_rx_t *r);
/* append received TCP data; -1 if it does not fit */
int  v2gtp_rx_push(v2gtp_rx_t *r, const uint8_t *data, uint32_t n);
/* 1: a whole message is at r->buf (payload at +8, length *plen), 0: incomplete,
 * -1: protocol error (bad header / too long) */
int  v2gtp_rx_peek(const v2gtp_rx_t *r, uint16_t *type, uint32_t *plen);
/* drop the first message after it was handled */
void v2gtp_rx_pop(v2gtp_rx_t *r);

#endif /* V2GTP_H */
