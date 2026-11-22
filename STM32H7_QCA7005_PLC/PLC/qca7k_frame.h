/*
 * qca7k_frame.h - QCA7000/QCA7005 SPI/UART frame encapsulation
 *
 * Ported from the Linux kernel driver drivers/net/ethernet/qualcomm/qca_7k_common.h
 *   Copyright (c) 2011, 2012, Atheros Communications Inc.
 *   Copyright (c) 2014, I2SE GmbH
 * which is dual licensed GPL-2.0 OR BSD-2-Clause; this port is used under BSD-2-Clause.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Frame on the wire (QCA7K -> host on SPI, each packet in the read buffer):
 *
 *   [HW LEN 4 bytes, SPI only] AA AA AA AA | LEN_L LEN_H | 00 00 | ethernet frame | 55 55
 */
#ifndef QCA7K_FRAME_H
#define QCA7K_FRAME_H

#include <stdint.h>

#define QCAFRM_HEADER_LEN  8u
#define QCAFRM_FOOTER_LEN  2u

#define QCAFRM_ETH_HLEN    14u
#define QCAFRM_ETH_ZLEN    60u      /* minimum frame length without FCS */
#define QCAFRM_MIN_LEN     QCAFRM_ETH_ZLEN
#define QCAFRM_MAX_LEN     (1500u + 18u) /* MTU + VLAN ethernet header */

/* return codes of qcafrm_fsm_decode() (>0 = complete frame of that length) */
#define QCAFRM_GATHER      0
#define QCAFRM_ERR_BASE    (-1000)
#define QCAFRM_NOHEAD      (QCAFRM_ERR_BASE - 1)
#define QCAFRM_NOTAIL      (QCAFRM_ERR_BASE - 2)
#define QCAFRM_INVLEN      (QCAFRM_ERR_BASE - 3)

typedef enum {
    QCAFRM_HW_LEN0 = 0x8000,
    QCAFRM_HW_LEN1 = QCAFRM_HW_LEN0 - 1,
    QCAFRM_HW_LEN2 = QCAFRM_HW_LEN1 - 1,
    QCAFRM_HW_LEN3 = QCAFRM_HW_LEN2 - 1,
    QCAFRM_WAIT_AA1 = QCAFRM_HW_LEN3 - 1,
    QCAFRM_WAIT_AA2 = QCAFRM_WAIT_AA1 - 1,
    QCAFRM_WAIT_AA3 = QCAFRM_WAIT_AA2 - 1,
    QCAFRM_WAIT_AA4 = QCAFRM_WAIT_AA3 - 1,
    QCAFRM_WAIT_LEN_BYTE0 = QCAFRM_WAIT_AA4 - 1,
    QCAFRM_WAIT_LEN_BYTE1 = QCAFRM_WAIT_AA4 - 2,
    QCAFRM_WAIT_RSVD_BYTE1 = QCAFRM_WAIT_AA4 - 3,
    QCAFRM_WAIT_RSVD_BYTE2 = QCAFRM_WAIT_AA4 - 4,
    /* the remaining frame length is used as state while receiving payload */
    QCAFRM_WAIT_551 = 1,
    QCAFRM_WAIT_552 = QCAFRM_WAIT_551 - 1
} qcafrm_state_t;

typedef struct {
    int32_t  state;   /* qcafrm_state_t or remaining length */
    int32_t  init;    /* initial state: SPI or UART */
    uint16_t offset;
} qcafrm_handle_t;

uint16_t qcafrm_create_header(uint8_t *buf, uint16_t len);
uint16_t qcafrm_create_footer(uint8_t *buf);

static inline void qcafrm_fsm_init_spi(qcafrm_handle_t *h)
{
    h->init = QCAFRM_HW_LEN0;
    h->state = h->init;
    h->offset = 0;
}

static inline void qcafrm_fsm_init_uart(qcafrm_handle_t *h)
{
    h->init = QCAFRM_WAIT_AA1;
    h->state = h->init;
    h->offset = 0;
}

/* Feed one received byte. Returns QCAFRM_GATHER, an error code, or the
 * length of a complete ethernet frame now stored in buf. */
int32_t qcafrm_fsm_decode(qcafrm_handle_t *h, uint8_t *buf, uint16_t buf_len, uint8_t recv_byte);

#endif /* QCA7K_FRAME_H */
