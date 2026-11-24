/*
 * din_secc.h - DIN SPEC 70121 charger side (SECC), DC charging, EIM
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Usage, after SLAC matched (same AVLN):
 *   din_secc_start(&s, &hw, cb, user)   -> SDP server + TCP listen
 *   din_secc_process(&s, now)           every loop (after v2g_netif_poll)
 *   din_secc_request_stop(&s)           charger wants to end the session
 *   din_secc_stop(&s)                   unplugged / SLAC lost: abort everything
 */
#ifndef DIN_SECC_H
#define DIN_SECC_H

#include <stdint.h>
#include "v2g_hw.h"
#include "v2g_events.h"
#include "v2gtp.h"

typedef enum {
    SECC_IDLE = 0,
    SECC_LISTEN,        /* waiting for the EV's TCP connection       */
    SECC_APPHAND,       /* waiting for supportedAppProtocolReq        */
    SECC_SESSION,       /* DIN 70121 messages                         */
    SECC_DONE,          /* SessionStop handled                        */
    SECC_FAILED
} secc_phase_t;

typedef enum {
    SECC_STEP_SESSION_SETUP = 0,
    SECC_STEP_SERVICE_DISCOVERY,
    SECC_STEP_PAYMENT_SELECTION,
    SECC_STEP_AUTHORIZATION,
    SECC_STEP_CHARGE_PARAMETER,
    SECC_STEP_CABLE_CHECK,
    SECC_STEP_PRECHARGE,
    SECC_STEP_CHARGING,        /* CurrentDemand loop                    */
    SECC_STEP_WELDING,         /* after PowerDelivery(stop)             */
    SECC_STEP_STOPPED
} secc_step_t;

typedef struct {
    const secc_hw_t *hw;
    v2g_event_cb_t cb;
    void *user;

    secc_phase_t phase;
    secc_step_t  step;
    void        *listen;        /* struct tcp_pcb * */
    void        *conn;
    uint8_t      session_id[8];
    uint8_t      stop_req;
    uint8_t      aborted;
    uint8_t      iso_started, iso_known;
    uint8_t      iso_level;     /* din_isolationLevelType */
    uint32_t     t_last_rx;
    uint32_t     now;
    uint32_t     requests;
    float        target_v, target_i;

    v2gtp_rx_t   rx;
    uint8_t      tx[1024];
} din_secc_t;

int  din_secc_start(din_secc_t *s, const secc_hw_t *hw, v2g_event_cb_t cb, void *user);
void din_secc_stop(din_secc_t *s);
void din_secc_request_stop(din_secc_t *s);
void din_secc_process(din_secc_t *s, uint32_t now);
const char *din_secc_phase_name(secc_phase_t p);

#endif /* DIN_SECC_H */
