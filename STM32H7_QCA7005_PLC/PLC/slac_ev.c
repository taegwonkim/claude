/*
 * slac_ev.c - ISO 15118-3 SLAC, EV (vehicle / PEV) side
 *
 * Message handling corresponds to open-plc-utils slac/pev_cm_*.c
 * (Copyright (c) 2013 Qualcomm Atheros, Inc.), rewritten as a non-blocking
 * state machine with the ISO 15118-3 timeouts and EVSE selection by the
 * lowest average attenuation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include "slac_ev.h"

#define LOG(...) PLC_LOG("[SLAC-EV] " __VA_ARGS__)
#define NEVER    0x40000000u

static void set_state(slac_ev_t *s, slac_ev_state_t st)
{
    if (s->state != st)
        LOG("%s -> %s\r\n", slac_ev_state_name(s->state), slac_ev_state_name(st));
    s->state = st;
}

static void emit(slac_ev_t *s, slac_event_t e, int arg)
{
    if (s->cb)
        s->cb(s->user, e, arg);
}

static int send(slac_ev_t *s, uint16_t len)
{
    if (len < 60) {
        memset(&s->frame[len], 0, 60u - len);
        len = 60;
    }
    if (s->io.send(s->io.ctx, s->frame, len) != 0) {
        LOG("send failed\r\n");
        return -1;
    }
    return 0;
}

static void send_set_key(slac_ev_t *s, uint32_t now)
{
    uint16_t len = hpgp_build_set_key(s->frame, s->io.host_mac, s->nid, s->nmk);
    send(s, len);
    s->t_deadline = now + SLAC_SET_KEY_TIMEOUT_MS;
}

/* random private key: leave whatever AVLN the modem was in */
static void private_key(slac_ev_t *s, uint32_t now)
{
    s->io.random(s->io.ctx, s->nmk, SLAC_NMK_LEN);
    hpgp_nid_from_nmk(s->nid, s->nmk, 0);
    s->retries = 0;
    set_state(s, SLAC_EV_INIT_KEY);
    send_set_key(s, now);
}

static void fail(slac_ev_t *s, slac_fail_t why)
{
    LOG("failed: %s\r\n", slac_fail_name(why));
    set_state(s, SLAC_EV_FAILED);   /* process() parks the timer */
    emit(s, SLAC_EVT_FAILED, why);
}

/* ---- messages -------------------------------------------------------------- */

static void send_param_req(slac_ev_t *s, uint32_t now)
{
    uint8_t *f = s->frame;
    memset(f, 0, SP_REQ_LEN);
    hpgp_hdr(f, HPGP_MAC_BROADCAST, s->io.host_mac, CM_SLAC_PARAM | MMTYPE_REQ);
    f[SP_REQ_APP] = SLAC_APP_TYPE_PEV_EVSE;
    f[SP_REQ_SEC] = SLAC_SECURITY_NONE;
    memcpy(&f[SP_REQ_RUNID], s->run_id, SLAC_RUNID_LEN);
    send(s, SP_REQ_LEN);
    s->t_deadline = now + TT_MATCH_RESPONSE_MS;
}

static void send_start_atten(slac_ev_t *s)
{
    uint8_t *f = s->frame;
    memset(f, 0, SA_IND_LEN);
    hpgp_hdr(f, s->msound_target, s->io.host_mac, CM_START_ATTEN_CHAR | MMTYPE_IND);
    f[SA_IND_APP] = SLAC_APP_TYPE_PEV_EVSE;
    f[SA_IND_SEC] = SLAC_SECURITY_NONE;
    f[SA_IND_NUM_SOUNDS] = s->num_sounds;
    f[SA_IND_TIME_OUT] = s->time_out;
    f[SA_IND_RESP_TYPE] = s->resp_type;
    memcpy(&f[SA_IND_FWD_STA], s->fwd_sta, PLC_MAC_LEN);
    memcpy(&f[SA_IND_RUNID], s->run_id, SLAC_RUNID_LEN);
    send(s, SA_IND_LEN);
}

static void send_sound(slac_ev_t *s, uint8_t cnt)
{
    uint8_t *f = s->frame;
    memset(f, 0, MS_IND_LEN);
    hpgp_hdr(f, s->msound_target, s->io.host_mac, CM_MNBC_SOUND | MMTYPE_IND);
    f[MS_IND_APP] = SLAC_APP_TYPE_PEV_EVSE;
    f[MS_IND_SEC] = SLAC_SECURITY_NONE;
    memcpy(&f[MS_IND_SENDER_ID], s->pev_id, SLAC_UNIQUE_ID_LEN);
    f[MS_IND_CNT] = cnt;                    /* counts down to 0 */
    memcpy(&f[MS_IND_RUNID], s->run_id, SLAC_RUNID_LEN);
    s->io.random(s->io.ctx, &f[MS_IND_RND], SLAC_RND_LEN);
    send(s, MS_IND_LEN);
}

static void send_atten_rsp(slac_ev_t *s, const uint8_t evse_mac[6])
{
    uint8_t *f = s->frame;
    memset(f, 0, AC_RSP_LEN);
    hpgp_hdr(f, evse_mac, s->io.host_mac, CM_ATTEN_CHAR | MMTYPE_RSP);
    f[AC_RSP_APP] = SLAC_APP_TYPE_PEV_EVSE;
    f[AC_RSP_SEC] = SLAC_SECURITY_NONE;
    memcpy(&f[AC_RSP_SRC_ADDR], s->io.host_mac, PLC_MAC_LEN);
    memcpy(&f[AC_RSP_RUNID], s->run_id, SLAC_RUNID_LEN);
    f[AC_RSP_RESULT] = 0;                   /* success */
    send(s, AC_RSP_LEN);
}

static void send_match_req(slac_ev_t *s, uint32_t now)
{
    uint8_t *f = s->frame;
    memset(f, 0, SM_REQ_LEN);
    hpgp_hdr(f, s->evse_mac, s->io.host_mac, CM_SLAC_MATCH | MMTYPE_REQ);
    f[SM_APP] = SLAC_APP_TYPE_PEV_EVSE;
    f[SM_SEC] = SLAC_SECURITY_NONE;
    plc_put_le16(&f[SM_MVF_LEN], SM_REQ_MVF_LEN);
    memcpy(&f[SM_PEV_ID], s->pev_id, SLAC_UNIQUE_ID_LEN);
    memcpy(&f[SM_PEV_MAC], s->io.host_mac, PLC_MAC_LEN);
    /* EVSE_ID: zero */
    memcpy(&f[SM_EVSE_MAC], s->evse_mac, PLC_MAC_LEN);
    memcpy(&f[SM_RUNID], s->run_id, SLAC_RUNID_LEN);
    send(s, SM_REQ_LEN);
    s->t_deadline = now + TT_MATCH_RESPONSE_MS;
}

static void begin_session(slac_ev_t *s, uint32_t now)
{
    s->io.random(s->io.ctx, s->run_id, SLAC_RUNID_LEN);
    memset(s->pev_id, 0, sizeof(s->pev_id));
    s->candidates = 0;
    s->evse_atten = 0xFF;
    s->retries = 0;
    LOG("-> CM_SLAC_PARAM.REQ RunID %02X%02X%02X%02X%02X%02X%02X%02X\r\n",
        s->run_id[0], s->run_id[1], s->run_id[2], s->run_id[3],
        s->run_id[4], s->run_id[5], s->run_id[6], s->run_id[7]);
    set_state(s, SLAC_EV_WAIT_PARAM_CNF);
    send_param_req(s, now);
}

static void evaluate_results(slac_ev_t *s, uint32_t now)
{
    if (s->candidates == 0) {
        if (s->session_retries < SLAC_EV_SESSION_RETRIES) {
            s->session_retries++;
            LOG("no CM_ATTEN_CHAR.IND, repeat SLAC (%u)\r\n", s->session_retries);
            begin_session(s, now);
            return;
        }
        fail(s, SLAC_FAIL_NO_ATTEN_CHAR);
        return;
    }
    if ((unsigned)s->evse_atten + 0u > (unsigned)SLAC_EV_ATTEN_LIMIT_DB) {
        fail(s, SLAC_FAIL_ATTEN_TOO_HIGH);
        return;
    }
    LOG("%u EVSE(s), best " PLC_MAC_FMT " avg %u dB -> CM_SLAC_MATCH.REQ\r\n",
        s->candidates, PLC_MAC_ARG(s->evse_mac), s->evse_atten);
    s->retries = 0;
    set_state(s, SLAC_EV_WAIT_MATCH_CNF);
    send_match_req(s, now);
}

/* ---- public API ------------------------------------------------------------ */

void slac_ev_init(slac_ev_t *s, const slac_io_t *io, slac_event_cb_t cb, void *user, uint32_t now)
{
    memset(s, 0, sizeof(*s));
    s->io = *io;
    s->cb = cb;
    s->user = user;
    private_key(s, now);
}

void slac_ev_start(slac_ev_t *s, uint32_t now)
{
    if (s->state == SLAC_EV_INIT_KEY) {
        s->start_req = 1;
        return;
    }
    if (s->state != SLAC_EV_IDLE && s->state != SLAC_EV_FAILED)
        return;
    s->session_retries = 0;
    emit(s, SLAC_EVT_STARTED, 0);
    begin_session(s, now);
}

void slac_ev_leave(slac_ev_t *s, uint32_t now)
{
    s->start_req = 0;
    if (s->state == SLAC_EV_IDLE)
        return;
    LOG("leave AVLN (new private NMK)\r\n");
    private_key(s, now);
}

void slac_ev_process(slac_ev_t *s, uint32_t now)
{
    if (!plc_time_after_eq(now, s->t_deadline))
        return;

    switch (s->state) {
    case SLAC_EV_INIT_KEY:
    case SLAC_EV_JOIN:
        if (++s->retries > SLAC_SET_KEY_RETRIES) {
            LOG("no CM_SET_KEY.CNF from the modem\r\n");
            emit(s, SLAC_EVT_FAILED, SLAC_FAIL_SET_KEY);
            s->retries = 0;
        }
        send_set_key(s, now);
        break;

    case SLAC_EV_WAIT_PARAM_CNF:
        if (s->retries < C_EV_MATCH_RETRY) {
            s->retries++;
            LOG("repeat CM_SLAC_PARAM.REQ (%u)\r\n", s->retries);
            send_param_req(s, now);
        } else {
            fail(s, SLAC_FAIL_NO_PARAM_CNF);
        }
        break;

    case SLAC_EV_START_ATTEN:
        send_start_atten(s);
        if (++s->count >= C_EV_START_ATTEN_CHAR_INDS) {
            s->count = 0;
            set_state(s, SLAC_EV_SOUNDING);
        }
        s->t_deadline = now + TP_EV_BATCH_MSG_INTERVAL_MS;
        break;

    case SLAC_EV_SOUNDING:
        s->count++;
        send_sound(s, (uint8_t)(s->num_sounds - s->count));
        if (s->count >= s->num_sounds) {
            set_state(s, SLAC_EV_WAIT_RESULTS);
            s->t_deadline = s->t_results;
        } else {
            s->t_deadline = now + TP_EV_BATCH_MSG_INTERVAL_MS;
        }
        break;

    case SLAC_EV_WAIT_RESULTS:
        evaluate_results(s, now);
        break;

    case SLAC_EV_WAIT_MATCH_CNF:
        if (s->retries < C_EV_MATCH_RETRY) {
            s->retries++;
            LOG("repeat CM_SLAC_MATCH.REQ (%u)\r\n", s->retries);
            send_match_req(s, now);
        } else {
            fail(s, SLAC_FAIL_NO_MATCH_CNF);
        }
        break;

    case SLAC_EV_IDLE:
    case SLAC_EV_MATCHED:
    case SLAC_EV_FAILED:
    default:
        s->t_deadline = now + NEVER;
        break;
    }
}

void slac_ev_on_frame(slac_ev_t *s, const uint8_t *f, uint16_t len, uint32_t now)
{
    uint8_t mmv;
    int32_t mmtype = hpgp_mmtype(f, len, &mmv);

    if (mmtype < 0 || mmv != HPGP_MMV_HOMEPLUG)
        return;
    if (!memcmp(&f[HPGP_OFS_OSA], s->io.host_mac, PLC_MAC_LEN))
        return;

    switch ((uint16_t)mmtype) {
    case CM_SET_KEY | MMTYPE_CNF:
        if (s->state == SLAC_EV_INIT_KEY) {
            LOG("CM_SET_KEY.CNF (private key) result %u\r\n", len > SK_CNF_RESULT ? f[SK_CNF_RESULT] : 0xFFu);
            emit(s, SLAC_EVT_KEY_SET, 0);
            set_state(s, SLAC_EV_IDLE);
            s->t_deadline = now + NEVER;
            if (s->start_req) {
                s->start_req = 0;
                s->session_retries = 0;
                emit(s, SLAC_EVT_STARTED, 0);
                begin_session(s, now);
            }
        } else if (s->state == SLAC_EV_JOIN) {
            LOG("CM_SET_KEY.CNF (EVSE key) result %u -> joining AVLN\r\n",
                len > SK_CNF_RESULT ? f[SK_CNF_RESULT] : 0xFFu);
            set_state(s, SLAC_EV_MATCHED);
            s->t_deadline = now + NEVER;
            emit(s, SLAC_EVT_MATCHED, 0);
        }
        break;

    case CM_SLAC_PARAM | MMTYPE_CNF:
        if (s->state != SLAC_EV_WAIT_PARAM_CNF || len < SP_CNF_LEN)
            break;
        if (memcmp(&f[SP_CNF_RUNID], s->run_id, SLAC_RUNID_LEN))
            break;
        memcpy(s->msound_target, &f[SP_CNF_MSOUND_TGT], PLC_MAC_LEN);
        memcpy(s->fwd_sta, &f[SP_CNF_FWD_STA], PLC_MAC_LEN);
        s->num_sounds = f[SP_CNF_NUM_SOUNDS];
        s->time_out = f[SP_CNF_TIME_OUT];
        s->resp_type = f[SP_CNF_RESP_TYPE];
        if (s->num_sounds == 0 || s->num_sounds > 2 * C_EV_MATCH_MNBC)
            s->num_sounds = C_EV_MATCH_MNBC;
        LOG("<- CM_SLAC_PARAM.CNF from " PLC_MAC_FMT ", %u sounds\r\n",
            PLC_MAC_ARG(&f[HPGP_OFS_OSA]), s->num_sounds);
        emit(s, SLAC_EVT_PARAM, 0);
        /* first START_ATTEN now; TT_EV_atten_results runs from here */
        s->count = 0;
        s->t_results = now + TT_EV_ATTEN_RESULTS_MS;
        set_state(s, SLAC_EV_START_ATTEN);
        s->t_deadline = now;
        break;

    case CM_ATTEN_CHAR | MMTYPE_IND: {
        unsigned i, ng, sum = 0, avg;
        const uint8_t *evse = &f[HPGP_OFS_OSA];
        if (len < AC_IND_AAG)
            break;
        if (s->state != SLAC_EV_SOUNDING && s->state != SLAC_EV_WAIT_RESULTS &&
            s->state != SLAC_EV_START_ATTEN && s->state != SLAC_EV_WAIT_MATCH_CNF)
            break;
        if (memcmp(&f[AC_IND_RUNID], s->run_id, SLAC_RUNID_LEN) ||
            memcmp(&f[AC_IND_SRC_ADDR], s->io.host_mac, PLC_MAC_LEN))
            break;
        send_atten_rsp(s, evse);    /* always confirm, also repeats */
        if (s->state == SLAC_EV_WAIT_MATCH_CNF)
            break;
        if (s->candidates && !memcmp(evse, s->last_mac, PLC_MAC_LEN))
            break;                  /* repeat of the same EVSE */
        memcpy(s->last_mac, evse, PLC_MAC_LEN);

        ng = f[AC_IND_NUM_GROUPS];
        if (ng > SLAC_GROUPS)
            ng = SLAC_GROUPS;
        if (len < AC_IND_AAG + ng)
            ng = len - AC_IND_AAG;
        for (i = 0; i < ng; i++)
            sum += f[AC_IND_AAG + i];
        avg = ng ? sum / ng : 255u;
        LOG("<- CM_ATTEN_CHAR.IND " PLC_MAC_FMT " %u sounds, %u groups, avg %u dB\r\n",
            PLC_MAC_ARG(evse), f[AC_IND_NUM_SOUNDS], ng, avg);
        s->candidates++;
        if (avg < s->evse_atten || s->candidates == 1) {
            s->evse_atten = (uint8_t)(avg > 255u ? 255u : avg);
            memcpy(s->evse_mac, evse, PLC_MAC_LEN);
        }
        break;
    }

    case CM_SLAC_MATCH | MMTYPE_CNF:
        if (s->state != SLAC_EV_WAIT_MATCH_CNF || len < SM_CNF_LEN)
            break;
        if (memcmp(&f[SM_RUNID], s->run_id, SLAC_RUNID_LEN) ||
            memcmp(&f[HPGP_OFS_OSA], s->evse_mac, PLC_MAC_LEN))
            break;
        memcpy(s->nid, &f[SM_CNF_NID], SLAC_NID_LEN);
        memcpy(s->nmk, &f[SM_CNF_NMK], SLAC_NMK_LEN);
        LOG("<- CM_SLAC_MATCH.CNF NID %02X%02X%02X%02X%02X%02X%02X -> CM_SET_KEY\r\n",
            s->nid[0], s->nid[1], s->nid[2], s->nid[3], s->nid[4], s->nid[5], s->nid[6]);
        s->retries = 0;
        set_state(s, SLAC_EV_JOIN);
        send_set_key(s, now);
        break;

    default:
        break;
    }
}

const char *slac_ev_state_name(slac_ev_state_t st)
{
    static const char *const n[] = {
        "INIT_KEY", "IDLE", "WAIT_PARAM_CNF", "START_ATTEN", "SOUNDING",
        "WAIT_RESULTS", "WAIT_MATCH_CNF", "JOIN", "MATCHED", "FAILED"
    };
    return (unsigned)st < sizeof(n) / sizeof(n[0]) ? n[st] : "?";
}
