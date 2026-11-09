/*
 * qca7000.c - QCA7000/7005 SPI host driver for STM32H7 + FreeRTOS
 *
 * Ported logic from Linux qca_spi.c (sync state machine, IRQ handling,
 * burst/legacy SPI access) and qca_7k_common.c (frame encapsulation).
 */
#include "qca7000.h"

#include <string.h>
#include <stddef.h>
#include "task.h"
#include "queue.h"
#include "semphr.h"

#ifndef QCA7000_LOG
#define QCA7000_LOG(...) do { } while (0)
#endif

/* ---- SPI command word ---------------------------------------------------- */
#define QCA7K_SPI_READ          (1u << 15)
#define QCA7K_SPI_WRITE         (0u << 15)
#define QCA7K_SPI_INTERNAL      (1u << 14)
#define QCA7K_SPI_EXTERNAL      (0u << 14)

/* ---- internal registers -------------------------------------------------- */
#define SPI_REG_BFR_SIZE        0x0100u
#define SPI_REG_WRBUF_SPC_AVA   0x0200u
#define SPI_REG_RDBUF_BYTE_AVA  0x0300u
#define SPI_REG_SPI_CONFIG      0x0400u
#define SPI_REG_SPI_STATUS      0x0500u
#define SPI_REG_INTR_CAUSE      0x0C00u
#define SPI_REG_INTR_ENABLE     0x0D00u
#define SPI_REG_RDBUF_WATERMARK 0x1200u
#define SPI_REG_WRBUF_WATERMARK 0x1300u
#define SPI_REG_SIGNATURE       0x1A00u
#define SPI_REG_ACTION_CTRL     0x1B00u

#define QCASPI_GOOD_SIGNATURE   0xAA55u
#define QCASPI_SLAVE_RESET_BIT  (1u << 6)

#define SPI_INT_WRBUF_BELOW_WM  (1u << 10)
#define SPI_INT_CPU_ON          (1u << 6)
#define SPI_INT_ADDR_ERR        (1u << 3)
#define SPI_INT_WRBUF_ERR       (1u << 2)
#define SPI_INT_RDBUF_ERR       (1u << 1)
#define SPI_INT_PKT_AVLBL       (1u << 0)

#define QCA_INTR_MASK  (SPI_INT_CPU_ON | SPI_INT_PKT_AVLBL | SPI_INT_RDBUF_ERR | SPI_INT_WRBUF_ERR)

/* ---- framing -------------------------------------------------------------- */
#define QCASPI_HW_BUF_LEN       0x0C5Bu   /* 3163 bytes, QCA7000 internal buffer */
#define QCASPI_HW_PKT_LEN       4u        /* length prefix per frame in read buffer */
#define QCAFRM_HEADER_LEN       8u        /* AA AA AA AA LL LL 00 00 */
#define QCAFRM_FOOTER_LEN       2u        /* 55 55 */
#define ETH_MIN_FRAME           60u
#define ETH_HDR                 14u

#define NOTIFY_IRQ              (1u << 0)
#define NOTIFY_TX               (1u << 1)

/*
 * DMA buffers.
 * MUST be placed in RAM reachable by DMA1/2 (AXI SRAM or D2 SRAM1..3, NOT DTCM)
 * and configured as non-cacheable via the MPU (see app_example.c), otherwise
 * D-Cache maintenance is required around every DMA transfer.
 * Linker script example:
 *   .dma_buffer (NOLOAD) : { . = ALIGN(32); *(.dma_buffer) } >RAM_D2
 */
#ifndef QCA7000_DMA_SECTION
#define QCA7000_DMA_SECTION __attribute__((section(".dma_buffer"), aligned(32)))
#endif

#define TX_DMA_LEN  (2u + QCAFRM_HEADER_LEN + QCA7000_ETH_MAX_LEN + QCAFRM_FOOTER_LEN)
#define RX_DMA_LEN  (2u + QCASPI_HW_BUF_LEN)

static uint8_t s_tx_dma[(TX_DMA_LEN + 31u) & ~31u] QCA7000_DMA_SECTION;
static uint8_t s_rx_dma[(RX_DMA_LEN + 31u) & ~31u] QCA7000_DMA_SECTION;
static uint8_t s_rx_cmd[(RX_DMA_LEN + 31u) & ~31u] QCA7000_DMA_SECTION; /* cmd + zeros clocked out during reads */

/* ---- TX pool --------------------------------------------------------------- */
typedef struct {
    uint16_t len;
    uint8_t  data[QCA7000_ETH_MAX_LEN];
} tx_slot_t;

static tx_slot_t     s_pool[QCA7000_TX_POOL];
static QueueHandle_t s_freeq;   /* uint8_t index of free slots */
static QueueHandle_t s_txq;     /* uint8_t index of slots to send */
static tx_slot_t    *s_tx_cur;  /* slot waiting for WRBUF space */

/* ---- driver state ---------------------------------------------------------- */
static qca7000_cfg_t     s_cfg;
static TaskHandle_t      s_task;
static SemaphoreHandle_t s_dma_done;
static volatile uint8_t  s_dma_err;
static volatile qca7000_state_t s_state = QCA7000_UNKNOWN;
static TickType_t        s_reset_start;
static uint8_t           s_bad_sig_cnt;
static qca7000_stats_t   s_stats;

/* ========================================================================== */
/*  Low level SPI                                                             */
/* ========================================================================== */

static inline void cs_low(void)  { HAL_GPIO_WritePin(s_cfg.cs_port, s_cfg.cs_pin, GPIO_PIN_RESET); }
static inline void cs_high(void) { HAL_GPIO_WritePin(s_cfg.cs_port, s_cfg.cs_pin, GPIO_PIN_SET); }

static bool irq_pin_active(void)
{
    return HAL_GPIO_ReadPin(s_cfg.int_port, s_cfg.int_pin) == GPIO_PIN_SET;
}

/* Short polled transfer (register access). No DMA -> no cache concerns. */
static int spi_poll(const uint8_t *tx, uint8_t *rx, uint16_t n)
{
    HAL_StatusTypeDef st;
    if (rx)
        st = HAL_SPI_TransmitReceive(s_cfg.hspi, (uint8_t *)tx, rx, n, 10);
    else
        st = HAL_SPI_Transmit(s_cfg.hspi, (uint8_t *)tx, n, 10);
    if (st != HAL_OK) { s_stats.spi_errors++; return -1; }
    return 0;
}

/* Bulk DMA transfer, blocks the calling (driver) task until completion. */
static int spi_dma(const uint8_t *tx, uint8_t *rx, uint16_t n)
{
    HAL_StatusTypeDef st;

    (void)xSemaphoreTake(s_dma_done, 0);   /* drop stale give */
    s_dma_err = 0;
    if (rx)
        st = HAL_SPI_TransmitReceive_DMA(s_cfg.hspi, (uint8_t *)tx, rx, n);
    else
        st = HAL_SPI_Transmit_DMA(s_cfg.hspi, (uint8_t *)tx, n);
    if (st != HAL_OK) { s_stats.spi_errors++; return -1; }

    if (xSemaphoreTake(s_dma_done, pdMS_TO_TICKS(50)) != pdTRUE) {
        (void)HAL_SPI_Abort(s_cfg.hspi);
        s_stats.spi_errors++;
        return -1;
    }
    if (s_dma_err) { s_stats.spi_errors++; return -1; }
    return 0;
}

static int reg_read(uint16_t reg, uint16_t *val)
{
    uint16_t cmd = (uint16_t)(QCA7K_SPI_READ | QCA7K_SPI_INTERNAL | reg);
    uint8_t tx[4] = { (uint8_t)(cmd >> 8), (uint8_t)cmd, 0, 0 };
    uint8_t rx[4] = { 0 };
    int r;

#if QCA7000_LEGACY_MODE
    cs_low(); r = spi_poll(tx, NULL, 2); cs_high();
    if (r == 0) { cs_low(); r = spi_poll(&tx[2], &rx[2], 2); cs_high(); }
#else
    cs_low(); r = spi_poll(tx, rx, 4); cs_high();
#endif
    if (r == 0)
        *val = (uint16_t)((rx[2] << 8) | rx[3]);
    return r;
}

static int reg_write(uint16_t reg, uint16_t val)
{
    uint16_t cmd = (uint16_t)(QCA7K_SPI_WRITE | QCA7K_SPI_INTERNAL | reg);
    uint8_t tx[4] = { (uint8_t)(cmd >> 8), (uint8_t)cmd, (uint8_t)(val >> 8), (uint8_t)val };
    int r;

#if QCA7000_LEGACY_MODE
    cs_low(); r = spi_poll(tx, NULL, 2); cs_high();
    if (r == 0) { cs_low(); r = spi_poll(&tx[2], NULL, 2); cs_high(); }
#else
    cs_low(); r = spi_poll(tx, NULL, 4); cs_high();
#endif
    return r;
}

/* ========================================================================== */
/*  Sync / reset (mirrors qcaspi_qca7k_sync)                                  */
/* ========================================================================== */

static void set_state(qca7000_state_t st)
{
    if (s_state == st)
        return;
    s_state = st;
    if (st == QCA7000_RESET)
        s_reset_start = xTaskGetTickCount();
    QCA7000_LOG("qca7000: state %d\n", (int)st);
    if (s_cfg.state_cb)
        s_cfg.state_cb(st, s_cfg.cb_arg);
}

static bool signature_ok(void)
{
    uint16_t sig = 0;
    if (reg_read(SPI_REG_SIGNATURE, &sig) == 0 && sig == QCASPI_GOOD_SIGNATURE)
        return true;
    /* second chance, as in Linux driver */
    return reg_read(SPI_REG_SIGNATURE, &sig) == 0 && sig == QCASPI_GOOD_SIGNATURE;
}

static void hard_reset(void)
{
    if (!s_cfg.rst_port)
        return;
    HAL_GPIO_WritePin(s_cfg.rst_port, s_cfg.rst_pin, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(100));
    HAL_GPIO_WritePin(s_cfg.rst_port, s_cfg.rst_pin, GPIO_PIN_SET);
    set_state(QCA7000_RESET);
}

static void soft_reset(void)
{
    uint16_t cfg = 0;
    if (reg_read(SPI_REG_SPI_CONFIG, &cfg) == 0) {
        cfg |= QCASPI_SLAVE_RESET_BIT;
        (void)reg_write(SPI_REG_SPI_CONFIG, cfg);
    }
    set_state(QCA7000_RESET);
}

/* Called on CPU_ON interrupt (QCA7000 finished booting). */
static void sync_cpu_on(void)
{
    uint16_t sig = 0, space = 0;

    /* Read signature twice; first read after boot may be invalid. */
    (void)reg_read(SPI_REG_SIGNATURE, &sig);
    (void)reg_read(SPI_REG_SIGNATURE, &sig);
    if (sig != QCASPI_GOOD_SIGNATURE) {
        set_state(QCA7000_UNKNOWN);
        return;
    }
    /* Write buffer must be completely empty after boot. */
    if (reg_read(SPI_REG_WRBUF_SPC_AVA, &space) != 0 || space != QCASPI_HW_BUF_LEN) {
        set_state(QCA7000_UNKNOWN);
        return;
    }
    s_stats.device_resets++;
    set_state(QCA7000_READY);
}

/* Periodic housekeeping. */
static void sync_update(void)
{
    switch (s_state) {
    case QCA7000_READY:
        if (!signature_ok())
            set_state(QCA7000_UNKNOWN);
        break;

    case QCA7000_UNKNOWN:
        if (!signature_ok()) {
            /* SPI does not answer at all -> only a hardware reset helps */
            if (s_cfg.rst_port && ++s_bad_sig_cnt >= 10) {
                s_bad_sig_cnt = 0;
                hard_reset();
            }
            break;
        }
        s_bad_sig_cnt = 0;
        soft_reset();
        break;

    case QCA7000_RESET:
        if ((xTaskGetTickCount() - s_reset_start) >= pdMS_TO_TICKS(QCA7000_RESET_TIMEOUT_MS)) {
            QCA7000_LOG("qca7000: reset timeout\n");
            set_state(QCA7000_UNKNOWN);
        }
        break;
    }
}

/* ========================================================================== */
/*  RX                                                                        */
/* ========================================================================== */

/*
 * Read buffer layout (per frame):
 *   [HW_LEN 4B][AA AA AA AA][LEN lo][LEN hi][00][00][ Ethernet frame ][55 55]
 * The whole RDBUF_BYTE_AVA amount is read at once, so frames are complete.
 */
static void decode_rx(const uint8_t *b, uint32_t n)
{
    uint32_t i = 0;

    while (n - i >= QCASPI_HW_PKT_LEN + QCAFRM_HEADER_LEN + QCAFRM_FOOTER_LEN) {
        const uint8_t *f = &b[i + QCASPI_HW_PKT_LEN];
        uint16_t len;
        uint32_t need;

        if (f[0] != 0xAA || f[1] != 0xAA || f[2] != 0xAA || f[3] != 0xAA) {
            s_stats.rx_errors++;
            return;
        }
        len  = (uint16_t)(f[4] | (f[5] << 8));
        need = QCASPI_HW_PKT_LEN + QCAFRM_HEADER_LEN + (uint32_t)len + QCAFRM_FOOTER_LEN;
        if (len < ETH_HDR || len > QCA7000_ETH_MAX_LEN || i + need > n) {
            s_stats.rx_errors++;
            return;
        }
        if (f[QCAFRM_HEADER_LEN + len] != 0x55 || f[QCAFRM_HEADER_LEN + len + 1] != 0x55) {
            s_stats.rx_errors++;
            return;
        }

        s_stats.rx_frames++;
        if (s_cfg.rx_cb)
            s_cfg.rx_cb(&f[QCAFRM_HEADER_LEN], len, s_cfg.cb_arg);
        i += need;
    }
}

static int do_rx(void)
{
    uint16_t avail = 0;
    int r;

    if (reg_read(SPI_REG_RDBUF_BYTE_AVA, &avail) != 0)
        return -1;
    if (avail == 0)
        return 0;
    if (avail > QCASPI_HW_BUF_LEN) {
        s_stats.rx_errors++;
        return -1;
    }
    if (reg_write(SPI_REG_BFR_SIZE, avail) != 0)
        return -1;

#if QCA7000_LEGACY_MODE
    cs_low(); r = spi_poll(s_rx_cmd, NULL, 2); cs_high();
    if (r == 0) { cs_low(); r = spi_dma(&s_rx_cmd[2], &s_rx_dma[2], avail); cs_high(); }
#else
    cs_low(); r = spi_dma(s_rx_cmd, s_rx_dma, (uint16_t)(2u + avail)); cs_high();
#endif
    if (r != 0)
        return -1;

    decode_rx(&s_rx_dma[2], avail);
    return 0;
}

/* ========================================================================== */
/*  TX                                                                        */
/* ========================================================================== */

/* return: 0 sent, 1 no space (retry later), -1 error */
static int tx_frame(const uint8_t *frame, uint16_t len)
{
    uint16_t eth_len = (len < ETH_MIN_FRAME) ? ETH_MIN_FRAME : len;
    uint16_t total   = (uint16_t)(QCAFRM_HEADER_LEN + eth_len + QCAFRM_FOOTER_LEN);
    uint16_t space   = 0;
    uint8_t *p       = &s_tx_dma[2];
    uint16_t cmd     = (uint16_t)(QCA7K_SPI_WRITE | QCA7K_SPI_EXTERNAL);
    int r;

    if (reg_read(SPI_REG_WRBUF_SPC_AVA, &space) != 0)
        return -1;
    if (space < total) {
        s_stats.tx_no_space++;
        return 1;
    }

    p[0] = p[1] = p[2] = p[3] = 0xAA;
    p[4] = (uint8_t)eth_len;
    p[5] = (uint8_t)(eth_len >> 8);
    p[6] = p[7] = 0x00;
    memcpy(&p[QCAFRM_HEADER_LEN], frame, len);
    if (eth_len > len)
        memset(&p[QCAFRM_HEADER_LEN + len], 0, (size_t)(eth_len - len));
    p[QCAFRM_HEADER_LEN + eth_len]     = 0x55;
    p[QCAFRM_HEADER_LEN + eth_len + 1] = 0x55;

    if (reg_write(SPI_REG_BFR_SIZE, total) != 0)
        return -1;

    s_tx_dma[0] = (uint8_t)(cmd >> 8);
    s_tx_dma[1] = (uint8_t)cmd;
#if QCA7000_LEGACY_MODE
    cs_low(); r = spi_poll(s_tx_dma, NULL, 2); cs_high();
    if (r == 0) { cs_low(); r = spi_dma(&s_tx_dma[2], NULL, total); cs_high(); }
#else
    cs_low(); r = spi_dma(s_tx_dma, NULL, (uint16_t)(2u + total)); cs_high();
#endif
    return r;
}

static void slot_release(tx_slot_t *slot)
{
    uint8_t idx = (uint8_t)(slot - s_pool);
    (void)xQueueSend(s_freeq, &idx, 0);
}

static void tx_drain(void)
{
    for (;;) {
        int r;

        if (!s_tx_cur) {
            uint8_t idx;
            if (xQueueReceive(s_txq, &idx, 0) != pdTRUE)
                return;
            s_tx_cur = &s_pool[idx];
        }

        r = tx_frame(s_tx_cur->data, s_tx_cur->len);
        if (r > 0)
            return;                 /* no space yet, keep s_tx_cur */

        if (r == 0) s_stats.tx_frames++;
        else        s_stats.tx_errors++;

        slot_release(s_tx_cur);
        s_tx_cur = NULL;

        if (r < 0) {
            set_state(QCA7000_UNKNOWN);
            return;
        }
    }
}

/* ========================================================================== */
/*  Interrupt service (task context)                                          */
/* ========================================================================== */

static void handle_irq(void)
{
    uint16_t cause = 0;

    if (reg_write(SPI_REG_INTR_ENABLE, 0) != 0 ||
        reg_read(SPI_REG_INTR_CAUSE, &cause) != 0) {
        set_state(QCA7000_UNKNOWN);
        return;
    }

    if (cause & SPI_INT_CPU_ON) {
        sync_cpu_on();
        if (s_state != QCA7000_READY)
            return;             /* sync_update() will retry */
    }

    if (s_state != QCA7000_READY)
        return;

    if (cause & (SPI_INT_RDBUF_ERR | SPI_INT_WRBUF_ERR)) {
        s_stats.buf_errors++;
        set_state(QCA7000_UNKNOWN);   /* -> soft reset on next sync_update */
        return;
    }

    if (cause & SPI_INT_PKT_AVLBL) {
        if (do_rx() != 0) {
            set_state(QCA7000_UNKNOWN);
            return;
        }
    }

    (void)reg_write(SPI_REG_INTR_CAUSE, cause);         /* acknowledge */
    (void)reg_write(SPI_REG_INTR_ENABLE, QCA_INTR_MASK);
}

/* ========================================================================== */
/*  Task                                                                      */
/* ========================================================================== */

static void qca_task(void *arg)
{
    TickType_t last_sync = xTaskGetTickCount();
    (void)arg;

    if (s_cfg.rst_port)
        hard_reset();            /* -> RESET, wait for CPU_ON */
    else
        set_state(QCA7000_UNKNOWN);

    for (;;) {
        uint32_t   bits = 0;
        TickType_t wait = s_tx_cur ? 1 : pdMS_TO_TICKS(QCA7000_POLL_MS);
        TickType_t now;
        TickType_t period;

        (void)xTaskNotifyWait(0, UINT32_MAX, &bits, wait);

        /* Edge-triggered INT: also check the pin level to catch missed edges. */
        if ((bits & NOTIFY_IRQ) || (s_state == QCA7000_READY && irq_pin_active())) {
            int guard = 8;
            do {
                handle_irq();
            } while (s_state == QCA7000_READY && irq_pin_active() && --guard);
        }

        now    = xTaskGetTickCount();
        period = pdMS_TO_TICKS(s_state == QCA7000_READY ? QCA7000_HEALTH_MS : QCA7000_POLL_MS);
        if ((now - last_sync) >= period) {
            last_sync = now;
            sync_update();
        }

        if (s_state == QCA7000_READY)
            tx_drain();
    }
}

/* ========================================================================== */
/*  Public API                                                                */
/* ========================================================================== */

int qca7000_init(const qca7000_cfg_t *cfg)
{
    uint16_t cmd = (uint16_t)(QCA7K_SPI_READ | QCA7K_SPI_EXTERNAL);
    uint8_t i;

    if (!cfg || !cfg->hspi || !cfg->cs_port || !cfg->int_port)
        return -1;
    s_cfg = *cfg;
    memset(&s_stats, 0, sizeof(s_stats));

    memset(s_rx_cmd, 0, sizeof(s_rx_cmd));
    s_rx_cmd[0] = (uint8_t)(cmd >> 8);
    s_rx_cmd[1] = (uint8_t)cmd;

    cs_high();

    s_dma_done = xSemaphoreCreateBinary();
    s_freeq    = xQueueCreate(QCA7000_TX_POOL, sizeof(uint8_t));
    s_txq      = xQueueCreate(QCA7000_TX_POOL, sizeof(uint8_t));
    if (!s_dma_done || !s_freeq || !s_txq)
        return -1;
    for (i = 0; i < QCA7000_TX_POOL; i++)
        (void)xQueueSend(s_freeq, &i, 0);

    if (xTaskCreate(qca_task, "qca7000", QCA7000_TASK_STACK, NULL,
                    cfg->task_prio, &s_task) != pdPASS)
        return -1;
    return 0;
}

qca7000_state_t qca7000_get_state(void) { return s_state; }

void qca7000_get_stats(qca7000_stats_t *out)
{
    taskENTER_CRITICAL();
    *out = s_stats;
    taskEXIT_CRITICAL();
}

uint8_t *qca7000_tx_alloc(uint32_t timeout_ms)
{
    uint8_t idx;
    if (xQueueReceive(s_freeq, &idx, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
        return NULL;
    return s_pool[idx].data;
}

static tx_slot_t *slot_of(uint8_t *buf)
{
    return (tx_slot_t *)(void *)(buf - offsetof(tx_slot_t, data));
}

void qca7000_tx_free(uint8_t *buf)
{
    if (buf)
        slot_release(slot_of(buf));
}

int qca7000_tx_commit(uint8_t *buf, uint16_t len)
{
    tx_slot_t *slot;
    uint8_t idx;

    if (!buf)
        return -1;
    slot = slot_of(buf);
    if (len < ETH_HDR || len > QCA7000_ETH_MAX_LEN || s_state != QCA7000_READY) {
        slot_release(slot);
        return -1;
    }
    slot->len = len;
    idx = (uint8_t)(slot - s_pool);
    (void)xQueueSend(s_txq, &idx, 0);   /* cannot be full: same depth as pool */
    (void)xTaskNotify(s_task, NOTIFY_TX, eSetBits);
    return 0;
}

int qca7000_send(const uint8_t *frame, uint16_t len, uint32_t timeout_ms)
{
    uint8_t *buf;

    if (len < ETH_HDR || len > QCA7000_ETH_MAX_LEN)
        return -1;
    buf = qca7000_tx_alloc(timeout_ms);
    if (!buf)
        return -1;
    memcpy(buf, frame, len);
    return qca7000_tx_commit(buf, len);
}

/* ---- ISR hooks ---------------------------------------------------------- */

void qca7000_irq_isr(void)
{
    BaseType_t woken = pdFALSE;
    if (s_task)
        (void)xTaskNotifyFromISR(s_task, NOTIFY_IRQ, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

void qca7000_spi_dma_isr(void)
{
    BaseType_t woken = pdFALSE;
    if (s_dma_done)
        (void)xSemaphoreGiveFromISR(s_dma_done, &woken);
    portYIELD_FROM_ISR(woken);
}

void qca7000_spi_error_isr(void)
{
    s_dma_err = 1;
    qca7000_spi_dma_isr();
}
