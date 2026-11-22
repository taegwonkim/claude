/*
 * slac_evse.c - ISO 15118-3 SLAC, EVSE (charger) side
 *
 * Message handling corresponds to open-plc-utils slac/evse_cm_*.c
 * (Copyright (c) 2013 Qualcomm Atheros, Inc.), rewritten as a non-blocking
 * state machine with the ISO 15118-3 timeouts.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include "slac_evse.h"

#define LOG(...) PLC_LOG("[SLAC-EVSE] " __VA_ARGS__)

static void set_state(slac_evse_t *s, slac_evse_state_t st)
{
    if (s->state != st)
        LOG("%s -> %s\r\n", slac_evse_state_name(s->state), slac_evse_state_name(st));
    s->state = st;
}

static void emit(slac_evse_t *s, slac_event_t e, int arg)
{
    if (s->cb)
        s->cb(s->user, e, arg);
}

static int send(slac_evse_t *s, uint16_t len)
{
    if (len < 60) {                         /* explicit padding, nicer in traces */
        memset(&s->frame[len], 0, 60u - len);
        len = 60;
    }
    if (s->io.send(s->io.ctx, s->frame, len) != 0) {
        LOG("send failed\r\n");
        return -1;
    }
    return 0;
}

/* ---- key handling -------------------------------------------------------- */

static void send_set_key(slac_evse_t *s, uint32_t now)
{
    uint16_t len = hpgp_build_set_key(s->frame, s->io.host_mac, s->nid, s->nmk);
    send(s, len);
    s->t_deadline = now + SLAC_SET_KEY_TIMEOUT_MS;
}

static void new_key(slac_evse_t *s, uint32_t now)
{
    s->io.random(s->io.ctx, s->nmk, SLAC_NMK_LEN);
    hpgp_nid_from_nmk(s->nid, s->nmk, 0);
    s->retries = 0;
    set_state(s, SLAC_EVSE_INIT_KEY);
    send_set_key(s, now);
}

/* ---- session helpers ----------------------------------------------------- */

static void wait_param(slac_evse_t *s, uint32_t now)
{
    (void)now;
    set_state(s, SLAC_EVSE_WAIT_PARAM);
    s->t_deadline = s->t_init;
}

static void send_param_cnf(slac_evse_t *s)
{
    uint8_t *f = s->frame;
    memset(f, 0, SP_CNF_LEN);
    hpgp_hdr(f, s->pev_mac, s->io.host_mac, CM_SLAC_PARAM | MMTYPE_CNF);
    memcpy(&f[SP_CNF_MSOUND_TGT], HPGP_MAC_BROADCAST, PLC_MAC_LEN);
    f[SP_CNF_NUM_SOUNDS] = C_EV_MATCH_MNBC;
    f[SP_CNF_TIME_OUT] = SLAC_TIME_OUT_100MS;
    f[SP_CNF_RESP_TYPE] = SLAC_RESP_TYPE_OTHER;
    memcpy(&f[SP_CNF_FWD_STA], s->pev_mac, PLC_MAC_LEN);
    f[SP_CNF_APP] = s->app_type;
    f[SP_CNF_SEC] = s->sec_type;
    memcpy(&f[SP_CNF_RUNID], s->run_id, SLAC_RUNID_LEN);
    send(s, SP_CNF_LEN);
}

static void send_atten_char(slac_evse_t *s)
{
    uint8_t *f = s->frame;
    memset(f, 0, AC_IND_LEN);
    hpgp_hdr(f, s->pev_mac, s->io.host_mac, CM_ATTEN_CHAR | MMTYPE_IND);
    f[AC_IND_APP] = s->app_type;
    f[AC_IND_SEC] = s->sec_type;
    memcpy(&f[AC_IND_SRC_ADDR], s->pev_mac, PLC_MAC_LEN);
    memcpy(&f[AC_IND_RUNID], s->run_id, SLAC_RUNID_LEN);
    /* SOURCE_ID, RESP_ID: zero */
    f[AC_IND_NUM_SOUNDS] = s->profiles;
    f[AC_IND_NUM_GROUPS] = s->num_groups;
    memcpy(&f[AC_IND_AAG], s->aag, s->num_groups);
    send(s, AC_IND_LEN);
}

static void send_match_cnf(slac_evse_t *s)
{
    uint8_t *f = s->frame;
    memset(f, 0, SM_CNF_LEN);
    hpgp_hdr(f, s->pev_mac, s->io.host_mac, CM_SLAC_MATCH | MMTYPE_CNF);
    f[SM_APP] = s->app_type;
    f[SM_SEC] = s->sec_type;
    plc_put_le16(&f[SM_MVF_LEN], SM_CNF_MVF_LEN);
    memcpy(&f[SM_PEV_ID], s->pev_id, SLAC_UNIQUE_ID_LEN);
    memcpy(&f[SM_PEV_MAC], s->pev_mac, PLC_MAC_LEN);
    /* EVSE_ID: zero */
    memcpy(&f[SM_EVSE_MAC], s->io.host_mac, PLC_MAC_LEN);
    memcpy(&f[SM_RUNID], s->run_id, SLAC_RUNID_LEN);
    memcpy(&f[SM_CNF_NID], s->nid, SLAC_NID_LEN);
    memcpy(&f[SM_CNF_NMK], s->nmk, SLAC_NMK_LEN);
    send(s, SM_CNF_LEN);
}

static void finish_sounding(slac_evse_t *s, uint32_t now)
{
    unsigned i, sum = 0;

    if (s->profiles) {
        for (i = 0; i < SLAC_GROUPS; i++)
            s->aag[i] = (uint8_t)(s->aag_sum[i] / s->profiles);
    }
    for (i = 0; i < s->num_groups; i++)
        sum += s->aag[i];

    LOG("sounding done: %u sounds, %u profiles, %u groups, avg %u dB\r\n",
        s->sounds, s->profiles, s->num_groups, s->num_groups ? sum / s->num_groups : 0u);
    emit(s, SLAC_EVT_SOUNDING_DONE, s->num_groups ? (int)(sum / s->num_groups) : -1);

    s->retries = 0;
    send_atten_char(s);
    set_state(s, SLAC_EVSE_WAIT_ATTEN_RSP);
    s->t_deadline = now + TT_MATCH_RESPONSE_MS;
}

static int is_started(const slac_evse_t *s)
{
    return s->state >= SLAC_EVSE_WAIT_PARAM && s->state <= SLAC_EVSE_MATCHED;
}

/* ---- public API ------------------------------------------------------------ */

void slac_evse_init(slac_evse_t *s, const slac_io_t *io, slac_event_cb_t cb, void *user, uint32_t now)
{
    memset(s, 0, sizeof(*s));
    s->io = *io;
    s->cb = cb;
    s->user = user;
    s->state = SLAC_EVSE_INIT_KEY;
    new_key(s, now);
}

void slac_evse_start(slac_evse_t *s, uint32_t now)
{
    s->t_init = now + TT_EVSE_SLAC_INIT_MS;
    if (s->state == SLAC_EVSE_INIT_KEY) {
        s->start_req = 1;           /* start once the modem has the key */
        return;
    }
    if (is_started(s))
        return;
    LOG("start, waiting for CM_SLAC_PARAM.REQ (%u s)\r\n", (unsigned)(TT_EVSE_SLAC_INIT_MS / 1000u));
    emit(s, SLAC_EVT_STARTED, 0);
    wait_param(s, now);
}

void slac_evse_stop(slac_evse_t *s, uint32_t now)
{
    s->start_req = 0;
    if (s->state == SLAC_EVSE_IDLE)
        return;
    LOG("stop, new NMK (old AVLN is left)\r\n");
    new_key(s, now);
}

void slac_evse_process(slac_evse_t *s, uint32_t now)
{
    if (!plc_time_after_eq(now, s->t_deadline))
        return;

    switch (s->state) {
    case SLAC_EVSE_INIT_KEY:
        if (++s->retries >= SLAC_SET_KEY_RETRIES) {
            LOG("no CM_SET_KEY.CNF from the modem\r\n");
            emit(s, SLAC_EVT_FAILED, SLAC_FAIL_SET_KEY);
            s->retries = 0;
        }
        send_set_key(s, now);       /* keep trying, the modem may still boot */
        break;

    case SLAC_EVSE_WAIT_PARAM:
        LOG("TT_EVSE_SLAC_init expired\r\n");
        set_state(s, SLAC_EVSE_FAILED);
        emit(s, SLAC_EVT_FAILED, SLAC_FAIL_INIT_TIMEOUT);
        break;

    case SLAC_EVSE_WAIT_START_ATTEN:
        LOG("no CM_START_ATTEN_CHAR.IND\r\n");
        wait_param(s, now);         /* the EV may repeat CM_SLAC_PARAM */
        break;

    case SLAC_EVSE_SOUNDING:
        finish_sounding(s, now);
        break;

    case SLAC_EVSE_WAIT_ATTEN_RSP:
        if (s->retries < C_EV_MATCH_RETRY) {
            s->retries++;
            LOG("repeat CM_ATTEN_CHAR.IND (%u)\r\n", s->retries);
            send_atten_char(s);
            s->t_deadline = now + TT_MATCH_RESPONSE_MS;
        } else {
            emit(s, SLAC_EVT_FAILED, SLAC_FAIL_NO_ATTEN_RSP);
            wait_param(s, now);
        }
        break;

    case SLAC_EVSE_WAIT_MATCH:
        emit(s, SLAC_EVT_FAILED, SLAC_FAIL_NO_MATCH_REQ);
        wait_param(s, now);
        break;

    case SLAC_EVSE_IDLE:
    case SLAC_EVSE_MATCHED:
    case SLAC_EVSE_FAILED:
    default:
        s->t_deadline = now + 0x40000000u;   /* nothing scheduled */
        break;
    }
}

void slac_evse_on_frame(slac_evse_t *s, const uint8_t *f, uint16_t len, uint32_t now)
{
    uint8_t mmv;
    int32_t mmtype = hpgp_mmtype(f, len, &mmv);
    unsigned i;

    if (mmtype < 0 || mmv != HPGP_MMV_HOMEPLUG)
        return;
    if (!memcmp(&f[HPGP_OFS_OSA], s->io.host_mac, PLC_MAC_LEN))
        return;                     /* own frame */

    switch ((uint16_t)mmtype) {
    case CM_SET_KEY | MMTYPE_CNF:
        if (s->state != SLAC_EVSE_INIT_KEY)
            break;
        /* QCA firmware answers RESULT=1 on success (the HomePlug spec says 0),
         * so the value is only logged */
        LOG("CM_SET_KEY.CNF result %u, NID %02X%02X%02X%02X%02X%02X%02X\r\n",
            len > SK_CNF_RESULT ? f[SK_CNF_RESULT] : 0xFFu,
            s->nid[0], s->nid[1], s->nid[2], s->nid[3], s->nid[4], s->nid[5], s->nid[6]);
        emit(s, SLAC_EVT_KEY_SET, 0);
        set_state(s, SLAC_EVSE_IDLE);
        s->t_deadline = now + 0x40000000u;
        if (s->start_req) {
            s->start_req = 0;
            emit(s, SLAC_EVT_STARTED, 0);
            wait_param(s, now);
        }
        break;

    case CM_SLAC_PARAM | MMTYPE_REQ:
        if (!is_started(s) || len < SP_REQ_LEN)
            break;
        if (f[SP_REQ_APP] != SLAC_APP_TYPE_PEV_EVSE)
            break;
        memcpy(s->pev_mac, &f[HPGP_OFS_OSA], PLC_MAC_LEN);
        memcpy(s->run_id, &f[SP_REQ_RUNID], SLAC_RUNID_LEN);
        s->app_type = f[SP_REQ_APP];
        s->sec_type = f[SP_REQ_SEC];
        memset(s->pev_id, 0, sizeof(s->pev_id));
        LOG("<- CM_SLAC_PARAM.REQ from " PLC_MAC_FMT "\r\n", PLC_MAC_ARG(s->pev_mac));
        send_param_cnf(s);
        emit(s, SLAC_EVT_PARAM, 0);
        set_state(s, SLAC_EVSE_WAIT_START_ATTEN);
        s->t_deadline = now + TT_MATCH_SEQUENCE_MS + TT_MATCH_RESPONSE_MS;
        break;

    case CM_START_ATTEN_CHAR | MMTYPE_IND:
        if (s->state != SLAC_EVSE_WAIT_START_ATTEN || len < SA_IND_LEN)
            break;                  /* repeats while sounding are ignored */
        if (memcmp(&f[SA_IND_RUNID], s->run_id, SLAC_RUNID_LEN))
            break;
        s->num_sounds = f[SA_IND_NUM_SOUNDS];
        s->time_out = f[SA_IND_TIME_OUT];
        if (s->num_sounds == 0 || s->num_sounds > 2 * C_EV_MATCH_MNBC)
            s->num_sounds = C_EV_MATCH_MNBC;
        if (s->time_out == 0 || s->time_out > 50)
            s->time_out = SLAC_TIME_OUT_100MS;
        s->sounds = 0;
        s->profiles = 0;
        s->num_groups = 0;
        memset(s->aag_sum, 0, sizeof(s->aag_sum));
        memset(s->aag, 0, sizeof(s->aag));
        LOG("<- CM_START_ATTEN_CHAR.IND sounds %u timeout %u00 ms\r\n", s->num_sounds, s->time_out);
        set_state(s, SLAC_EVSE_SOUNDING);
        s->t_deadline = now + (uint32_t)s->time_out * 100u;
        break;

    case CM_MNBC_SOUND | MMTYPE_IND:
        if (s->state != SLAC_EVSE_SOUNDING || len < MS_IND_LEN)
            break;
        if (memcmp(&f[MS_IND_RUNID], s->run_id, SLAC_RUNID_LEN) ||
            memcmp(&f[HPGP_OFS_OSA], s->pev_mac, PLC_MAC_LEN))
            break;
        s->sounds++;
        break;

    case CM_ATTEN_PROFILE | MMTYPE_IND: {
        uint8_t ng;
        if (s->state != SLAC_EVSE_SOUNDING || len < AP_IND_AAG)
            break;
        if (memcmp(&f[AP_IND_PEV_MAC], s->pev_mac, PLC_MAC_LEN))
            break;
        ng = f[AP_IND_NUM_GROUPS];
        if (ng > SLAC_GROUPS)
            ng = SLAC_GROUPS;
        if (len < AP_IND_AAG + ng)
            ng = (uint8_t)(len - AP_IND_AAG);
        for (i = 0; i < ng; i++)
            s->aag_sum[i] = (uint16_t)(s->aag_sum[i] + f[AP_IND_AAG + i]);
        s->num_groups = ng;
        s->profiles++;
        if (s->profiles >= s->num_sounds)
            finish_sounding(s, now);
        break;
    }

    case CM_ATTEN_CHAR | MMTYPE_RSP:
        if (s->state != SLAC_EVSE_WAIT_ATTEN_RSP || len < AC_RSP_LEN)
            break;
        if (memcmp(&f[AC_RSP_RUNID], s->run_id, SLAC_RUNID_LEN))
            break;
        LOG("<- CM_ATTEN_CHAR.RSP result %u\r\n", f[AC_RSP_RESULT]);
        set_state(s, SLAC_EVSE_WAIT_MATCH);
        s->t_deadline = now + TT_EVSE_MATCH_SESSION_MS;
        break;

    case CM_SLAC_MATCH | MMTYPE_REQ:
        if (len < SM_REQ_LEN)
            break;
        if (s->state != SLAC_EVSE_WAIT_MATCH && s->state != SLAC_EVSE_WAIT_ATTEN_RSP &&
            s->state != SLAC_EVSE_MATCHED)
            break;
        if (memcmp(&f[SM_RUNID], s->run_id, SLAC_RUNID_LEN) ||
            memcmp(&f[SM_PEV_MAC], s->pev_mac, PLC_MAC_LEN))
            break;
        if (memcmp(&f[SM_EVSE_MAC], s->io.host_mac, PLC_MAC_LEN))
            break;                  /* the EV chose another EVSE */
        memcpy(s->pev_id, &f[SM_PEV_ID], SLAC_UNIQUE_ID_LEN);
        LOG("<- CM_SLAC_MATCH.REQ, -> CNF (NMK handed over)\r\n");
        send_match_cnf(s);
        if (s->state != SLAC_EVSE_MATCHED) {
            set_state(s, SLAC_EVSE_MATCHED);
            s->t_deadline = now + 0x40000000u;
            emit(s, SLAC_EVT_MATCHED, 0);
        }
        break;

    default:
        break;
    }
}

const char *slac_evse_state_name(slac_evse_state_t st)
{
    static const char *const n[] = {
        "INIT_KEY", "IDLE", "WAIT_PARAM", "WAIT_START_ATTEN", "SOUNDING",
        "WAIT_ATTEN_RSP", "WAIT_MATCH", "MATCHED", "FAILED"
    };
    return (unsigned)st < sizeof(n) / sizeof(n[0]) ? n[st] : "?";
}
