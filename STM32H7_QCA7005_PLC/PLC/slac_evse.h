/*
 * slac_evse.h - ISO 15118-3 SLAC, EVSE (charger) side, non-blocking
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Usage (main loop):
 *   slac_evse_init(&s, &io, cb, user, now)     once the QCA7K driver is ready
 *   slac_evse_on_frame(&s, f, len, now)         for every received frame
 *   slac_evse_process(&s, now)                  every loop
 *   slac_evse_start(&s, now)                    CP state B and 5 % PWM switched on
 *   slac_evse_stop(&s, now)                     CP state A (unplugged) / session end
 */
#ifndef SLAC_EVSE_H
#define SLAC_EVSE_H

#include "slac.h"

typedef enum {
    SLAC_EVSE_INIT_KEY = 0,     /* programming a fresh NMK into the local modem */
    SLAC_EVSE_IDLE,             /* key set, not started                         */
    SLAC_EVSE_WAIT_PARAM,       /* waiting for CM_SLAC_PARAM.REQ                */
    SLAC_EVSE_WAIT_START_ATTEN, /* CNF sent, waiting for CM_START_ATTEN_CHAR    */
    SLAC_EVSE_SOUNDING,         /* collecting CM_ATTEN_PROFILE.IND              */
    SLAC_EVSE_WAIT_ATTEN_RSP,   /* CM_ATTEN_CHAR.IND sent                       */
    SLAC_EVSE_WAIT_MATCH,       /* waiting for CM_SLAC_MATCH.REQ                */
    SLAC_EVSE_MATCHED,          /* CM_SLAC_MATCH.CNF sent - same AVLN as the EV */
    SLAC_EVSE_FAILED            /* TT_EVSE_SLAC_init expired                    */
} slac_evse_state_t;

typedef struct {
    slac_io_t io;
    slac_event_cb_t cb;
    void *user;

    slac_evse_state_t state;
    uint8_t  start_req;          /* start requested while the key was being set */
    uint8_t  retries;
    uint32_t t_deadline;         /* timeout of the current state       */
    uint32_t t_init;             /* end of TT_EVSE_SLAC_init           */

    uint8_t  nmk[SLAC_NMK_LEN];
    uint8_t  nid[SLAC_NID_LEN];

    /* session */
    uint8_t  pev_mac[PLC_MAC_LEN];
    uint8_t  pev_id[SLAC_UNIQUE_ID_LEN];
    uint8_t  run_id[SLAC_RUNID_LEN];
    uint8_t  app_type, sec_type;
    uint8_t  num_sounds, time_out;
    uint8_t  sounds;             /* CM_MNBC_SOUND.IND received   */
    uint8_t  profiles;           /* CM_ATTEN_PROFILE.IND received */
    uint8_t  num_groups;
    uint16_t aag_sum[SLAC_GROUPS];
    uint8_t  aag[SLAC_GROUPS];

    uint8_t  frame[HPGP_FRAME_MAX];
} slac_evse_t;

void slac_evse_init(slac_evse_t *s, const slac_io_t *io, slac_event_cb_t cb, void *user, uint32_t now);
void slac_evse_start(slac_evse_t *s, uint32_t now);
void slac_evse_stop(slac_evse_t *s, uint32_t now);
void slac_evse_process(slac_evse_t *s, uint32_t now);
void slac_evse_on_frame(slac_evse_t *s, const uint8_t *f, uint16_t len, uint32_t now);

static inline slac_evse_state_t slac_evse_state(const slac_evse_t *s) { return s->state; }
const char *slac_evse_state_name(slac_evse_state_t st);

#endif /* SLAC_EVSE_H */
