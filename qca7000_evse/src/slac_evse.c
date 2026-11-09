/*
 * slac_evse.c - ISO 15118-3 SLAC state machine, EVSE side
 *
 * Message content follows open-plc-utils slac/evse_*.c.
 */
#include "slac_evse.h"
#include "hpgp_mme.h"
#include "qca7000.h"

#include <string.h>
#include "task.h"
#include "queue.h"

#ifndef SLAC_LOG
#define SLAC_LOG(...) do { } while (0)
#endif

#define SLAC_TASK_STACK   768    /* words */
#define SLAC_RXQ_DEPTH    16
#define SLAC_MSG_MAX      160    /* largest SLAC MME handled is 129 bytes */

enum { ITEM_FRAME = 0, ITEM_CMD };
enum { CMD_START = 1, CMD_STOP };

typedef struct {
    uint8_t  kind;
    uint8_t  cmd;
    uint16_t len;
    uint8_t  data[SLAC_MSG_MAX];
} slac_item_t;

typedef enum {
    ST_IDLE = 0,
    ST_WAIT_SETKEY_CNF,
    ST_WAIT_PARM_REQ,
    ST_WAIT_START_ATTEN,
    ST_SOUNDING,
    ST_WAIT_ATTEN_RSP,
    ST_WAIT_MATCH_REQ,
    ST_MATCHED
} slac_state_t;

static const uint8_t k_bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static struct {
    slac_evse_cfg_t cfg;
    QueueHandle_t   q;
    TaskHandle_t    task;
    slac_state_t    st;

    bool            tmo_on;
    TickType_t      deadline;
    bool            session_on;
    TickType_t      session_deadline;
    uint8_t         retries;

    uint8_t         nmk[SLAC_NMK_LEN];
    uint8_t         nid[SLAC_NID_LEN];

    uint8_t         ev_mac[6];
    uint8_t         run_id[SLAC_RUNID_LEN];
    uint8_t         expected_sounds;
    uint8_t         sounds;
    uint8_t         num_groups;
    uint32_t        aag_sum[SLAC_GROUPS];

    uint8_t         tx[SLAC_MSG_MAX];
} S;

/* ========================================================================== */
/*  Helpers                                                                   */
/* ========================================================================== */

static void set_timeout(uint32_t ms)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t dl  = now + pdMS_TO_TICKS(ms);

    /* bound by match session timer if running */
    if (S.session_on && (int32_t)(S.session_deadline - dl) < 0)
        dl = S.session_deadline;
    S.deadline = dl;
    S.tmo_on   = true;
}

static TickType_t ticks_to_deadline(void)
{
    int32_t d;
    if (!S.tmo_on)
        return portMAX_DELAY;
    d = (int32_t)(S.deadline - xTaskGetTickCount());
    return d > 0 ? (TickType_t)d : 0;
}

static bool session_expired(void)
{
    return S.session_on && (int32_t)(xTaskGetTickCount() - S.session_deadline) >= 0;
}

static void enter(slac_state_t st, uint32_t timeout_ms)
{
    SLAC_LOG("slac: state %d -> %d\n", (int)S.st, (int)st);
    S.st = st;
    if (timeout_ms)
        set_timeout(timeout_ms);
    else
        S.tmo_on = false;
}

static void emit(slac_evse_event_t ev)
{
    if (S.cfg.on_event)
        S.cfg.on_event(ev, S.ev_mac, S.cfg.user);
}

static void hdr_init(hpgp_hdr_t *h, const uint8_t dst[6], uint16_t mmtype)
{
    memcpy(h->eth.oda, dst, 6);
    memcpy(h->eth.osa, S.cfg.host_mac, 6);
    hpgp_wr_be16(h->eth.ethertype, ETHERTYPE_HOMEPLUG);
    h->mmv = HPGP_MMV_AV11;
    hpgp_wr_le16(h->mmtype, mmtype);
    h->fmsn = 0;
    h->fmid = 0;
}

static void *tx_begin(void)
{
    memset(S.tx, 0, sizeof(S.tx));
    return S.tx;
}

static void tx_end(uint16_t len)
{
    if (qca7000_send(S.tx, len, 20) != 0)
        SLAC_LOG("slac: tx failed\n");
}

/* HomePlug AV NID from NMK (open-plc-utils HPAVKeyNID), security level 0 */
static void nid_from_nmk(const uint8_t nmk[SLAC_NMK_LEN], uint8_t nid[SLAC_NID_LEN])
{
    uint8_t d[32], t[32];
    int i;

    slac_port_sha256(nmk, SLAC_NMK_LEN, d);
    for (i = 0; i < 4; i++) {
        slac_port_sha256(d, sizeof(d), t);
        memcpy(d, t, sizeof(d));
    }
    memcpy(nid, d, SLAC_NID_LEN);
    nid[SLAC_NID_LEN - 1] = (uint8_t)(d[SLAC_NID_LEN - 1] >> 4);   /* | (level << 4), level = 0 */
}

static void reset_sounding(void)
{
    S.sounds = 0;
    S.num_groups = 0;
    S.expected_sounds = S.cfg.num_sounds;
    memset(S.aag_sum, 0, sizeof(S.aag_sum));
}

static void back_to_wait_parm(void)
{
    if (session_expired()) {
        SLAC_LOG("slac: match session expired\n");
        S.session_on = false;
        enter(ST_IDLE, 0);
        emit(SLAC_EVSE_EV_FAILED);
        return;
    }
    enter(ST_WAIT_PARM_REQ, S.cfg.match_session_ms);
}

/* ========================================================================== */
/*  Transmit                                                                  */
/* ========================================================================== */

static void send_set_key(void)
{
    cm_set_key_req_t *m = tx_begin();

    hdr_init(&m->h, S.cfg.modem_mac, CM_SET_KEY | MMTYPE_REQ);
    m->key_type = 0x01;                       /* NMK */
    memset(m->my_nonce, 0xAA, sizeof(m->my_nonce));
    memset(m->your_nonce, 0x00, sizeof(m->your_nonce));
    m->pid = 0x04;                            /* HLE protocol */
    hpgp_wr_le16(m->prn, 0x0000);
    m->pmn = 0x00;
    m->cco_capability = 0x00;
    memcpy(m->nid, S.nid, SLAC_NID_LEN);
    m->new_eks = 0x01;
    memcpy(m->new_key, S.nmk, SLAC_NMK_LEN);
    tx_end(sizeof(*m));
}

static void send_parm_cnf(void)
{
    cm_slac_parm_cnf_t *m = tx_begin();

    hdr_init(&m->h, S.ev_mac, CM_SLAC_PARAM | MMTYPE_CNF);
    memcpy(m->msound_target, k_bcast, 6);
    m->num_sounds = S.cfg.num_sounds;
    m->time_out = (uint8_t)(S.cfg.mnbc_ms / 100u);
    m->resp_type = 0x01;
    memcpy(m->forwarding_sta, S.ev_mac, 6);
    m->application_type = 0x00;
    m->security_type = 0x00;
    memcpy(m->run_id, S.run_id, SLAC_RUNID_LEN);
    tx_end(sizeof(*m));
}

static void send_atten_char(void)
{
    cm_atten_char_ind_t *m = tx_begin();
    uint8_t i;

    hdr_init(&m->h, S.ev_mac, CM_ATTEN_CHAR | MMTYPE_IND);
    m->application_type = 0x00;
    m->security_type = 0x00;
    memcpy(m->source_address, S.ev_mac, 6);
    memcpy(m->run_id, S.run_id, SLAC_RUNID_LEN);
    /* source_id / resp_id: all zero */
    m->num_sounds = S.sounds;
    m->num_groups = S.num_groups;
    for (i = 0; i < S.num_groups; i++)
        m->aag[i] = (uint8_t)((S.aag_sum[i] + S.sounds / 2u) / S.sounds);
    tx_end(sizeof(*m));
}

static void send_match_cnf(const cm_slac_match_req_t *req)
{
    cm_slac_match_cnf_t *m = tx_begin();

    hdr_init(&m->h, S.ev_mac, CM_SLAC_MATCH | MMTYPE_CNF);
    m->application_type = 0x00;
    m->security_type = 0x00;
    hpgp_wr_le16(m->mvf_length, SLAC_MVF_CNF_LEN);
    memcpy(m->pev_id, req->pev_id, SLAC_UNIQUE_ID_LEN);
    memcpy(m->pev_mac, req->pev_mac, 6);
    memcpy(m->evse_id, req->evse_id, SLAC_UNIQUE_ID_LEN);
    memcpy(m->evse_mac, S.cfg.host_mac, 6);
    memcpy(m->run_id, S.run_id, SLAC_RUNID_LEN);
    memcpy(m->nid, S.nid, SLAC_NID_LEN);
    memcpy(m->nmk, S.nmk, SLAC_NMK_LEN);
    tx_end(sizeof(*m));
}

/* ========================================================================== */
/*  Receive handlers                                                          */
/* ========================================================================== */

static void finish_sounding(void)
{
    if (S.sounds == 0) {
        SLAC_LOG("slac: no sounds received\n");
        back_to_wait_parm();
        return;
    }
    SLAC_LOG("slac: %u sounds, %u groups\n", S.sounds, S.num_groups);
    S.retries = 0;
    send_atten_char();
    enter(ST_WAIT_ATTEN_RSP, S.cfg.match_response_ms);
}

static void on_parm_req(const uint8_t *f, uint16_t len)
{
    const cm_slac_parm_req_t *r = (const void *)f;

    if (len < sizeof(*r) || S.st < ST_WAIT_PARM_REQ)
        return;
    if (r->application_type != 0x00 || r->security_type != 0x00)
        return;

    memcpy(S.ev_mac, r->h.eth.osa, 6);
    memcpy(S.run_id, r->run_id, SLAC_RUNID_LEN);
    reset_sounding();

    /* (Re)start match session timer */
    S.session_on = false;
    S.session_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(S.cfg.match_session_ms);
    S.session_on = true;

    send_parm_cnf();
    enter(ST_WAIT_START_ATTEN, S.cfg.start_atten_wait_ms);
}

static void on_start_atten(const uint8_t *f, uint16_t len)
{
    const cm_start_atten_char_ind_t *r = (const void *)f;

    if (len < sizeof(*r) || S.st != ST_WAIT_START_ATTEN)
        return;     /* repeats while SOUNDING are ignored */
    if (memcmp(r->h.eth.osa, S.ev_mac, 6) != 0 ||
        memcmp(r->run_id, S.run_id, SLAC_RUNID_LEN) != 0)
        return;

    if (r->num_sounds > 0)
        S.expected_sounds = r->num_sounds;
    enter(ST_SOUNDING, S.cfg.mnbc_ms);
}

static void on_atten_profile(const uint8_t *f, uint16_t len)
{
    const cm_atten_profile_ind_t *r = (const void *)f;
    uint8_t n, i;

    if (len < sizeof(hpgp_hdr_t) + 8u)
        return;
    /* tolerate a lost START_ATTEN_CHAR: first profile also opens the window */
    if (S.st == ST_WAIT_START_ATTEN)
        enter(ST_SOUNDING, S.cfg.mnbc_ms);
    if (S.st != ST_SOUNDING || memcmp(r->pev_mac, S.ev_mac, 6) != 0)
        return;

    n = r->num_groups;
    if (n > SLAC_GROUPS)
        n = SLAC_GROUPS;
    if (len < sizeof(hpgp_hdr_t) + 8u + n)
        return;

    for (i = 0; i < n; i++)
        S.aag_sum[i] += r->aag[i];
    if (n > S.num_groups)
        S.num_groups = n;
    S.sounds++;

    if (S.sounds >= S.expected_sounds)
        finish_sounding();
}

static void on_atten_rsp(const uint8_t *f, uint16_t len)
{
    const cm_atten_char_rsp_t *r = (const void *)f;

    if (len < sizeof(*r) || S.st != ST_WAIT_ATTEN_RSP)
        return;
    if (memcmp(r->h.eth.osa, S.ev_mac, 6) != 0 ||
        memcmp(r->run_id, S.run_id, SLAC_RUNID_LEN) != 0)
        return;

    if (r->result != 0x00) {
        SLAC_LOG("slac: ATTEN_CHAR.RSP result %u\n", r->result);
        back_to_wait_parm();
        return;
    }
    enter(ST_WAIT_MATCH_REQ, S.cfg.match_session_ms);  /* bounded by session timer */
}

static void on_match_req(const uint8_t *f, uint16_t len)
{
    const cm_slac_match_req_t *r = (const void *)f;

    if (len < sizeof(*r))
        return;
    /* WAIT_ATTEN_RSP accepted too: the RSP may have been lost */
    if (S.st != ST_WAIT_ATTEN_RSP && S.st != ST_WAIT_MATCH_REQ && S.st != ST_MATCHED)
        return;
    if (memcmp(r->h.eth.osa, S.ev_mac, 6) != 0 ||
        memcmp(r->run_id, S.run_id, SLAC_RUNID_LEN) != 0)
        return;

    send_match_cnf(r);       /* also re-sent if EV repeats the REQ */
    if (S.st != ST_MATCHED) {
        S.session_on = false;
        enter(ST_MATCHED, 0);
        emit(SLAC_EVSE_EV_MATCHED);
    }
}

static void on_set_key_cnf(const uint8_t *f, uint16_t len)
{
    const cm_set_key_cnf_t *r = (const void *)f;

    if (len < sizeof(*r) || S.st != ST_WAIT_SETKEY_CNF)
        return;
    /* RESULT semantics differ between firmware versions: log, do not reject. */
    SLAC_LOG("slac: SET_KEY.CNF result %u\n", r->result);
    emit(SLAC_EVSE_EV_KEY_SET);
    enter(ST_WAIT_PARM_REQ, S.cfg.slac_init_ms);
}

static void handle_frame(const uint8_t *f, uint16_t len)
{
    const hpgp_hdr_t *h = (const void *)f;

    switch (hpgp_rd_le16(h->mmtype)) {
    case CM_SET_KEY | MMTYPE_CNF:          on_set_key_cnf(f, len);   break;
    case CM_SLAC_PARAM | MMTYPE_REQ:       on_parm_req(f, len);      break;
    case CM_START_ATTEN_CHAR | MMTYPE_IND: on_start_atten(f, len);   break;
    case CM_ATTEN_PROFILE | MMTYPE_IND:    on_atten_profile(f, len); break;
    case CM_ATTEN_CHAR | MMTYPE_RSP:       on_atten_rsp(f, len);     break;
    case CM_SLAC_MATCH | MMTYPE_REQ:       on_match_req(f, len);     break;
    default: break;   /* CM_MNBC_SOUND.IND etc. not needed on EVSE host */
    }
}

static void handle_timeout(void)
{
    S.tmo_on = false;

    switch (S.st) {
    case ST_WAIT_SETKEY_CNF:
        if (S.retries++ < S.cfg.setkey_retries) {
            send_set_key();
            set_timeout(S.cfg.setkey_timeout_ms);
        } else {
            enter(ST_IDLE, 0);
            emit(SLAC_EVSE_EV_FAILED);
        }
        break;

    case ST_WAIT_PARM_REQ:
        if (S.session_on && !session_expired()) {
            set_timeout(S.cfg.match_session_ms);
            break;
        }
        SLAC_LOG("slac: no CM_SLAC_PARM.REQ\n");
        S.session_on = false;
        enter(ST_IDLE, 0);
        emit(SLAC_EVSE_EV_FAILED);
        break;

    case ST_WAIT_START_ATTEN:
        back_to_wait_parm();
        break;

    case ST_SOUNDING:
        finish_sounding();
        break;

    case ST_WAIT_ATTEN_RSP:
        if (S.retries++ < S.cfg.atten_char_retries && !session_expired()) {
            send_atten_char();
            set_timeout(S.cfg.match_response_ms);
        } else {
            back_to_wait_parm();
        }
        break;

    case ST_WAIT_MATCH_REQ:
        SLAC_LOG("slac: no CM_SLAC_MATCH.REQ\n");
        S.session_on = false;
        enter(ST_IDLE, 0);
        emit(SLAC_EVSE_EV_FAILED);
        break;

    default:
        break;
    }
}

static void do_start(void)
{
    S.session_on = false;
    memset(S.ev_mac, 0, sizeof(S.ev_mac));
    reset_sounding();

    /* New random NMK per session (recommended) */
    slac_port_random(S.nmk, SLAC_NMK_LEN);
    nid_from_nmk(S.nmk, S.nid);

    S.retries = 0;
    send_set_key();
    enter(ST_WAIT_SETKEY_CNF, S.cfg.setkey_timeout_ms);
}

/* ========================================================================== */
/*  Task                                                                      */
/* ========================================================================== */

static void slac_task(void *arg)
{
    static slac_item_t it;   /* static: keep it off the task stack */
    (void)arg;

    for (;;) {
        if (xQueueReceive(S.q, &it, ticks_to_deadline()) == pdTRUE) {
            if (it.kind == ITEM_CMD) {
                if (it.cmd == CMD_START) do_start();
                else { S.session_on = false; enter(ST_IDLE, 0); }
            } else if (S.st != ST_IDLE) {
                handle_frame(it.data, it.len);
            }
        }
        if (S.tmo_on && (int32_t)(xTaskGetTickCount() - S.deadline) >= 0)
            handle_timeout();
    }
}

/* ========================================================================== */
/*  Public API                                                                */
/* ========================================================================== */

void slac_evse_default_cfg(slac_evse_cfg_t *c)
{
    static const uint8_t localcast[6] = { 0x00, 0xB0, 0x52, 0x00, 0x00, 0x01 };

    memset(c, 0, sizeof(*c));
    memcpy(c->modem_mac, localcast, 6);
    c->slac_init_ms        = 50000;  /* TT_EVSE_SLAC_init (20..50 s)       */
    c->match_session_ms    = 10000;  /* TT_EVSE_match_session              */
    c->mnbc_ms             = 600;    /* TT_EVSE_match_MNBC                  */
    c->match_response_ms   = 200;    /* TT_match_response                   */
    c->start_atten_wait_ms = 1000;
    c->num_sounds          = 10;     /* C_EV_match_MNBC                     */
    c->atten_char_retries  = 2;
    c->setkey_timeout_ms   = 1000;
    c->setkey_retries      = 3;
}

int slac_evse_init(const slac_evse_cfg_t *cfg, UBaseType_t prio)
{
    memset(&S, 0, sizeof(S));
    S.cfg = *cfg;
    S.st  = ST_IDLE;
    S.q   = xQueueCreate(SLAC_RXQ_DEPTH, sizeof(slac_item_t));
    if (!S.q)
        return -1;
    if (xTaskCreate(slac_task, "slac", SLAC_TASK_STACK, NULL, prio, &S.task) != pdPASS)
        return -1;
    return 0;
}

static void post_cmd(uint8_t cmd)
{
    slac_item_t it;          /* ~164 B on caller stack; only header is used */
    it.kind = ITEM_CMD;
    it.cmd  = cmd;
    it.len  = 0;
    (void)xQueueSend(S.q, &it, pdMS_TO_TICKS(10));
}

void slac_evse_start(void) { post_cmd(CMD_START); }
void slac_evse_stop(void)  { post_cmd(CMD_STOP); }

void slac_evse_set_modem_mac(const uint8_t mac[6])
{
    taskENTER_CRITICAL();
    memcpy(S.cfg.modem_mac, mac, 6);
    taskEXIT_CRITICAL();
}

/* Called from qca7000 driver task (rx callback). Non-blocking. */
void slac_evse_on_frame(const uint8_t *frame, uint16_t len)
{
    static slac_item_t it;   /* only the qca7000 task calls this */
    const hpgp_hdr_t *h = (const void *)frame;
    uint16_t mmtype;

    if (len < sizeof(hpgp_hdr_t) || !S.q)
        return;
    if (hpgp_rd_be16(h->eth.ethertype) != ETHERTYPE_HOMEPLUG || h->mmv != HPGP_MMV_AV11)
        return;
    mmtype = hpgp_rd_le16(h->mmtype);
    if ((mmtype & 0xFF00u) != 0x6000u || (mmtype & ~3u) == CM_MNBC_SOUND)
        return;     /* only CM_* SLAC/SET_KEY messages; MNBC sounds not needed */

    if (len > SLAC_MSG_MAX)
        len = SLAC_MSG_MAX;
    it.kind = ITEM_FRAME;
    it.len  = len;
    memcpy(it.data, frame, len);
    (void)xQueueSend(S.q, &it, 0);   /* drop if full */
}
