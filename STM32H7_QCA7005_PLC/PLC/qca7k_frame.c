/*
 * qca7k_frame.c - QCA7000/QCA7005 frame encapsulation and receive state machine
 *
 * Ported from the Linux kernel driver drivers/net/ethernet/qualcomm/qca_7k_common.c
 *   Copyright (c) 2011, 2012, Atheros Communications Inc.
 *   Copyright (c) 2014, I2SE GmbH
 * which is dual licensed GPL-2.0 OR BSD-2-Clause; this port is used under BSD-2-Clause.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "qca7k_frame.h"

uint16_t qcafrm_create_header(uint8_t *buf, uint16_t length)
{
    if (!buf)
        return 0;

    buf[0] = 0xAA;
    buf[1] = 0xAA;
    buf[2] = 0xAA;
    buf[3] = 0xAA;
    buf[4] = (uint8_t)(length & 0xFFu);   /* little endian */
    buf[5] = (uint8_t)(length >> 8);
    buf[6] = 0;
    buf[7] = 0;

    return QCAFRM_HEADER_LEN;
}

uint16_t qcafrm_create_footer(uint8_t *buf)
{
    if (!buf)
        return 0;

    buf[0] = 0x55;
    buf[1] = 0x55;
    return QCAFRM_FOOTER_LEN;
}

int32_t qcafrm_fsm_decode(qcafrm_handle_t *h, uint8_t *buf, uint16_t buf_len, uint8_t recv_byte)
{
    int32_t ret = QCAFRM_GATHER;
    uint16_t len;

    switch (h->state) {
    case QCAFRM_HW_LEN0:
    case QCAFRM_HW_LEN1:
        /* the first two bytes of the hardware length must be 0 */
        h->state--;
        if (recv_byte != 0x00)
            h->state = h->init;
        break;
    case QCAFRM_HW_LEN2:
    case QCAFRM_HW_LEN3:
        h->state--;
        break;
    case QCAFRM_WAIT_AA1:
    case QCAFRM_WAIT_AA2:
    case QCAFRM_WAIT_AA3:
    case QCAFRM_WAIT_AA4:
        if (recv_byte != 0xAA) {
            ret = QCAFRM_NOHEAD;
            h->state = h->init;
        } else {
            h->state--;
        }
        break;
    case QCAFRM_WAIT_LEN_BYTE0:
        h->offset = recv_byte;            /* borrow offset to hold the length */
        h->state = QCAFRM_WAIT_LEN_BYTE1;
        break;
    case QCAFRM_WAIT_LEN_BYTE1:
        h->offset = (uint16_t)(h->offset | ((uint16_t)recv_byte << 8));
        h->state = QCAFRM_WAIT_RSVD_BYTE1;
        break;
    case QCAFRM_WAIT_RSVD_BYTE1:
        h->state = QCAFRM_WAIT_RSVD_BYTE2;
        break;
    case QCAFRM_WAIT_RSVD_BYTE2:
        len = h->offset;
        if (len > buf_len || len < QCAFRM_MIN_LEN) {
            ret = QCAFRM_INVLEN;
            h->state = h->init;
        } else {
            h->state = (int32_t)len + 1;  /* remaining bytes + 1 */
            h->offset = 0;
        }
        break;
    default:
        /* receiving the ethernet frame itself */
        buf[h->offset] = recv_byte;
        h->offset++;
        h->state--;
        break;
    case QCAFRM_WAIT_551:
        if (recv_byte != 0x55) {
            ret = QCAFRM_NOTAIL;
            h->state = h->init;
        } else {
            h->state = QCAFRM_WAIT_552;
        }
        break;
    case QCAFRM_WAIT_552:
        if (recv_byte != 0x55) {
            ret = QCAFRM_NOTAIL;
            h->state = h->init;
        } else {
            ret = h->offset;              /* frame complete */
            h->state = h->init;
        }
        break;
    }

    return ret;
}
