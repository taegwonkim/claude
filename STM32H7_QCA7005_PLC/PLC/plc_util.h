/*
 * plc_util.h - small helpers shared by the PLC library (no HAL dependency)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef PLC_UTIL_H
#define PLC_UTIL_H

#include <stdint.h>
#include <string.h>

#ifndef PLC_LOG
#include <stdio.h>
#define PLC_LOG(...) printf(__VA_ARGS__)
#endif

#define PLC_MAC_LEN 6

static inline void plc_put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static inline uint16_t plc_get_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static inline void plc_put_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

static inline uint16_t plc_get_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/* Wrap-safe "a is at or after b" for millisecond ticks */
static inline int plc_time_after_eq(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) >= 0;
}

#define PLC_MAC_FMT     "%02X:%02X:%02X:%02X:%02X:%02X"
#define PLC_MAC_ARG(m)  (m)[0], (m)[1], (m)[2], (m)[3], (m)[4], (m)[5]

#endif /* PLC_UTIL_H */
