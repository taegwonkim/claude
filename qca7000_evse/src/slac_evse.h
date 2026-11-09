/*
 * slac_evse.h - ISO 15118-3 SLAC, EVSE side, for QCA7000 + FreeRTOS
 *
 * Flow (EVSE view):
 *   start()  -> CM_SET_KEY.REQ (random NMK) to local QCA7000
 *   EV       -> CM_SLAC_PARM.REQ          EVSE -> CM_SLAC_PARM.CNF
 *   EV       -> CM_START_ATTEN_CHAR.IND x3
 *   EV       -> CM_MNBC_SOUND.IND x N     QCA7000 -> host: CM_ATTEN_PROFILE.IND x N
 *   EVSE     -> CM_ATTEN_CHAR.IND         EV -> CM_ATTEN_CHAR.RSP
 *   EV       -> CM_SLAC_MATCH.REQ         EVSE -> CM_SLAC_MATCH.CNF (NID, NMK)
 *   => MATCHED callback, EV joins the AVLN, start SDP / V2G.
 */
#ifndef SLAC_EVSE_H
#define SLAC_EVSE_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"

typedef enum {
    SLAC_EVSE_EV_MATCHED = 0,   /* CM_SLAC_MATCH.CNF sent */
    SLAC_EVSE_EV_FAILED,        /* timeout / protocol failure (app decides: retry, CP state E/F, ...) */
    SLAC_EVSE_EV_KEY_SET        /* local modem accepted the new NMK */
} slac_evse_event_t;

typedef void (*slac_evse_cb_t)(slac_evse_event_t ev, const uint8_t ev_mac[6], void *user);

/*
 * Timing defaults -- verify against ISO 15118-3 (Table "SLAC timing")
 * for the edition you certify against.
 */
typedef struct {
    uint8_t  host_mac[6];          /* STM32 host MAC (OSA, EVSE_MAC), NOT the QCA7000 MAC */
    uint8_t  modem_mac[6];         /* local QCA7000 MAC for CM_SET_KEY (learn from VS_SW_VER.CNF) */
    uint32_t slac_init_ms;         /* TT_EVSE_SLAC_init: start -> first CM_SLAC_PARM.REQ */
    uint32_t match_session_ms;     /* TT_EVSE_match_session: PARM.REQ -> SLAC_MATCH.REQ */
    uint32_t mnbc_ms;              /* TT_EVSE_match_MNBC: sounding window */
    uint32_t match_response_ms;    /* TT_match_response: wait for CM_ATTEN_CHAR.RSP */
    uint32_t start_atten_wait_ms;  /* PARM.CNF -> START_ATTEN_CHAR.IND */
    uint8_t  num_sounds;           /* C_EV_match_MNBC announced in PARM.CNF */
    uint8_t  atten_char_retries;   /* re-sends of CM_ATTEN_CHAR.IND */
    uint32_t setkey_timeout_ms;
    uint8_t  setkey_retries;
    slac_evse_cb_t on_event;       /* called in SLAC task context */
    void          *user;
} slac_evse_cfg_t;

void slac_evse_default_cfg(slac_evse_cfg_t *cfg);
int  slac_evse_init(const slac_evse_cfg_t *cfg, UBaseType_t prio);

/* Call when CP enters state B with 5 % PWM (HLC) / when the EV is unplugged. */
void slac_evse_start(void);
void slac_evse_stop(void);

/* Update modem MAC once learned (call before slac_evse_start()). */
void slac_evse_set_modem_mac(const uint8_t mac[6]);

/* Feed every received EtherType 0x88E1 frame (from qca7000 rx callback). */
void slac_evse_on_frame(const uint8_t *frame, uint16_t len);

/* Platform hooks (implement in port / app code) */
void slac_port_random(uint8_t *out, uint32_t len);                 /* TRNG */
void slac_port_sha256(const uint8_t *in, uint32_t len, uint8_t out[32]);

#endif /* SLAC_EVSE_H */
