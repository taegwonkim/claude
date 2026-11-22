/*
 * qca7k_port_stm32.h - STM32H7 HAL binding of the QCA7K driver
 */
#ifndef QCA7K_PORT_STM32_H
#define QCA7K_PORT_STM32_H

#include "qca7k.h"

extern const qca7k_port_t qca7k_port_stm32;

/* hardware RNG, used for NMK / RunID */
void stm32_random(void *ctx, uint8_t *buf, uint16_t len);
/* locally administered MAC address derived from the 96 bit device UID */
void stm32_make_mac(uint8_t mac[6], uint8_t role);

#endif
