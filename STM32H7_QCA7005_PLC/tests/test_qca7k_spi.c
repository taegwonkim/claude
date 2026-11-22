/* tests/test_qca7k_spi.c - qca7k driver against a simulated QCA7000 SPI slave */
#include <string.h>
#include "test_common.h"
#include "qca7k.h"
#include "plc_util.h"

/* ---- simulated QCA7000 ------------------------------------------------------ */
typedef struct {
    int      cs;
    unsigned pos;
    uint16_t cmd;
    uint16_t wval;
    int      booted;
    int      signature_broken;
    uint16_t bfr_size, intr_cause, intr_enable, spi_config;
    uint8_t  rdbuf[4096]; unsigned rd_len, rd_pos;
    uint8_t  wrbuf[4096]; unsigned wr_len;
    /* captured transmitted packets */
    uint8_t  tx_pkt[8][1600]; unsigned tx_len[8]; unsigned tx_count;
    unsigned bfr_mismatch;
    unsigned resets;
    uint32_t ms;
} fake_qca_t;

static fake_qca_t F;

static uint16_t reg_read(uint16_t reg)
{
    switch (reg) {
    case SPI_REG_SIGNATURE:      return (F.booted && !F.signature_broken) ? QCASPI_GOOD_SIGNATURE : 0x0000;
    case SPI_REG_WRBUF_SPC_AVA:  return QCASPI_HW_BUF_LEN;  /* packets leave at once */
    case SPI_REG_RDBUF_BYTE_AVA: return (uint16_t)(F.rd_len - F.rd_pos);
    case SPI_REG_INTR_CAUSE:     return F.intr_cause;
    case SPI_REG_INTR_ENABLE:    return F.intr_enable;
    case SPI_REG_SPI_CONFIG:     return F.spi_config;
    case SPI_REG_BFR_SIZE:       return F.bfr_size;
    default:                     return 0;
    }
}

static void reg_write(uint16_t reg, uint16_t v)
{
    switch (reg) {
    case SPI_REG_SPI_CONFIG:
        if (v & QCASPI_SLAVE_RESET_BIT) {   /* soft reset: reboot, raise CPU_ON */
            F.resets++;
            F.booted = 1;
            F.signature_broken = 0;
            F.rd_len = F.rd_pos = 0;
            F.intr_enable = 0;
            F.intr_cause = SPI_INT_CPU_ON;
        }
        F.spi_config = (uint16_t)(v & ~QCASPI_SLAVE_RESET_BIT);
        break;
    case SPI_REG_INTR_CAUSE:  F.intr_cause = (uint16_t)(F.intr_cause & ~v); break;
    case SPI_REG_INTR_ENABLE: F.intr_enable = v; break;
    case SPI_REG_BFR_SIZE:    F.bfr_size = v; break;
    default: break;
    }
}

static void f_cs(void *ctx, int active)
{
    (void)ctx;
    if (!active && F.cs) {
        /* end of a command */
        if (!(F.cmd & QCA7K_SPI_INTERNAL) && !(F.cmd & QCA7K_SPI_READ) && F.pos > 2) {
            if (F.wr_len != F.bfr_size)
                F.bfr_mismatch++;
            if (F.tx_count < 8) {
                memcpy(F.tx_pkt[F.tx_count], F.wrbuf, F.wr_len);
                F.tx_len[F.tx_count] = F.wr_len;
                F.tx_count++;
            }
        }
    }
    if (active && !F.cs) {
        F.pos = 0;
        F.cmd = 0;
        F.wr_len = 0;
    }
    F.cs = active;
}

static int f_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    uint16_t i;
    (void)ctx;
    if (!F.cs)
        return -1;                          /* CS must be held */
    for (i = 0; i < len; i++) {
        uint8_t b = tx ? tx[i] : 0, o = 0;
        if (F.pos < 2) {
            F.cmd = (uint16_t)((F.cmd << 8) | b);
        } else if (F.cmd & QCA7K_SPI_INTERNAL) {
            uint16_t reg = (uint16_t)(F.cmd & 0x3FFF);
            if (F.cmd & QCA7K_SPI_READ) {
                uint16_t v = reg_read(reg);
                o = (F.pos == 2) ? (uint8_t)(v >> 8) : (uint8_t)v;
            } else {
                F.wval = (uint16_t)((F.wval << 8) | b);
                if (F.pos == 3)
                    reg_write(reg, F.wval);
            }
        } else if (F.cmd & QCA7K_SPI_READ) {
            o = F.rd_pos < F.rd_len ? F.rdbuf[F.rd_pos++] : 0;
        } else {
            if (F.wr_len < sizeof(F.wrbuf))
                F.wrbuf[F.wr_len++] = b;
        }
        if (rx)
            rx[i] = o;
        F.pos++;
    }
    return 0;
}

static int f_intr(void *ctx)
{
    (void)ctx;
    return (F.intr_cause & SPI_INT_CPU_ON) || (F.intr_cause & F.intr_enable);
}

static uint32_t f_millis(void *ctx) { (void)ctx; return F.ms; }

static const qca7k_port_t PORT = { NULL, f_cs, f_xfer, f_intr, NULL, f_millis };

static void inject_rx(const uint8_t *eth, uint16_t len)
{
    uint8_t *p = &F.rdbuf[F.rd_len];
    uint16_t total = (uint16_t)(QCAFRM_HEADER_LEN + len + QCAFRM_FOOTER_LEN);
    p[0] = 0; p[1] = 0; p[2] = (uint8_t)(total >> 8); p[3] = (uint8_t)total;
    qcafrm_create_header(&p[4], len);
    memcpy(&p[12], eth, len);
    qcafrm_create_footer(&p[12 + len]);
    F.rd_len += 4u + total;
    F.intr_cause |= SPI_INT_PKT_AVLBL;
}

/* ---- driver under test ----------------------------------------------------- */
static qca7k_t Q;
static unsigned rx_count;
static uint16_t rx_lens[8];
static int echo_in_callback;

static void on_rx(void *user, const uint8_t *frame, uint16_t len)
{
    (void)user;
    if (rx_count < 8)
        rx_lens[rx_count] = len;
    rx_count++;
    if (echo_in_callback)
        CHECK_EQ(qca7k_send(&Q, frame, len), 0);  /* re-entrant send must work */
}

static void run(unsigned ms)
{
    while (ms--) {
        qca7k_poll(&Q);
        F.ms++;
    }
}

void test_qca7k_spi(void)
{
    uint8_t f1[60], f2[200], small[20];
    unsigned i;

    printf("qca7k driver (simulated SPI slave)\n");
    memset(&F, 0, sizeof(F));
    F.booted = 1;                               /* powered, not yet reset by us */

    qca7k_init(&Q, &PORT, on_rx, NULL);
    run(50);
    CHECK_EQ(F.resets, 1);                      /* soft reset via SPI_CONFIG */
    CHECK(qca7k_is_ready(&Q));
    CHECK_EQ(F.intr_enable, SPI_INT_CPU_ON | SPI_INT_PKT_AVLBL | SPI_INT_RDBUF_ERR | SPI_INT_WRBUF_ERR);
    CHECK_EQ(F.intr_cause, 0);                  /* acknowledged */

    /* receive two frames in one burst */
    for (i = 0; i < sizeof(f1); i++) f1[i] = (uint8_t)i;
    for (i = 0; i < sizeof(f2); i++) f2[i] = (uint8_t)(i * 3);
    inject_rx(f1, sizeof(f1));
    inject_rx(f2, sizeof(f2));
    run(5);
    CHECK_EQ(rx_count, 2);
    CHECK_EQ(rx_lens[0], 60);
    CHECK_EQ(rx_lens[1], 200);
    CHECK_EQ(F.rd_pos, F.rd_len);

    /* transmit: short frame is padded to 60 bytes, framed, BFR_SIZE matches */
    memset(small, 0x5A, sizeof(small));
    CHECK_EQ(qca7k_send(&Q, small, sizeof(small)), 0);
    CHECK_EQ(F.tx_count, 1);
    CHECK_EQ(F.tx_len[0], 8 + 60 + 2);
    CHECK_EQ(F.bfr_mismatch, 0);
    CHECK(F.tx_pkt[0][0] == 0xAA && F.tx_pkt[0][3] == 0xAA);
    CHECK_EQ(plc_get_le16(&F.tx_pkt[0][4]), 60);
    CHECK(F.tx_pkt[0][8] == 0x5A && F.tx_pkt[0][8 + 19] == 0x5A && F.tx_pkt[0][8 + 20] == 0x00);
    CHECK(F.tx_pkt[0][68] == 0x55 && F.tx_pkt[0][69] == 0x55);

    /* re-entrant send from the rx callback */
    echo_in_callback = 1;
    inject_rx(f2, sizeof(f2));
    run(5);
    echo_in_callback = 0;
    CHECK_EQ(rx_count, 3);
    CHECK_EQ(F.tx_count, 2);
    CHECK_EQ(F.tx_len[1], 8 + 200 + 2);
    CHECK(memcmp(&F.tx_pkt[1][8], f2, sizeof(f2)) == 0);

    /* transient SPI trouble (bad signature) -> driver drops sync, then
     * soft-resets the device once the signature reads correctly again */
    F.signature_broken = 1;
    run(QCA7K_SIGNATURE_CHECK_MS + 10);
    CHECK(!qca7k_is_ready(&Q));
    CHECK_EQ(qca7k_send(&Q, small, sizeof(small)), -1);
    F.signature_broken = 0;
    run(QCA7K_REBOOT_TIME_MS + 10);
    CHECK_EQ(F.resets, 2);
    CHECK(qca7k_is_ready(&Q));

    /* polling fallback: no INT line, data still received */
    {
        qca7k_port_t p2 = PORT;
        p2.intr_pending = NULL;
        Q.port = &p2;
        inject_rx(f1, sizeof(f1));
        F.intr_cause = 0;                       /* even if the edge was missed */
        run(QCA7K_INTR_POLL_MS + 2);
        CHECK_EQ(rx_count, 4);
        Q.port = &PORT;
    }
}
