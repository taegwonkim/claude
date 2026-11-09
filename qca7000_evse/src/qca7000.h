/*
 * qca7000.h - QCA7000/7005 SPI host driver for STM32H7 + FreeRTOS
 *
 * Protocol reference: Linux drivers/net/ethernet/qualcomm/qca_spi.c,
 *                     qca_7k.c, qca_7k_common.c
 *
 * Threading model:
 *   - One driver task owns the SPI bus exclusively.
 *   - EXTI ISR (QCA7000 INT pin, rising edge) -> qca7000_irq_isr()
 *   - SPI DMA complete / error ISR          -> qca7000_spi_dma_isr() / qca7000_spi_error_isr()
 *   - Any task may transmit via qca7000_send() or tx_alloc()/tx_commit().
 *   - Received Ethernet frames are delivered through cfg.rx_cb in the
 *     driver task context (keep it short: copy & post to another queue).
 */
#ifndef QCA7000_H
#define QCA7000_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"

/* ---- build options ------------------------------------------------------ */
#ifndef QCA7000_LEGACY_MODE
#define QCA7000_LEGACY_MODE     0   /* 0: burst mode (Linux default), 1: legacy (CS toggles after cmd) */
#endif
#ifndef QCA7000_TX_POOL
#define QCA7000_TX_POOL         6   /* number of TX frame buffers */
#endif
#ifndef QCA7000_TASK_STACK
#define QCA7000_TASK_STACK      1024 /* words */
#endif
#ifndef QCA7000_POLL_MS
#define QCA7000_POLL_MS         100 /* sync / missed-IRQ polling period when not READY */
#endif
#ifndef QCA7000_HEALTH_MS
#define QCA7000_HEALTH_MS       1000 /* signature check period when READY */
#endif
#ifndef QCA7000_RESET_TIMEOUT_MS
#define QCA7000_RESET_TIMEOUT_MS 3000 /* wait for CPU_ON after reset */
#endif

#define QCA7000_ETH_MAX_LEN     1518u

typedef enum {
    QCA7000_UNKNOWN = 0,   /* not synchronised */
    QCA7000_RESET,         /* reset issued, waiting for CPU_ON */
    QCA7000_READY          /* frames can be exchanged */
} qca7000_state_t;

typedef void (*qca7000_rx_cb_t)(const uint8_t *frame, uint16_t len, void *arg);
typedef void (*qca7000_state_cb_t)(qca7000_state_t st, void *arg);

typedef struct {
    SPI_HandleTypeDef *hspi;     /* Mode 3, 8 bit, MSB first, NSS soft, <= 8 MHz to start */
    GPIO_TypeDef      *cs_port;  uint16_t cs_pin;
    GPIO_TypeDef      *int_port; uint16_t int_pin;   /* EXTI rising edge */
    GPIO_TypeDef      *rst_port; uint16_t rst_pin;   /* active low, rst_port = NULL if unused */
    qca7000_rx_cb_t    rx_cb;
    qca7000_state_cb_t state_cb;
    void              *cb_arg;
    UBaseType_t        task_prio;
} qca7000_cfg_t;

typedef struct {
    uint32_t rx_frames;
    uint32_t rx_errors;
    uint32_t tx_frames;
    uint32_t tx_errors;
    uint32_t tx_no_space;
    uint32_t buf_errors;      /* RDBUF/WRBUF error interrupts */
    uint32_t device_resets;   /* CPU_ON events that led to READY */
    uint32_t spi_errors;
} qca7000_stats_t;

int  qca7000_init(const qca7000_cfg_t *cfg);
qca7000_state_t qca7000_get_state(void);
void qca7000_get_stats(qca7000_stats_t *out);

/* Zero-copy style TX: allocate buffer, fill Ethernet frame (no FCS), commit. */
uint8_t *qca7000_tx_alloc(uint32_t timeout_ms);
int      qca7000_tx_commit(uint8_t *buf, uint16_t len);
void     qca7000_tx_free(uint8_t *buf);           /* release without sending */

/* Convenience: copy a complete Ethernet frame and queue it. 0 = queued. */
int  qca7000_send(const uint8_t *frame, uint16_t len, uint32_t timeout_ms);

/* ISR hooks */
void qca7000_irq_isr(void);
void qca7000_spi_dma_isr(void);
void qca7000_spi_error_isr(void);

#endif /* QCA7000_H */
