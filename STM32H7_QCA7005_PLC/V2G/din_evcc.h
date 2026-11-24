/*
 * din_evcc.h - DIN SPEC 70121 vehicle side (EVCC), DC charging, EIM
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Usage, after SLAC matched (same AVLN):
 *   din_evcc_start(&e, &hw, cb, user, now)  -> SDP, TCP connect, whole session
 *   din_evcc_process(&e, now)               every loop (after v2g_netif_poll)
 *   din_evcc_request_stop(&e)               driver / BMS wants to stop early
 *   din_evcc_stop(&e)                       unplugged: abort, open S2/contactor
 */
#ifndef DIN_EVCC_H
#define DIN_EVCC_H

#include <stdint.h>
#include "v2g_hw.h"
#include "v2g_events.h"
#include "v2g_sdp.h"
#include "v2gtp.h"

typedef enum {
    EVCC_IDLE = 0,
    EVCC_SDP,           /* looking for the charger              */
    EVCC_CONNECTING,    /* TCP connect                           */
    EVCC_SESSION,       /* supportedAppProtocol + DIN messages   */
    EVCC_DONE,
    EVCC_FAILED
} evcc_phase_t;

typedef enum {
    EVCC_REQ_APPHAND = 0,
    EVCC_REQ_SESSION_SETUP,
    EVCC_REQ_SERVICE_DISCOVERY,
    EVCC_REQ_PAYMENT_SELECTION,
    EVCC_REQ_AUTHORIZATION,
    EVCC_REQ_CHARGE_PARAMETER,
    EVCC_REQ_CABLE_CHECK,
    EVCC_REQ_PRECHARGE,
    EVCC_REQ_POWER_DELIVERY_ON,
    EVCC_REQ_CURRENT_DEMAND,
    EVCC_REQ_POWER_DELIVERY_OFF,
    EVCC_REQ_WELDING,
    EVCC_REQ_SESSION_STOP,
    EVCC_REQ_NONE
} evcc_req_t;

typedef struct {
    const evcc_hw_t *hw;
    v2g_event_cb_t cb;
    void *user;
    uint8_t mac[6];             /* EVCCID */

    evcc_phase_t phase;
    evcc_req_t   req;           /* request to send / in flight */
    uint8_t      waiting;       /* response outstanding */
    uint8_t      stop_req;
    uint8_t      aborted;
    uint8_t      welding_count;
    uint16_t     service_id;
    uint8_t      session_id[8];
    uint8_t      session_id_len;
    uint32_t     now;
    uint32_t     t_next;        /* send the next request */
    uint32_t     t_deadline;    /* response timeout */
    uint32_t     t_phase;       /* start of CableCheck / PreCharge */
    uint32_t     t_log;
    uint32_t     cd_count;      /* CurrentDemand cycles */
    float        evse_v, evse_i;
    float        evse_max_v, evse_max_i, evse_max_p;

    v2g_sdp_client_t sdp;
    void        *conn;          /* struct tcp_pcb * */
    v2gtp_rx_t   rx;
    uint8_t      tx[1024];
} din_evcc_t;

void din_evcc_start(din_evcc_t *e, const evcc_hw_t *hw, const uint8_t mac[6],
                    v2g_event_cb_t cb, void *user, uint32_t now);
void din_evcc_stop(din_evcc_t *e);
void din_evcc_request_stop(din_evcc_t *e);
void din_evcc_process(din_evcc_t *e, uint32_t now);
const char *din_evcc_phase_name(evcc_phase_t p);
const char *din_evcc_req_name(evcc_req_t r);

#endif /* DIN_EVCC_H */
