/*
 * slac.h - ISO 15118-3 SLAC (Signal Level Attenuation Characterization)
 *          definitions shared by the EVSE (charger) and EV (vehicle) side
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Message sequence (ISO 15118-3, HomePlug GP)
 *
 *   EV host                         EVSE host           EVSE modem
 *     |-- CM_SLAC_PARAM.REQ (bcast) -->|
 *     |<-- CM_SLAC_PARAM.CNF ----------|
 *     |-- CM_START_ATTEN_CHAR.IND x3 ->|
 *     |-- CM_MNBC_SOUND.IND x10 ------>|  (modem measures) |
 *     |                                |<- CM_ATTEN_PROFILE.IND (per sound)
 *     |<-- CM_ATTEN_CHAR.IND ----------|  (average attenuation, 58 groups)
 *     |-- CM_ATTEN_CHAR.RSP ---------->|
 *     |-- CM_SLAC_MATCH.REQ ---------->|
 *     |<-- CM_SLAC_MATCH.CNF (NID,NMK)-|
 *     |-- CM_SET_KEY.REQ (to own modem) -> modems join the same AVLN
 */
#ifndef SLAC_H
#define SLAC_H

#include <stdint.h>
#include "hpgp.h"

/* ---- ISO 15118-3 timing / counters (Annex A, SLAC timing parameters) ------ */
#define TT_EVSE_SLAC_INIT_MS        40000u  /* 20..50 s  EVSE waits for CM_SLAC_PARAM.REQ */
#define TT_MATCH_RESPONSE_MS        200u    /* wait for a response message            */
#define TT_MATCH_SEQUENCE_MS        400u    /* gap between messages of one sequence   */
#define TT_EVSE_MATCH_MNBC_MS       600u    /* sounding window (TIME_OUT = 6 x 100 ms) */
#define TT_EV_ATTEN_RESULTS_MS      1200u   /* EV collects CM_ATTEN_CHAR.IND          */
#define TT_EVSE_MATCH_SESSION_MS    10000u  /* EVSE waits for CM_SLAC_MATCH.REQ       */
#define TT_MATCH_JOIN_MS            12000u  /* until the AVLN is formed               */
#define TP_EV_BATCH_MSG_INTERVAL_MS 30u     /* 20..50 ms between START_ATTEN / SOUNDs */
#define TP_MATCH_LEAVE_MS           1000u
#define C_EV_MATCH_RETRY            2u      /* retries of a request                   */
#define C_EV_START_ATTEN_CHAR_INDS  3u
#define C_EV_MATCH_MNBC             10u     /* number of sounds                       */
#define SLAC_TIME_OUT_100MS         (TT_EVSE_MATCH_MNBC_MS / 100u)
#define SLAC_SET_KEY_TIMEOUT_MS     500u
#define SLAC_SET_KEY_RETRIES        3u

/* ---- interface to the link (driver) and the platform -------------------- */
typedef struct {
    void *ctx;
    /* send one ethernet frame; return 0 on success */
    int  (*send)(void *ctx, const uint8_t *frame, uint16_t len);
    /* fill buf with random bytes (hardware RNG) */
    void (*random)(void *ctx, uint8_t *buf, uint16_t len);
    uint8_t host_mac[PLC_MAC_LEN];   /* MAC address of this host (MCU) */
} slac_io_t;

typedef enum {
    SLAC_EVT_KEY_SET = 1,     /* local modem accepted CM_SET_KEY        */
    SLAC_EVT_STARTED,         /* SLAC session running                   */
    SLAC_EVT_PARAM,           /* CM_SLAC_PARAM exchanged                */
    SLAC_EVT_SOUNDING_DONE,   /* EVSE: attenuation profile averaged     */
    SLAC_EVT_MATCHED,         /* NMK/NID exchanged, keys being applied  */
    SLAC_EVT_FAILED           /* see slac_fail_t                        */
} slac_event_t;

typedef enum {
    SLAC_FAIL_NONE = 0,
    SLAC_FAIL_SET_KEY,        /* local modem did not answer CM_SET_KEY  */
    SLAC_FAIL_INIT_TIMEOUT,   /* EVSE: no CM_SLAC_PARAM.REQ in time     */
    SLAC_FAIL_NO_PARAM_CNF,   /* EV: no EVSE answered                   */
    SLAC_FAIL_NO_ATTEN_CHAR,  /* EV: no attenuation result              */
    SLAC_FAIL_NO_ATTEN_RSP,   /* EVSE: EV did not confirm ATTEN_CHAR    */
    SLAC_FAIL_NO_MATCH_REQ,   /* EVSE: EV did not send CM_SLAC_MATCH    */
    SLAC_FAIL_NO_MATCH_CNF,   /* EV: EVSE did not answer CM_SLAC_MATCH  */
    SLAC_FAIL_ATTEN_TOO_HIGH, /* EV: best EVSE above the threshold      */
    SLAC_FAIL_SEND            /* driver could not send                  */
} slac_fail_t;

typedef void (*slac_event_cb_t)(void *user, slac_event_t evt, int arg);

const char *slac_fail_name(slac_fail_t f);

#endif /* SLAC_H */
