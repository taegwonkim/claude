/*
 * app_plc.c - glue: QCA7005 driver <-> modem queries <-> role (EVSE / EV)
 */
#include <stdio.h>
#include "main.h"
#include "app_plc.h"
#include "cp.h"
#include "qca7k.h"
#include "qca7k_port_stm32.h"

slac_io_t   g_plc_io;
plc_modem_t g_plc_modem;

static qca7k_t  s_qca;
static uint8_t  s_was_ready;
static uint32_t s_t_status;
static uint32_t s_t_led;

static int io_send(void *ctx, const uint8_t *frame, uint16_t len)
{
  int r = qca7k_send((qca7k_t *)ctx, frame, len);
  return r == 0 ? 0 : -1;
}

static void on_rx(void *user, const uint8_t *frame, uint16_t len)
{
  uint32_t now = HAL_GetTick();
  (void)user;
  plc_modem_on_frame(&g_plc_modem, frame, len, now);
  app_role_on_frame(frame, len, now);
  /* IPv6 / V2GTP frames for the higher layers (SDP, DIN 70121, ISO 15118-2)
   * arrive here too: hand frames with ethertype 0x86DD to a TCP/IP stack. */
}

void APP_PLC_Init(void)
{
  uint32_t now = HAL_GetTick();

  printf("\r\n=== STM32H743 + QCA7005 HomePlug GP, role %s ===\r\n", app_role_name());
  printf("SPI1 mode 3 @ %lu kHz, CP ADC zero %d mV, %d mV/V\r\n",
         (unsigned long)(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123) /
                         (2u << (hspi1.Init.BaudRatePrescaler >> SPI_CFG1_MBR_Pos)) / 1000u),
         CP_ADC_ZERO_MV, CP_ADC_MV_PER_V);

  cp_init();

  g_plc_io.ctx = &s_qca;
  g_plc_io.send = io_send;
  g_plc_io.random = stm32_random;
  stm32_make_mac(g_plc_io.host_mac, PLC_ROLE == PLC_ROLE_EVSE ? 0xE5 : 0xEF);
  printf("host MAC " PLC_MAC_FMT "\r\n", PLC_MAC_ARG(g_plc_io.host_mac));

  qca7k_init(&s_qca, &qca7k_port_stm32, on_rx, NULL);   /* pulses RESET_L */
  s_t_status = now + APP_STATUS_PERIOD_MS;
}

static void leds(uint32_t now)
{
  int ready = qca7k_is_ready(&s_qca);
  int matched = ready && app_role_matched();
  int blink = (now / 250u) & 1u;

  HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN,
                    matched && (g_plc_modem.link_up || blink) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, ready && !matched ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN,
                    (!ready && blink) || (ready && app_role_failed()) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void APP_PLC_Process(void)
{
  uint32_t now;
  int ready;

  qca7k_poll(&s_qca);
  now = HAL_GetTick();
  ready = qca7k_is_ready(&s_qca);

  if (ready && !s_was_ready)
  {
    /* first sync or the modem rebooted: its key is back to the PIB default */
    printf("QCA7005 ready -> init modem queries and SLAC\r\n");
    plc_modem_init(&g_plc_modem, &g_plc_io, NULL, NULL, now);
    app_role_init(now);
  }
  s_was_ready = (uint8_t)ready;

  if (ready)
  {
    plc_modem_process(&g_plc_modem, now);
  }
  app_role_process(now);      /* CP handling runs even without the modem */

  if ((int32_t)(now - s_t_led) >= 0)
  {
    s_t_led = now + 50u;
    leds(now);
  }

  if ((int32_t)(now - s_t_status) >= 0)
  {
    s_t_status = now + APP_STATUS_PERIOD_MS;
    printf("[%7lu] QCA %s rx %lu tx %lu err %lu/%lu resets %lu | AVLN %s (%d) | ",
           (unsigned long)now, ready ? "ok " : "---",
           (unsigned long)s_qca.stats.rx_packets, (unsigned long)s_qca.stats.tx_packets,
           (unsigned long)s_qca.stats.rx_errors, (unsigned long)s_qca.stats.tx_errors,
           (unsigned long)s_qca.stats.device_reset,
           g_plc_modem.link_up ? "UP" : "down", g_plc_modem.stations);
    app_role_status();
    printf("\r\n");
  }
}
