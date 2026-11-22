/*
 * qca7k_port_stm32.c - STM32H7 HAL binding of the QCA7K driver
 *
 *  SPI1 polling transfers (no DMA -> no D-cache maintenance needed),
 *  software chip select PD14, INTR on PE13 (EXTI, rising), RESET_L on PE14.
 */
#include <string.h>
#include "main.h"
#include "qca7k_port_stm32.h"

#define CHUNK 256u

static volatile uint8_t s_intr_flag;
static uint8_t s_zero[CHUNK];       /* MOSI = 0x00 while reading */
static uint8_t s_dummy[CHUNK];      /* MISO sink while writing   */

static void port_cs(void *ctx, int active)
{
  (void)ctx;
  HAL_GPIO_WritePin(QCA_CS_PORT, QCA_CS_PIN, active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static int port_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
  (void)ctx;
  while (len)
  {
    uint16_t n = len > CHUNK ? (uint16_t)CHUNK : len;
    uint8_t *t = tx ? (uint8_t *)tx : s_zero;
    uint8_t *r = rx ? rx : s_dummy;

    if (HAL_SPI_TransmitReceive(&hspi1, t, r, n, 50) != HAL_OK)
      return -1;
    if (tx) tx += n;
    if (rx) rx += n;
    len = (uint16_t)(len - n);
  }
  return 0;
}

static int port_intr_pending(void *ctx)
{
  (void)ctx;
  if (s_intr_flag)
  {
    s_intr_flag = 0;
    return 1;
  }
  /* level check catches an edge that came while INTR_ENABLE was being rewritten */
  return HAL_GPIO_ReadPin(QCA_INT_PORT, QCA_INT_PIN) == GPIO_PIN_SET;
}

static void port_reset(void *ctx, int assert)
{
  (void)ctx;
  HAL_GPIO_WritePin(QCA_RST_PORT, QCA_RST_PIN, assert ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static uint32_t port_millis(void *ctx)
{
  (void)ctx;
  return HAL_GetTick();
}

const qca7k_port_t qca7k_port_stm32 = {
  NULL, port_cs, port_xfer, port_intr_pending, port_reset, port_millis
};

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
  if (pin == QCA_INT_PIN)
    s_intr_flag = 1;
}

void stm32_random(void *ctx, uint8_t *buf, uint16_t len)
{
  (void)ctx;
  while (len)
  {
    uint32_t r = 0;
    uint16_t n = len > 4 ? 4 : len;
    if (HAL_RNG_GenerateRandomNumber(&hrng, &r) != HAL_OK)
    {
      /* RNG clock/seed error: fall back to a weak source, keep running */
      r = DWT->CYCCNT * 2654435761u ^ HAL_GetTick();
    }
    memcpy(buf, &r, n);
    buf += n;
    len = (uint16_t)(len - n);
  }
}

void stm32_make_mac(uint8_t mac[6], uint8_t role)
{
  uint32_t a = HAL_GetUIDw0(), b = HAL_GetUIDw1(), c = HAL_GetUIDw2();
  uint32_t h = a ^ (b * 0x9E3779B1u) ^ (c * 0x85EBCA77u);

  mac[0] = 0x02;                    /* locally administered, unicast */
  mac[1] = role;
  mac[2] = (uint8_t)(h >> 24);
  mac[3] = (uint8_t)(h >> 16);
  mac[4] = (uint8_t)(h >> 8);
  mac[5] = (uint8_t)h;
}
