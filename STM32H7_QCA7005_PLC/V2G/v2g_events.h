/*
 * v2g_events.h - events reported by the DIN 70121 state machines
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef V2G_EVENTS_H
#define V2G_EVENTS_H

typedef enum {
    V2G_EVT_SDP_DONE = 1,      /* EV: SECC address known                      */
    V2G_EVT_CONNECTED,         /* TCP connection up                            */
    V2G_EVT_PROTOCOL,          /* supportedAppProtocol: DIN 70121 agreed       */
    V2G_EVT_SESSION,           /* SessionSetup done                            */
    V2G_EVT_CABLE_CHECK,       /* insulation test passed                       */
    V2G_EVT_PRECHARGE_DONE,    /* EV: voltages matched                         */
    V2G_EVT_CHARGING,          /* PowerDelivery(start) done, power flows       */
    V2G_EVT_CHARGING_STOPPED,  /* PowerDelivery(stop)                          */
    V2G_EVT_DONE,              /* SessionStop exchanged, TCP closed            */
    V2G_EVT_FAILED             /* arg: v2g_fail_t                              */
} v2g_event_t;

typedef enum {
    V2G_FAIL_NONE = 0,
    V2G_FAIL_SDP,              /* no SECC Discovery Response                   */
    V2G_FAIL_CONNECT,          /* TCP connect failed / timed out               */
    V2G_FAIL_NO_PROTOCOL,      /* no common application protocol               */
    V2G_FAIL_TIMEOUT,          /* response / sequence timeout                  */
    V2G_FAIL_RESPONSE_CODE,    /* FAILED_* response code                       */
    V2G_FAIL_SEQUENCE,         /* unexpected message                           */
    V2G_FAIL_DECODE,           /* EXI could not be decoded                     */
    V2G_FAIL_ISOLATION,        /* insulation fault                             */
    V2G_FAIL_CONNECTION_LOST,  /* TCP closed / reset by the peer               */
    V2G_FAIL_EVSE_STOP         /* EVSE status not ready during charging         */
} v2g_fail_t;

typedef void (*v2g_event_cb_t)(void *user, v2g_event_t evt, int arg);

const char *v2g_fail_name(v2g_fail_t f);

#endif
