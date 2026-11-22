/*
 * qca7k.h - QCA7000 / QCA7005 HomePlug Green PHY SPI host driver (bare metal)
 *
 * Protocol behaviour (register map, sync/reset sequence, burst transfers) follows
 * the Linux kernel driver drivers/net/ethernet/qualcomm/qca_spi.c / qca_7k.c
 * (dual licensed GPL-2.0 OR BSD-2-Clause, used here under BSD-2-Clause).
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Hardware independent: the board supplies a qca7k_port_t (SPI transfer,
 * chip select, interrupt line, reset pin, millisecond tick).
 *
 * SPI: mode 3 (CPOL=1, CPHA=1), 8 bit, MSB first, <= 16 MHz (8 MHz typical),
 *      chip select driven by software and held low for a whole command.
 */
#ifndef QCA7K_H
#define QCA7K_H

#include <stdint.h>
#include "qca7k_frame.h"

/* ---- SPI command word (16 bit, big endian on the wire) ---------------- */
#define QCA7K_SPI_READ      (1u << 15)
#define QCA7K_SPI_WRITE     (0u << 15)
#define QCA7K_SPI_INTERNAL  (1u << 14)   /* register access */
#define QCA7K_SPI_EXTERNAL  (0u << 14)   /* buffer (frame) access */

/* ---- internal registers ------------------------------------------------ */
#define SPI_REG_BFR_SIZE        0x0100
#define SPI_REG_WRBUF_SPC_AVA   0x0200
#define SPI_REG_RDBUF_BYTE_AVA  0x0300
#define SPI_REG_SPI_CONFIG      0x0400
#define SPI_REG_SPI_STATUS      0x0500
#define SPI_REG_INTR_CAUSE      0x0C00
#define SPI_REG_INTR_ENABLE     0x0D00
#define SPI_REG_RDBUF_WATERMARK 0x1200
#define SPI_REG_WRBUF_WATERMARK 0x1300
#define SPI_REG_SIGNATURE       0x1A00
#define SPI_REG_ACTION_CTRL     0x1B00

#define QCASPI_SLAVE_RESET_BIT  (1u << 6)   /* in SPI_CONFIG */

#define SPI_INT_WRBUF_BELOW_WM  (1u << 10)
#define SPI_INT_CPU_ON          (1u << 6)
#define SPI_INT_ADDR_ERR        (1u << 3)
#define SPI_INT_WRBUF_ERR       (1u << 2)
#define SPI_INT_RDBUF_ERR       (1u << 1)
#define SPI_INT_PKT_AVLBL       (1u << 0)

#define QCASPI_GOOD_SIGNATURE   0xAA55u
#define QCASPI_HW_PKT_LEN       4u        /* per-packet length word in the buffers */
#define QCASPI_HW_BUF_LEN       0x0C5Bu   /* 3163 bytes write/read buffer */

/* ---- tuning ---------------------------------------------------------------- */
#ifndef QCA7K_REBOOT_TIME_MS
#define QCA7K_REBOOT_TIME_MS     1000u    /* sync retry period while not ready */
#endif
#ifndef QCA7K_RESET_TIMEOUT
#define QCA7K_RESET_TIMEOUT      10u      /* periods to wait for CPU_ON after reset */
#endif
#ifndef QCA7K_SIGNATURE_CHECK_MS
#define QCA7K_SIGNATURE_CHECK_MS 1000u    /* periodic signature check when ready */
#endif
#ifndef QCA7K_INTR_POLL_MS
#define QCA7K_INTR_POLL_MS       20u      /* read INTR_CAUSE even without the INT line */
#endif
#ifndef QCA7K_TX_WAIT_MS
#define QCA7K_TX_WAIT_MS         5u       /* max wait for write buffer space */
#endif
#ifndef QCA7K_HW_RESET_MS
#define QCA7K_HW_RESET_MS        20u      /* RESET_L low time at start-up */
#endif

typedef struct {
    void *ctx;
    /* chip select: active != 0 -> CS low */
    void (*cs)(void *ctx, int active);
    /* full duplex transfer while CS is held. tx == NULL: send 0x00,
     * rx == NULL: discard. len can be up to 3167. return 0 on success */
    int (*xfer)(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len);
    /* return non-zero if the QCA7K interrupt is pending (latched EXTI flag or
     * INT pin high). Should clear the latched flag. May be NULL (polling only). */
    int (*intr_pending)(void *ctx);
    /* drive RESET_L: assert != 0 -> reset active (pin low). May be NULL. */
    void (*reset)(void *ctx, int assert);
    uint32_t (*millis)(void *ctx);
} qca7k_port_t;

typedef void (*qca7k_rx_cb_t)(void *user, const uint8_t *frame, uint16_t len);

typedef enum {
    QCA7K_SYNC_HWRESET = 0,   /* RESET_L held low */
    QCA7K_SYNC_UNKNOWN,       /* waiting for a valid signature */
    QCA7K_SYNC_RESET,         /* soft reset issued, waiting for CPU_ON */
    QCA7K_SYNC_READY
} qca7k_sync_t;

typedef struct {
    uint32_t rx_packets, tx_packets;
    uint32_t rx_errors, tx_errors;
    uint32_t bad_signature, trig_reset, reset_timeout, device_reset;
    uint32_t buf_avail_err, write_buf_miss, read_buf_err, write_buf_err;
    uint32_t spi_err;
} qca7k_stats_t;

typedef struct {
    const qca7k_port_t *port;
    qca7k_rx_cb_t rx_cb;
    void *rx_user;

    qca7k_sync_t sync;
    uint8_t  reset_pending;
    uint8_t  reset_count;
    uint32_t t_sync;          /* next sync step */
    uint32_t t_sig;           /* next signature check when ready */
    uint32_t t_intr;          /* next interrupt-cause poll */

    qcafrm_handle_t frm;
    uint8_t rx_frame[QCAFRM_MAX_LEN];
    /* whole read buffer is fetched first, then decoded with CS released, so
     * the rx callback may call qca7k_send() */
    uint8_t rx_burst[QCASPI_HW_BUF_LEN + QCASPI_HW_PKT_LEN];

    qca7k_stats_t stats;
} qca7k_t;

void     qca7k_init(qca7k_t *q, const qca7k_port_t *port, qca7k_rx_cb_t cb, void *user);
/* call as often as possible from the main loop (does all SPI work) */
void     qca7k_poll(qca7k_t *q);
int      qca7k_is_ready(const qca7k_t *q);
/* send one ethernet frame (dst,src,type,payload; no FCS). Pads to 60 bytes.
 * return 0 ok, -1 not ready, -2 no buffer space, -3 SPI error, -4 bad length */
int      qca7k_send(qca7k_t *q, const uint8_t *frame, uint16_t len);

/* low level, exposed for diagnostics */
int      qca7k_read_reg(qca7k_t *q, uint16_t reg, uint16_t *val);
int      qca7k_write_reg(qca7k_t *q, uint16_t reg, uint16_t val);

#endif /* QCA7K_H */
