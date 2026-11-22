/*
 * slac_ev.h - ISO 15118-3 SLAC, EV (vehicle / PEV) side, non-blocking
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Usage (main loop):
 *   slac_ev_init(&s, &io, cb, user, now)   once the QCA7K driver is ready
 *   slac_ev_on_frame(&s, f, len, now)       for every received frame
 *   slac_ev_process(&s, now)                every loop
 *   slac_ev_start(&s, now)                  CP state B with 5 % PWM detected
 *   slac_ev_leave(&s, now)                  unplugged (CP state A) / session end
 */
#ifndef SLAC_EV_H
#define SLAC_EV_H

#include "slac.h"

/* reject an EVSE whose average attenuation is above this (dB). 255 = no limit */
#ifndef SLAC_EV_ATTEN_LIMIT_DB
#define SLAC_EV_ATTEN_LIMIT_DB   255u
#endif
/* how often the whole SLAC sequence is repeated when no EVSE answered */
#ifndef SLAC_EV_SESSION_RETRIES
#define SLAC_EV_SESSION_RETRIES  2u
#endif

typedef enum {
    SLAC_EV_INIT_KEY = 0,       /* random NMK -> not member of any AVLN */
    SLAC_EV_IDLE,
    SLAC_EV_WAIT_PARAM_CNF,     /* CM_SLAC_PARAM.REQ sent (broadcast)    */
    SLAC_EV_START_ATTEN,        /* sending CM_START_ATTEN_CHAR.IND x3    */
    SLAC_EV_SOUNDING,           /* sending CM_MNBC_SOUND.IND x10         */
    SLAC_EV_WAIT_RESULTS,       /* collecting CM_ATTEN_CHAR.IND          */
    SLAC_EV_WAIT_MATCH_CNF,     /* CM_SLAC_MATCH.REQ sent to best EVSE   */
    SLAC_EV_JOIN,               /* CM_SET_KEY with the EVSE's NMK        */
    SLAC_EV_MATCHED,            /* same AVLN as the EVSE                 */
    SLAC_EV_FAILED
} slac_ev_state_t;

typedef struct {
    slac_io_t io;
    slac_event_cb_t cb;
    void *user;

    slac_ev_state_t state;
    uint8_t  start_req;
    uint8_t  retries;            /* retries of the current request      */
    uint8_t  session_retries;    /* repeats of the whole sequence       */
    uint8_t  count;              /* messages sent in START_ATTEN/SOUNDING */
    uint32_t t_deadline;
    uint32_t t_results;          /* end of TT_EV_atten_results          */

    uint8_t  nmk[SLAC_NMK_LEN];  /* key currently programmed / received */
    uint8_t  nid[SLAC_NID_LEN];

    uint8_t  run_id[SLAC_RUNID_LEN];
    uint8_t  pev_id[SLAC_UNIQUE_ID_LEN];
    uint8_t  msound_target[PLC_MAC_LEN];
    uint8_t  fwd_sta[PLC_MAC_LEN];
    uint8_t  num_sounds, time_out, resp_type;

    /* results */
    uint8_t  candidates;
    uint8_t  evse_mac[PLC_MAC_LEN];   /* best (lowest attenuation) EVSE */
    uint8_t  evse_atten;              /* its average attenuation, dB    */
    uint8_t  last_mac[PLC_MAC_LEN];   /* to ignore repeated ATTEN_CHAR */

    uint8_t  frame[HPGP_FRAME_MAX];
} slac_ev_t;

void slac_ev_init(slac_ev_t *s, const slac_io_t *io, slac_event_cb_t cb, void *user, uint32_t now);
void slac_ev_start(slac_ev_t *s, uint32_t now);
void slac_ev_leave(slac_ev_t *s, uint32_t now);
void slac_ev_process(slac_ev_t *s, uint32_t now);
void slac_ev_on_frame(slac_ev_t *s, const uint8_t *f, uint16_t len, uint32_t now);

static inline slac_ev_state_t slac_ev_state(const slac_ev_t *s) { return s->state; }
const char *slac_ev_state_name(slac_ev_state_t st);

#endif /* SLAC_EV_H */
