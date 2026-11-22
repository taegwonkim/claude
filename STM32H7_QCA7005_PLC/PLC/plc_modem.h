/*
 * plc_modem.h - local QCA7005 modem queries (firmware version, AVLN status)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * VS_SW_VER  : read once at start-up, proves that the modem firmware runs
 * VS_NW_INFO : polled periodically, number of stations in our AVLN
 *              (> 0 after SLAC means the EV and EVSE modems are linked)
 */
#ifndef PLC_MODEM_H
#define PLC_MODEM_H

#include "slac.h"

#ifndef PLC_MODEM_POLL_MS
#define PLC_MODEM_POLL_MS 1000u
#endif

typedef void (*plc_link_cb_t)(void *user, int up, int stations);

typedef struct {
    slac_io_t io;
    plc_link_cb_t cb;
    void *user;

    uint8_t  modem_mac[PLC_MAC_LEN];
    uint8_t  have_version;
    uint8_t  device_id;
    char     version[80];

    int      stations;          /* -1 unknown */
    uint8_t  nid[SLAC_NID_LEN];
    uint8_t  link_up;
    uint8_t  unanswered;
    uint32_t t_next;

    uint8_t  frame[64];
} plc_modem_t;

void plc_modem_init(plc_modem_t *m, const slac_io_t *io, plc_link_cb_t cb, void *user, uint32_t now);
void plc_modem_process(plc_modem_t *m, uint32_t now);
void plc_modem_on_frame(plc_modem_t *m, const uint8_t *f, uint16_t len, uint32_t now);

#endif /* PLC_MODEM_H */
