/*
 * tests/test_slac_sim.c - EVSE and EV SLAC state machines on a simulated powerline
 *
 * Each node = host (state machine under test) + simulated QCA7005 modem.
 *  - frames to 00:B0:52:00:00:01 are handled by the local modem
 *    (CM_SET_KEY, VS_SW_VER, VS_NW_INFO)
 *  - other frames go over the "line" to every coupled node, 1 ms latency
 *  - an EVSE modem that receives CM_MNBC_SOUND.IND produces
 *    CM_ATTEN_PROFILE.IND for its host with the configured attenuation
 */
#include <string.h>
#include "test_common.h"
#include "slac_evse.h"
#include "slac_ev.h"
#include "plc_modem.h"

#define MAX_NODES 4
#define QLEN      512

enum { ROLE_EVSE, ROLE_EV };

typedef struct {
    int role;
    slac_io_t io;
    slac_evse_t evse;
    slac_ev_t ev;
    plc_modem_t modem_q;
    /* simulated modem */
    uint8_t modem_mac[6];
    uint8_t nmk[16], nid[7];
    int key_set;
    /* stats */
    int matched_events, fail_events, last_fail, link_up;
} node_t;

typedef struct { uint32_t t; int node; uint16_t len; uint8_t d[256]; } msg_t;

static node_t N[MAX_NODES];
static int n_nodes;
static uint8_t atten[MAX_NODES][MAX_NODES];   /* dB, 255 = not coupled */
static msg_t Qm[QLEN];
static int q_count;
static uint32_t now;
static int drop_atten_rsp;                     /* drop this many ATTEN_CHAR.RSP */
static uint32_t rnd_state = 12345;

static void to_host(int node, const uint8_t *f, uint16_t len, uint32_t delay)
{
    if (q_count >= QLEN) { printf("queue full\n"); return; }
    Qm[q_count].t = now + delay;
    Qm[q_count].node = node;
    Qm[q_count].len = len;
    memcpy(Qm[q_count].d, f, len);
    q_count++;
}

static void sim_random(void *ctx, uint8_t *b, uint16_t n)
{
    (void)ctx;
    while (n--) {
        rnd_state = rnd_state * 1103515245u + 12345u;
        *b++ = (uint8_t)(rnd_state >> 16);
    }
}

/* local management frames */
static void modem_local(int i, const uint8_t *f, uint16_t len)
{
    node_t *n = &N[i];
    uint8_t r[128];
    int32_t mmtype = hpgp_mmtype(f, len, NULL);

    memset(r, 0, sizeof(r));
    if (mmtype == (CM_SET_KEY | MMTYPE_REQ)) {
        memcpy(n->nid, &f[SK_REQ_NID], 7);
        memcpy(n->nmk, &f[SK_REQ_NEWKEY], 16);
        n->key_set = 1;
        hpgp_hdr(r, n->io.host_mac, n->modem_mac, CM_SET_KEY | MMTYPE_CNF);
        r[SK_CNF_RESULT] = 1;                   /* like QCA firmware */
        to_host(i, r, 60, 5);
    } else if (mmtype == (VS_SW_VER | MMTYPE_REQ)) {
        static const char ver[] = "MAC-QCA7005-1.1.0.730-04-20140815-CS";
        hpgp_vs_hdr(r, n->io.host_mac, n->modem_mac, VS_SW_VER | MMTYPE_CNF);
        r[SW_CNF_DEVICEID] = 0x20;
        r[SW_CNF_VERLEN] = sizeof(ver);
        memcpy(&r[SW_CNF_VERSION], ver, sizeof(ver));
        to_host(i, r, 100, 2);
    } else if (mmtype == (VS_NW_INFO | MMTYPE_REQ)) {
        int j, sta = 0;
        for (j = 0; j < n_nodes; j++)
            if (j != i && N[j].key_set && n->key_set && !memcmp(N[j].nid, n->nid, 7) && atten[i][j] < 255)
                sta++;
        hpgp_vs_hdr(r, n->io.host_mac, n->modem_mac, VS_NW_INFO | MMTYPE_CNF);
        r[NW_CNF_NUMAVLNS] = 1;
        memcpy(&r[NW_CNF_NUMAVLNS + 1], n->nid, 7);
        r[NW_CNF_NUMAVLNS + 1 + NW_NET_NUMSTAS] = (uint8_t)sta;
        to_host(i, r, (uint16_t)(NW_CNF_NUMAVLNS + 1 + NW_NET_LEN + sta * NW_STA_LEN), 2);
    }
}

static int sim_send(void *ctx, const uint8_t *f, uint16_t len)
{
    int i = (int)(long)ctx, j;
    int32_t mmtype = hpgp_mmtype(f, len, NULL);

    if (!memcmp(f, HPGP_MAC_LOCAL, 6)) {
        modem_local(i, f, len);
        return 0;
    }
    if (mmtype == (CM_ATTEN_CHAR | MMTYPE_RSP) && drop_atten_rsp > 0) {
        drop_atten_rsp--;
        return 0;                               /* lost on the line */
    }
    for (j = 0; j < n_nodes; j++) {
        int bcast = !memcmp(f, HPGP_MAC_BROADCAST, 6);
        if (j == i || atten[i][j] == 255)
            continue;
        if (bcast || !memcmp(f, N[j].io.host_mac, 6))
            to_host(j, f, len, 1);
        if (N[j].role == ROLE_EVSE && mmtype == (CM_MNBC_SOUND | MMTYPE_IND)) {
            uint8_t p[128];
            unsigned g;
            memset(p, 0, sizeof(p));
            hpgp_hdr(p, N[j].io.host_mac, N[j].modem_mac, CM_ATTEN_PROFILE | MMTYPE_IND);
            memcpy(&p[AP_IND_PEV_MAC], &f[HPGP_OFS_OSA], 6);
            p[AP_IND_NUM_GROUPS] = SLAC_GROUPS;
            for (g = 0; g < SLAC_GROUPS; g++)
                p[AP_IND_AAG + g] = (uint8_t)(atten[i][j] + (g % 5));   /* +0..4 dB ripple */
            to_host(j, p, AP_IND_AAG + SLAC_GROUPS, 2);
        }
    }
    return 0;
}

static void on_event(void *user, slac_event_t e, int arg)
{
    node_t *n = (node_t *)user;
    if (e == SLAC_EVT_MATCHED) n->matched_events++;
    if (e == SLAC_EVT_FAILED) { n->fail_events++; n->last_fail = arg; }
}

static void on_link(void *user, int up, int st)
{
    (void)st;
    ((node_t *)user)->link_up = up;
}

static void add_node(int role)
{
    int i = n_nodes++;
    node_t *n = &N[i];
    memset(n, 0, sizeof(*n));
    n->role = role;
    n->io.ctx = (void *)(long)i;
    n->io.send = sim_send;
    n->io.random = sim_random;
    {
        uint8_t mac[6] = {0x02, 0x00, 0x00, 0x00, (uint8_t)role, (uint8_t)(0x10 + i)};
        uint8_t mm[6] = {0x00, 0xB0, 0x52, 0x00, 0x00, (uint8_t)(0x20 + i)};
        memcpy(n->io.host_mac, mac, 6);
        memcpy(n->modem_mac, mm, 6);
    }
    if (role == ROLE_EVSE)
        slac_evse_init(&n->evse, &n->io, on_event, n, now);
    else
        slac_ev_init(&n->ev, &n->io, on_event, n, now);
    plc_modem_init(&n->modem_q, &n->io, on_link, n, now);
}

static void reset_sim(void)
{
    memset(N, 0, sizeof(N));
    memset(atten, 255, sizeof(atten));
    n_nodes = 0;
    q_count = 0;
    now = 1000;
    drop_atten_rsp = 0;
}

static void couple(int a, int b, uint8_t db) { atten[a][b] = atten[b][a] = db; }

static void step(unsigned ms)
{
    while (ms--) {
        int k, i;
        /* deliver due messages in order */
        for (k = 0; k < q_count;) {
            if (plc_time_after_eq(now, Qm[k].t)) {
                msg_t m = Qm[k];
                memmove(&Qm[k], &Qm[k + 1], (size_t)(q_count - k - 1) * sizeof(msg_t));
                q_count--;
                node_t *n = &N[m.node];
                plc_modem_on_frame(&n->modem_q, m.d, m.len, now);
                if (n->role == ROLE_EVSE) slac_evse_on_frame(&n->evse, m.d, m.len, now);
                else                      slac_ev_on_frame(&n->ev, m.d, m.len, now);
            } else {
                k++;
            }
        }
        for (i = 0; i < n_nodes; i++) {
            if (N[i].role == ROLE_EVSE) slac_evse_process(&N[i].evse, now);
            else                        slac_ev_process(&N[i].ev, now);
            plc_modem_process(&N[i].modem_q, now);
        }
        now++;
    }
}

/* run until the EV is matched or failed; returns elapsed ms */
static uint32_t run_until_ev_done(int ev, uint32_t limit)
{
    uint32_t t0 = now;
    while (now - t0 < limit) {
        slac_ev_state_t st = slac_ev_state(&N[ev].ev);
        if (st == SLAC_EV_MATCHED || st == SLAC_EV_FAILED)
            break;
        step(1);
    }
    return now - t0;
}

static void scenario_single(void)
{
    uint32_t t;
    printf("slac: 1 EVSE + 1 EV\n");
    reset_sim();
    add_node(ROLE_EVSE);                        /* 0 */
    add_node(ROLE_EV);                          /* 1 */
    couple(0, 1, 18);
    step(100);                                  /* keys programmed */
    CHECK_EQ(slac_evse_state(&N[0].evse), SLAC_EVSE_IDLE);
    CHECK_EQ(slac_ev_state(&N[1].ev), SLAC_EV_IDLE);
    CHECK(N[0].modem_q.have_version);
    CHECK(memcmp(N[0].nid, N[1].nid, 7) != 0);  /* different networks before SLAC */

    slac_evse_start(&N[0].evse, now);           /* CP B, 5 % PWM on */
    step(50);
    slac_ev_start(&N[1].ev, now);               /* EV sees 5 % */
    t = run_until_ev_done(1, 5000);
    printf("  matched after %u ms\n", (unsigned)t);
    CHECK_EQ(slac_ev_state(&N[1].ev), SLAC_EV_MATCHED);
    CHECK_EQ(slac_evse_state(&N[0].evse), SLAC_EVSE_MATCHED);
    CHECK(t < 2500);
    CHECK_EQ(N[1].ev.evse_atten, 19);           /* 18 + avg of the 0..4 dB ripple (1.95) */
    CHECK(memcmp(N[0].nmk, N[1].nmk, 16) == 0); /* both modems got the EVSE NMK */
    CHECK(memcmp(N[0].nid, N[1].nid, 7) == 0);
    CHECK_EQ(N[0].matched_events, 1);
    CHECK_EQ(N[1].matched_events, 1);
    step(2100);                                 /* NW_INFO polls */
    CHECK(N[0].link_up);
    CHECK(N[1].link_up);

    /* unplug: EV leaves with a private key, EVSE makes a fresh NMK */
    {
        uint8_t old[16];
        memcpy(old, N[0].nmk, 16);
        slac_ev_leave(&N[1].ev, now);
        slac_evse_stop(&N[0].evse, now);
        step(2100);
        CHECK_EQ(slac_ev_state(&N[1].ev), SLAC_EV_IDLE);
        CHECK_EQ(slac_evse_state(&N[0].evse), SLAC_EVSE_IDLE);
        CHECK(memcmp(N[0].nmk, old, 16) != 0);
        CHECK(memcmp(N[0].nid, N[1].nid, 7) != 0);
        CHECK(!N[0].link_up);
        CHECK(!N[1].link_up);
    }

    /* plug in again: works a second time */
    slac_evse_start(&N[0].evse, now);
    slac_ev_start(&N[1].ev, now);
    run_until_ev_done(1, 5000);
    CHECK_EQ(slac_ev_state(&N[1].ev), SLAC_EV_MATCHED);
    CHECK(memcmp(N[0].nmk, N[1].nmk, 16) == 0);
}

static void scenario_crosstalk(void)
{
    printf("slac: 2 EVSEs (crosstalk) + 1 EV -> lowest attenuation wins\n");
    reset_sim();
    add_node(ROLE_EVSE);                        /* 0: neighbour, crosstalk */
    add_node(ROLE_EVSE);                        /* 1: the one we are plugged into */
    add_node(ROLE_EV);                          /* 2 */
    couple(0, 2, 47);
    couple(1, 2, 15);
    couple(0, 1, 50);
    step(100);
    slac_evse_start(&N[0].evse, now);
    slac_evse_start(&N[1].evse, now);
    slac_ev_start(&N[2].ev, now);
    run_until_ev_done(2, 5000);
    CHECK_EQ(slac_ev_state(&N[2].ev), SLAC_EV_MATCHED);
    CHECK_EQ(N[2].ev.candidates, 2);
    CHECK(memcmp(N[2].ev.evse_mac, N[1].io.host_mac, 6) == 0);
    CHECK_EQ(slac_evse_state(&N[1].evse), SLAC_EVSE_MATCHED);
    CHECK(slac_evse_state(&N[0].evse) != SLAC_EVSE_MATCHED);
    CHECK(memcmp(N[1].nid, N[2].nid, 7) == 0);
    CHECK(memcmp(N[0].nid, N[2].nid, 7) != 0);
    step(TT_EVSE_MATCH_SESSION_MS + 100);       /* the other EVSE gives up waiting */
    CHECK_EQ(slac_evse_state(&N[0].evse), SLAC_EVSE_WAIT_PARAM);
    CHECK_EQ(N[0].last_fail, SLAC_FAIL_NO_MATCH_REQ);
}

static void scenario_lost_rsp(void)
{
    printf("slac: lost CM_ATTEN_CHAR.RSP is repeated\n");
    reset_sim();
    add_node(ROLE_EVSE);
    add_node(ROLE_EV);
    couple(0, 1, 30);
    step(100);
    drop_atten_rsp = 1;
    slac_evse_start(&N[0].evse, now);
    slac_ev_start(&N[1].ev, now);
    run_until_ev_done(1, 5000);
    CHECK_EQ(slac_ev_state(&N[1].ev), SLAC_EV_MATCHED);
    CHECK_EQ(slac_evse_state(&N[0].evse), SLAC_EVSE_MATCHED);
    CHECK_EQ(drop_atten_rsp, 0);
}

static void scenario_no_evse(void)
{
    uint32_t t;
    printf("slac: EV without EVSE fails with no CM_SLAC_PARAM.CNF\n");
    reset_sim();
    add_node(ROLE_EV);
    step(100);
    slac_ev_start(&N[0].ev, now);
    t = run_until_ev_done(0, 5000);
    CHECK_EQ(slac_ev_state(&N[0].ev), SLAC_EV_FAILED);
    CHECK_EQ(N[0].last_fail, SLAC_FAIL_NO_PARAM_CNF);
    CHECK(t >= 3 * TT_MATCH_RESPONSE_MS && t < 3 * TT_MATCH_RESPONSE_MS + 20);
}

static void scenario_evse_timeout(void)
{
    printf("slac: EVSE TT_EVSE_SLAC_init timeout\n");
    reset_sim();
    add_node(ROLE_EVSE);
    step(100);
    slac_evse_start(&N[0].evse, now);
    step(TT_EVSE_SLAC_INIT_MS + 10);
    CHECK_EQ(slac_evse_state(&N[0].evse), SLAC_EVSE_FAILED);
    CHECK_EQ(N[0].last_fail, SLAC_FAIL_INIT_TIMEOUT);
}

void test_slac_sim(void)
{
    scenario_single();
    scenario_crosstalk();
    scenario_lost_rsp();
    scenario_no_evse();
    scenario_evse_timeout();
}
