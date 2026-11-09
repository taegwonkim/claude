/*
 * app_example.c - STM32H7 + FreeRTOS integration example (EVSE)
 *
 * Adapt pin/peripheral names to your board. CubeMX generated init code
 * (MX_SPIx_Init, MX_DMA_Init, MX_GPIO_Init, MX_RNG_Init) is assumed.
 */
#include <stdio.h>
#include <string.h>
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#include "qca7000.h"
#include "slac_evse.h"
#include "hpgp_mme.h"

#ifndef APP_USE_LWIP
#define APP_USE_LWIP 1
#endif

#if APP_USE_LWIP
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/ip6_addr.h"
extern err_t qca7000_netif_init(struct netif *netif);
extern void  qca7000_netif_input(struct netif *netif, const uint8_t *frame, uint16_t len);
static struct netif s_plc_netif;
#endif

/* ---- board specific ------------------------------------------------------ */
extern SPI_HandleTypeDef hspi1;
extern RNG_HandleTypeDef hrng;

#define QCA_CS_PORT   GPIOA
#define QCA_CS_PIN    GPIO_PIN_4
#define QCA_INT_PORT  GPIOB
#define QCA_INT_PIN   GPIO_PIN_0     /* EXTI0, rising edge */
#define QCA_RST_PORT  GPIOB
#define QCA_RST_PIN   GPIO_PIN_1

/* Host MAC: use a unique, locally administered or purchased OUI address */
static uint8_t s_host_mac[6] = { 0x02, 0x00, 0x00, 0x12, 0x34, 0x56 };
static uint8_t s_modem_mac[6];
static volatile bool s_modem_known;

/* ========================================================================== */
/*  1. MPU: non-cacheable region for DMA buffers (.dma_buffer in RAM_D2)      */
/* ========================================================================== */
void app_mpu_config(void)
{
    MPU_Region_InitTypeDef r = { 0 };

    HAL_MPU_Disable();
    r.Enable           = MPU_REGION_ENABLE;
    r.Number           = MPU_REGION_NUMBER1;
    r.BaseAddress      = 0x30000000;            /* D2 SRAM1 on H743/H753 - check your part */
    r.Size             = MPU_REGION_SIZE_32KB;
    r.SubRegionDisable = 0x00;
    r.TypeExtField     = MPU_TEX_LEVEL1;        /* TEX=1,C=0,B=0 -> normal, non-cacheable */
    r.AccessPermission = MPU_REGION_FULL_ACCESS;
    r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
    r.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&r);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    /* If D2 SRAM clocks are off on your part: __HAL_RCC_D2SRAM1_CLK_ENABLE(); */
}

/* ========================================================================== */
/*  2. SPI settings for QCA7000 (reference - normally in MX_SPI1_Init)        */
/* ========================================================================== */
void app_spi_init_reference(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_HIGH;     /* Mode 3 */
    hspi1.Init.CLKPhase          = SPI_PHASE_2EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;          /* CS by GPIO */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* choose so SCK <= 8 MHz first */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    hspi1.Init.FifoThreshold     = SPI_FIFO_THRESHOLD_01DATA;
    hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE; /* keep SCK idle high */
    (void)HAL_SPI_Init(&hspi1);
    /*
     * NVIC: EXTI, SPI1 and its DMA streams must have priority numerically
     * >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (e.g. 6 when it is 5),
     * because their ISRs call FreeRTOS *FromISR() APIs.
     */
}

/* ========================================================================== */
/*  3. HAL callbacks -> driver                                                */
/* ========================================================================== */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == QCA_INT_PIN)
        qca7000_irq_isr();
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)   { if (hspi == &hspi1) qca7000_spi_dma_isr(); }
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) { if (hspi == &hspi1) qca7000_spi_dma_isr(); }
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)    { if (hspi == &hspi1) qca7000_spi_error_isr(); }

/* ========================================================================== */
/*  4. SLAC platform hooks                                                    */
/* ========================================================================== */
void slac_port_random(uint8_t *out, uint32_t len)
{
    while (len) {
        uint32_t r = 0, n = len < 4 ? len : 4;
        (void)HAL_RNG_GenerateRandomNumber(&hrng, &r);
        memcpy(out, &r, n);
        out += n;
        len -= n;
    }
}

#include "mbedtls/sha256.h"
void slac_port_sha256(const uint8_t *in, uint32_t len, uint8_t out[32])
{
    (void)mbedtls_sha256(in, len, out, 0);   /* or STM32 HASH peripheral */
}

/* ========================================================================== */
/*  5. Bring-up: VS_SW_VER (same as `plctool -r`)                              */
/* ========================================================================== */
static void send_vs_sw_ver_req(void)
{
    static const uint8_t localcast[6] = { 0x00, 0xB0, 0x52, 0x00, 0x00, 0x01 };
    uint8_t f[60] = { 0 };
    qca_vs_hdr_t *h = (qca_vs_hdr_t *)(void *)f;

    memcpy(h->eth.oda, localcast, 6);
    memcpy(h->eth.osa, s_host_mac, 6);
    hpgp_wr_be16(h->eth.ethertype, ETHERTYPE_HOMEPLUG);
    h->mmv = QCA_MMV_VS;
    hpgp_wr_le16(h->mmtype, VS_SW_VER | MMTYPE_REQ);
    h->oui[0] = 0x00; h->oui[1] = 0xB0; h->oui[2] = 0x52;
    (void)qca7000_send(f, sizeof(f), 50);
}

static void handle_vs_mme(const uint8_t *f, uint16_t len)
{
    const vs_sw_ver_cnf_t *c = (const void *)f;

    if (len < sizeof(qca_vs_hdr_t) + 3u || c->h.mmv != QCA_MMV_VS)
        return;
    if (hpgp_rd_le16(c->h.mmtype) == (VS_SW_VER | MMTYPE_CNF)) {
        uint8_t vl = c->mverlength;
        if (vl > sizeof(c->mversion)) vl = sizeof(c->mversion);
        if (len < sizeof(qca_vs_hdr_t) + 3u + vl) vl = (uint8_t)(len - sizeof(qca_vs_hdr_t) - 3u);
        printf("QCA7000 status=%u dev=0x%02X fw=%.*s\n",
               c->mstatus, c->mdeviceid, (int)vl, (const char *)c->mversion);
        memcpy(s_modem_mac, c->h.eth.osa, 6);   /* modem's own MAC */
        s_modem_known = true;
    }
}

/* ========================================================================== */
/*  6. RX dispatch (runs in qca7000 task)                                     */
/* ========================================================================== */
static void on_qca_rx(const uint8_t *f, uint16_t len, void *arg)
{
    uint16_t et;
    (void)arg;

    if (len < ETH_HDR_LEN)
        return;
    et = (uint16_t)((f[12] << 8) | f[13]);
    if (et == ETHERTYPE_HOMEPLUG) {
        if (f[14] == QCA_MMV_VS) handle_vs_mme(f, len);
        else                     slac_evse_on_frame(f, len);
    }
#if APP_USE_LWIP
    else if (et == ETHERTYPE_IPV6) {
        qca7000_netif_input(&s_plc_netif, f, len);
    }
#endif
}

#if APP_USE_LWIP
static void link_up_cb(void *n)   { netif_set_link_up((struct netif *)n); }
static void link_down_cb(void *n) { netif_set_link_down((struct netif *)n); }
#endif

static void on_qca_state(qca7000_state_t st, void *arg)
{
    (void)arg;
    printf("QCA7000 state %d\n", (int)st);
#if APP_USE_LWIP
    /* netif_set_link_up/down must run in tcpip thread context */
    (void)tcpip_callback(st == QCA7000_READY ? link_up_cb : link_down_cb, &s_plc_netif);
#endif
}

/* ========================================================================== */
/*  7. SLAC events                                                            */
/* ========================================================================== */
static void on_slac_event(slac_evse_event_t ev, const uint8_t mac[6], void *user)
{
    (void)user;
    switch (ev) {
    case SLAC_EVSE_EV_KEY_SET:
        printf("SLAC: NMK set, waiting for EV\n");
        break;
    case SLAC_EVSE_EV_MATCHED:
        printf("SLAC: matched EV %02X:%02X:%02X:%02X:%02X:%02X\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        /* EV joins AVLN shortly (TT_match_join). Start SDP server (UDP 15118) here. */
        break;
    case SLAC_EVSE_EV_FAILED:
        printf("SLAC: failed\n");
        /* ISO 15118-3: e.g. retry, or CP state E/F then back to B, or fall back to basic charging */
        break;
    }
}

/* Call from your CP (control pilot) state machine */
void app_cp_hlc_started(void)  { slac_evse_start(); }   /* State B + 5 % PWM */
void app_cp_unplugged(void)    { slac_evse_stop();  }   /* State A */

/* ========================================================================== */
/*  8. Startup                                                                */
/* ========================================================================== */
static void plc_app_task(void *arg)
{
    (void)arg;

    while (qca7000_get_state() != QCA7000_READY)
        vTaskDelay(pdMS_TO_TICKS(100));

    for (int i = 0; i < 5 && !s_modem_known; i++) {
        send_vs_sw_ver_req();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    if (s_modem_known)
        slac_evse_set_modem_mac(s_modem_mac);
    else
        printf("QCA7000: no VS_SW_VER.CNF (check SPI / firmware)\n");

    for (;;) {
        qca7000_stats_t st;
        qca7000_get_stats(&st);
        printf("qca rx=%lu/%lu tx=%lu/%lu buferr=%lu resets=%lu spierr=%lu\n",
               (unsigned long)st.rx_frames, (unsigned long)st.rx_errors,
               (unsigned long)st.tx_frames, (unsigned long)st.tx_errors,
               (unsigned long)st.buf_errors, (unsigned long)st.device_resets,
               (unsigned long)st.spi_errors);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

#if APP_USE_LWIP
static void netif_setup(void *arg)
{
    (void)arg;
#if LWIP_IPV4
    netif_add(&s_plc_netif, NULL, NULL, NULL, s_host_mac, qca7000_netif_init, tcpip_input);
#else
    netif_add(&s_plc_netif, s_host_mac, qca7000_netif_init, tcpip_input);
#endif
    netif_create_ip6_linklocal_address(&s_plc_netif, 1);
    netif_set_up(&s_plc_netif);
}
#endif

/* Call from main() after HAL/Cube init, before vTaskStartScheduler(). */
void app_plc_init(void)
{
    qca7000_cfg_t qc = {
        .hspi = &hspi1,
        .cs_port = QCA_CS_PORT,   .cs_pin = QCA_CS_PIN,
        .int_port = QCA_INT_PORT, .int_pin = QCA_INT_PIN,
        .rst_port = QCA_RST_PORT, .rst_pin = QCA_RST_PIN,
        .rx_cb = on_qca_rx,
        .state_cb = on_qca_state,
        .cb_arg = NULL,
        .task_prio = configMAX_PRIORITIES - 2,
    };
    slac_evse_cfg_t sc;

    slac_evse_default_cfg(&sc);
    memcpy(sc.host_mac, s_host_mac, 6);
    sc.on_event = on_slac_event;

#if APP_USE_LWIP
    tcpip_init(netif_setup, NULL);
#endif
    (void)slac_evse_init(&sc, configMAX_PRIORITIES - 3);
    (void)qca7000_init(&qc);
    (void)xTaskCreate(plc_app_task, "plcapp", 512, NULL, tskIDLE_PRIORITY + 1, NULL);
}
