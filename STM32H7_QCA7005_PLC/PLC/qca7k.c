/*
 * qca7k.c - QCA7000 / QCA7005 HomePlug Green PHY SPI host driver (bare metal)
 *
 * The register protocol, the sync / reset state machine and the interrupt
 * handling order follow the Linux kernel driver
 *   drivers/net/ethernet/qualcomm/qca_spi.c, qca_7k.c
 *   Copyright (c) 2011, 2012, Qualcomm Atheros Communications Inc.
 *   Copyright (c) 2014, I2SE GmbH
 * (dual licensed GPL-2.0 OR BSD-2-Clause, used here under BSD-2-Clause).
 * The code is rewritten for a single-threaded MCU main loop.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include "qca7k.h"
#include "plc_util.h"

#define INTR_ENABLE_MASK (SPI_INT_CPU_ON | SPI_INT_PKT_AVLBL | SPI_INT_RDBUF_ERR | SPI_INT_WRBUF_ERR)

static uint32_t now_ms(qca7k_t *q)
{
    return q->port->millis(q->port->ctx);
}

/* ---- raw SPI helpers ------------------------------------------------------ */

static int spi_cmd(qca7k_t *q, uint16_t cmd)
{
    uint8_t c[2];
    plc_put_be16(c, cmd);
    return q->port->xfer(q->port->ctx, c, NULL, 2);
}

int qca7k_read_reg(qca7k_t *q, uint16_t reg, uint16_t *val)
{
    uint8_t rx[2] = {0, 0};
    int ret;

    q->port->cs(q->port->ctx, 1);
    ret = spi_cmd(q, (uint16_t)(QCA7K_SPI_READ | QCA7K_SPI_INTERNAL | reg));
    if (ret == 0)
        ret = q->port->xfer(q->port->ctx, NULL, rx, 2);
    q->port->cs(q->port->ctx, 0);

    if (ret) {
        q->stats.spi_err++;
        *val = 0;
        return ret;
    }
    *val = plc_get_be16(rx);
    return 0;
}

int qca7k_write_reg(qca7k_t *q, uint16_t reg, uint16_t val)
{
    uint8_t tx[4];
    int ret;

    plc_put_be16(&tx[0], (uint16_t)(QCA7K_SPI_WRITE | QCA7K_SPI_INTERNAL | reg));
    plc_put_be16(&tx[2], val);
    q->port->cs(q->port->ctx, 1);
    ret = q->port->xfer(q->port->ctx, tx, NULL, 4);
    q->port->cs(q->port->ctx, 0);
    if (ret)
        q->stats.spi_err++;
    return ret;
}

/* ---- sync state machine (qcaspi_qca7k_sync) ------------------------------ */

static void request_reset(qca7k_t *q)
{
    q->reset_pending = 1;
    q->t_sync = now_ms(q);          /* handle on the next poll */
}

static void sync_cpu_on(qca7k_t *q)
{
    uint16_t sig = 0, wrbuf = 0;

    /* read signature twice, if not valid go back to unknown state */
    qca7k_read_reg(q, SPI_REG_SIGNATURE, &sig);
    qca7k_read_reg(q, SPI_REG_SIGNATURE, &sig);
    if (sig != QCASPI_GOOD_SIGNATURE) {
        if (q->sync == QCA7K_SYNC_READY)
            q->stats.bad_signature++;
        request_reset(q);
        return;
    }
    /* ensure that the write buffer is empty */
    qca7k_read_reg(q, SPI_REG_WRBUF_SPC_AVA, &wrbuf);
    if (wrbuf != QCASPI_HW_BUF_LEN) {
        q->sync = QCA7K_SYNC_UNKNOWN;
        q->stats.buf_avail_err++;
        return;
    }
    if (q->sync != QCA7K_SYNC_READY)
        PLC_LOG("[QCA] in sync (signature 0x%04X)\r\n", sig);
    q->sync = QCA7K_SYNC_READY;
    q->t_sig = now_ms(q) + QCA7K_SIGNATURE_CHECK_MS;
}

static void sync_update(qca7k_t *q)
{
    uint16_t sig = 0, cfg = 0;

    if (q->reset_pending) {
        q->reset_pending = 0;
        if (q->sync == QCA7K_SYNC_READY)
            PLC_LOG("[QCA] lost sync, resetting\r\n");
        q->sync = QCA7K_SYNC_UNKNOWN;
    }

    switch (q->sync) {
    case QCA7K_SYNC_READY:
        qca7k_read_reg(q, SPI_REG_SIGNATURE, &sig);
        if (sig != QCASPI_GOOD_SIGNATURE)
            qca7k_read_reg(q, SPI_REG_SIGNATURE, &sig);
        if (sig != QCASPI_GOOD_SIGNATURE) {
            q->stats.bad_signature++;
            request_reset(q);       /* don't reset right away */
        }
        break;

    case QCA7K_SYNC_UNKNOWN:
        qca7k_read_reg(q, SPI_REG_SIGNATURE, &sig);
        if (sig != QCASPI_GOOD_SIGNATURE) {
            PLC_LOG("[QCA] no signature (read 0x%04X), check SPI wiring/mode 3\r\n", sig);
            break;                  /* stay in unknown state */
        }
        /* soft reset through SPI_CONFIG */
        qca7k_read_reg(q, SPI_REG_SPI_CONFIG, &cfg);
        cfg |= QCASPI_SLAVE_RESET_BIT;
        qca7k_write_reg(q, SPI_REG_SPI_CONFIG, cfg);
        q->sync = QCA7K_SYNC_RESET;
        q->stats.trig_reset++;
        q->reset_count = 0;
        break;

    case QCA7K_SYNC_RESET:
        q->reset_count++;
        if (q->reset_count >= QCA7K_RESET_TIMEOUT) {
            q->stats.reset_timeout++;
            PLC_LOG("[QCA] reset timeout (no CPU_ON), retry\r\n");
            request_reset(q);
        }
        break;

    case QCA7K_SYNC_HWRESET:
    default:
        break;
    }
}

/* ---- receive (qcaspi_receive) ---------------------------------------------- */

static void receive(qca7k_t *q)
{
    uint16_t avail = 0;
    int ret;
    uint16_t i;

    qca7k_read_reg(q, SPI_REG_RDBUF_BYTE_AVA, &avail);
    if (avail > QCASPI_HW_BUF_LEN + QCASPI_HW_PKT_LEN) {
        q->stats.buf_avail_err++;   /* SPI interference, retry later */
        return;
    }
    if (avail == 0)
        return;

    qca7k_write_reg(q, SPI_REG_BFR_SIZE, avail);

    q->port->cs(q->port->ctx, 1);
    ret = spi_cmd(q, (uint16_t)(QCA7K_SPI_READ | QCA7K_SPI_EXTERNAL));
    if (ret == 0)
        ret = q->port->xfer(q->port->ctx, NULL, q->rx_burst, avail);
    q->port->cs(q->port->ctx, 0);
    if (ret) {
        q->stats.spi_err++;
        q->stats.rx_errors++;
        return;
    }

    for (i = 0; i < avail; i++) {
        int32_t r = qcafrm_fsm_decode(&q->frm, q->rx_frame, sizeof(q->rx_frame), q->rx_burst[i]);
        if (r == QCAFRM_GATHER || r == QCAFRM_NOHEAD)
            continue;
        if (r < 0) {
            q->stats.rx_errors++;
            continue;
        }
        q->stats.rx_packets++;
        if (q->rx_cb)
            q->rx_cb(q->rx_user, q->rx_frame, (uint16_t)r);
    }
}

/* ---- interrupt handling (start/end_spi_intr_handling) -------------------- */

static void handle_intr(qca7k_t *q, int polled)
{
    uint16_t cause = 0;

    qca7k_write_reg(q, SPI_REG_INTR_ENABLE, 0);
    qca7k_read_reg(q, SPI_REG_INTR_CAUSE, &cause);

    if (cause == 0) {
        /* nothing latched; when polling without the INT line also look at
         * the read buffer directly */
        if (polled && q->sync == QCA7K_SYNC_READY)
            receive(q);
        qca7k_write_reg(q, SPI_REG_INTR_ENABLE, INTR_ENABLE_MASK);
        return;
    }

    if (cause & SPI_INT_CPU_ON) {
        sync_cpu_on(q);
        if (q->frm.state != q->frm.init)
            q->stats.rx_errors++;   /* frame decoding was in progress */
        qcafrm_fsm_init_spi(&q->frm);
        q->stats.device_reset++;
        if (q->sync != QCA7K_SYNC_READY)
            return;
    }

    if (cause & SPI_INT_RDBUF_ERR) {
        q->stats.read_buf_err++;
        request_reset(q);
        return;
    }
    if (cause & SPI_INT_WRBUF_ERR) {
        q->stats.write_buf_err++;
        request_reset(q);
        return;
    }

    if (q->sync == QCA7K_SYNC_READY && (cause & SPI_INT_PKT_AVLBL))
        receive(q);

    qca7k_write_reg(q, SPI_REG_INTR_CAUSE, cause);
    qca7k_write_reg(q, SPI_REG_INTR_ENABLE, INTR_ENABLE_MASK);
}

/* ---- public API ------------------------------------------------------------ */

void qca7k_init(qca7k_t *q, const qca7k_port_t *port, qca7k_rx_cb_t cb, void *user)
{
    memset(q, 0, sizeof(*q));
    q->port = port;
    q->rx_cb = cb;
    q->rx_user = user;
    qcafrm_fsm_init_spi(&q->frm);
    port->cs(port->ctx, 0);

    if (port->reset) {
        port->reset(port->ctx, 1);
        q->sync = QCA7K_SYNC_HWRESET;
        q->t_sync = now_ms(q) + QCA7K_HW_RESET_MS;
    } else {
        q->sync = QCA7K_SYNC_UNKNOWN;
        q->t_sync = now_ms(q);
    }
    q->t_intr = now_ms(q);
}

int qca7k_is_ready(const qca7k_t *q)
{
    return q->sync == QCA7K_SYNC_READY;
}

void qca7k_poll(qca7k_t *q)
{
    uint32_t now = now_ms(q);
    int pending = 0, polled = 0;

    if (q->sync == QCA7K_SYNC_HWRESET) {
        if (!plc_time_after_eq(now, q->t_sync))
            return;
        q->port->reset(q->port->ctx, 0);
        /* the device boots and signals CPU_ON */
        q->sync = QCA7K_SYNC_RESET;
        q->reset_count = 0;
        q->t_sync = now + QCA7K_REBOOT_TIME_MS;
        return;
    }

    /* periodic sync / signature check */
    if (q->sync == QCA7K_SYNC_READY) {
        if (q->reset_pending || plc_time_after_eq(now, q->t_sig)) {
            sync_update(q);
            q->t_sig = now + QCA7K_SIGNATURE_CHECK_MS;
        }
    } else if (plc_time_after_eq(now, q->t_sync)) {
        sync_update(q);
        q->t_sync = now + QCA7K_REBOOT_TIME_MS;
    }

    /* interrupt: INT line, or poll INTR_CAUSE as a fallback */
    if (q->port->intr_pending)
        pending = q->port->intr_pending(q->port->ctx);
    if (!pending && q->sync != QCA7K_SYNC_UNKNOWN && plc_time_after_eq(now, q->t_intr)) {
        pending = 1;
        polled = 1;
    }
    if (pending) {
        q->t_intr = now + QCA7K_INTR_POLL_MS;
        handle_intr(q, polled);
    }
}

int qca7k_send(qca7k_t *q, const uint8_t *frame, uint16_t len)
{
    uint8_t hdr[QCAFRM_HEADER_LEN], ftr[QCAFRM_FOOTER_LEN];
    uint16_t flen, total, avail = 0;
    uint32_t t0;
    int ret;

    if (q->sync != QCA7K_SYNC_READY)
        return -1;
    if (len < QCAFRM_ETH_HLEN || len > QCAFRM_MAX_LEN)
        return -4;

    flen = len < QCAFRM_MIN_LEN ? QCAFRM_MIN_LEN : len;
    total = (uint16_t)(QCAFRM_HEADER_LEN + flen + QCAFRM_FOOTER_LEN);

    t0 = now_ms(q);
    for (;;) {
        if (qca7k_read_reg(q, SPI_REG_WRBUF_SPC_AVA, &avail))
            return -3;
        if (avail > QCASPI_HW_BUF_LEN) {
            q->stats.buf_avail_err++;
            return -3;
        }
        if (avail >= total + QCASPI_HW_PKT_LEN)
            break;
        if (plc_time_after_eq(now_ms(q), t0 + QCA7K_TX_WAIT_MS)) {
            q->stats.write_buf_miss++;
            return -2;
        }
    }

    qca7k_write_reg(q, SPI_REG_BFR_SIZE, total);

    qcafrm_create_header(hdr, flen);
    qcafrm_create_footer(ftr);

    q->port->cs(q->port->ctx, 1);
    ret = spi_cmd(q, (uint16_t)(QCA7K_SPI_WRITE | QCA7K_SPI_EXTERNAL));
    if (ret == 0) ret = q->port->xfer(q->port->ctx, hdr, NULL, sizeof(hdr));
    if (ret == 0) ret = q->port->xfer(q->port->ctx, frame, NULL, len);
    if (ret == 0 && flen > len) ret = q->port->xfer(q->port->ctx, NULL, NULL, (uint16_t)(flen - len));
    if (ret == 0) ret = q->port->xfer(q->port->ctx, ftr, NULL, sizeof(ftr));
    q->port->cs(q->port->ctx, 0);

    if (ret) {
        q->stats.spi_err++;
        q->stats.tx_errors++;
        return -3;
    }
    q->stats.tx_packets++;
    return 0;
}
