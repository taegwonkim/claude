/*
 * v2gtp.c - V2G Transfer Protocol header and TCP stream reassembly
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include "v2gtp.h"

void v2gtp_write_header(uint8_t *b, uint16_t type, uint32_t len)
{
    b[0] = V2GTP_VERSION;
    b[1] = (uint8_t)~V2GTP_VERSION;
    b[2] = (uint8_t)(type >> 8);
    b[3] = (uint8_t)type;
    b[4] = (uint8_t)(len >> 24);
    b[5] = (uint8_t)(len >> 16);
    b[6] = (uint8_t)(len >> 8);
    b[7] = (uint8_t)len;
}

int v2gtp_parse_header(const uint8_t *b, uint32_t avail, uint16_t *type, uint32_t *len)
{
    if (avail < V2GTP_HDR_LEN)
        return 0;
    if (b[0] != V2GTP_VERSION || b[1] != (uint8_t)~V2GTP_VERSION)
        return -1;
    *type = (uint16_t)((b[2] << 8) | b[3]);
    *len = ((uint32_t)b[4] << 24) | ((uint32_t)b[5] << 16) | ((uint32_t)b[6] << 8) | b[7];
    return 1;
}

void v2gtp_rx_reset(v2gtp_rx_t *r)
{
    r->len = 0;
}

int v2gtp_rx_push(v2gtp_rx_t *r, const uint8_t *data, uint32_t n)
{
    if (n > sizeof(r->buf) - r->len)
        return -1;
    memcpy(&r->buf[r->len], data, n);
    r->len += n;
    return 0;
}

int v2gtp_rx_peek(const v2gtp_rx_t *r, uint16_t *type, uint32_t *plen)
{
    int h = v2gtp_parse_header(r->buf, r->len, type, plen);
    if (h <= 0)
        return h;
    if (*plen > sizeof(r->buf) - V2GTP_HDR_LEN)
        return -1;
    return r->len >= V2GTP_HDR_LEN + *plen ? 1 : 0;
}

void v2gtp_rx_pop(v2gtp_rx_t *r)
{
    uint16_t type;
    uint32_t plen, used;
    if (v2gtp_rx_peek(r, &type, &plen) != 1)
        return;
    used = V2GTP_HDR_LEN + plen;
    memmove(r->buf, &r->buf[used], r->len - used);
    r->len -= used;
}
