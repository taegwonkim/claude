/*
 * tests/test_v2g.c - V2G stack on the host: codec unit tests and a complete
 *                    DIN 70121 DC session between two processes
 *
 * The charger (SECC) and the vehicle (EVCC) each run in their own process with
 * their own lwIP instance. The "powerline" is a socketpair that carries whole
 * ethernet frames, i.e. what the two QCA7005 modems do after SLAC.
 *
 *   ./test_v2g          run everything
 *   ./test_v2g -v       with the protocol log
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <math.h>

#include "v2g_codec.h"
#include "v2gtp.h"
#include "v2g_sdp.h"
#include "v2g_netif.h"
#include "v2g_sim.h"
#include "din_secc.h"
#include "din_evcc.h"
#include "lwip/sys.h"

static int g_verbose;
static int g_failures;
static const char *g_role = "";

#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_failures++; } } while (0)

int test_log(const char *fmt, ...)
{
    va_list ap;
    int r;
    if (!g_verbose)
        return 0;
    va_start(ap, fmt);
    printf("    %s", g_role);
    r = vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    return r;
}

/* ---- platform functions lwIP / V2G need ---------------------------------------- */
u32_t sys_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

uint32_t v2g_rand32(void)
{
    return (uint32_t)random();
}

/* ============================================================================== */
/* part 1: codec unit tests                                                       */
/* ============================================================================== */

static void test_pv(void)
{
    struct din_PhysicalValueType pv;
    printf("physical values\n");
    v2g_pv_set(&pv, 400.0f, din_unitSymbolType_V);
    CHECK(pv.Value == 400 && pv.Multiplier == 0);
    v2g_pv_set(&pv, 12.5f, din_unitSymbolType_A);
    CHECK(pv.Value == 125 && pv.Multiplier == -1);
    v2g_pv_set(&pv, 50000.0f, din_unitSymbolType_W);
    CHECK(pv.Multiplier == 1 && pv.Value == 5000);
    CHECK(fabsf(v2g_pv_get(&pv) - 50000.0f) < 1.0f);
}

static void test_v2gtp(void)
{
    v2gtp_rx_t rx;
    uint8_t msg[20];
    uint16_t type;
    uint32_t plen;

    printf("V2GTP framing\n");
    v2gtp_write_header(msg, V2GTP_TYPE_EXI, 4);
    memset(&msg[8], 0xAB, 4);
    v2gtp_rx_reset(&rx);
    v2gtp_rx_push(&rx, msg, 5);                     /* split across TCP segments */
    CHECK(v2gtp_rx_peek(&rx, &type, &plen) == 0);
    v2gtp_rx_push(&rx, &msg[5], 7);
    v2gtp_rx_push(&rx, msg, 12);                    /* second message right behind */
    CHECK(v2gtp_rx_peek(&rx, &type, &plen) == 1 && type == V2GTP_TYPE_EXI && plen == 4);
    v2gtp_rx_pop(&rx);
    CHECK(rx.len == 12);
    CHECK(v2gtp_rx_peek(&rx, &type, &plen) == 1);
    msg[1] = 0x00;                                   /* broken inverse version */
    v2gtp_rx_reset(&rx);
    v2gtp_rx_push(&rx, msg, 12);
    CHECK(v2gtp_rx_peek(&rx, &type, &plen) == -1);
}

static void test_sdp_codec(void)
{
    uint8_t b[32];
    ip6_addr_t a, a2;
    uint16_t port, n;
    uint8_t sec, tr;

    printf("SDP codec\n");
    n = v2g_sdp_build_req(b, V2G_SDP_SECURITY_NONE, V2G_SDP_TRANSPORT_TCP);
    CHECK(n == 10 && b[2] == 0x90 && b[3] == 0x00 && b[7] == 2);
    CHECK(v2g_sdp_parse_req(b, n, &sec, &tr) == 0 && sec == 0x10 && tr == 0x00);

    IP6_ADDR(&a, PP_HTONL(0xfe800000UL), 0, PP_HTONL(0x0200e5ffUL), PP_HTONL(0xfe123456UL));
    n = v2g_sdp_build_res(b, &a, 61851, V2G_SDP_SECURITY_NONE, V2G_SDP_TRANSPORT_TCP);
    CHECK(n == 28 && b[8] == 0xfe && b[9] == 0x80 && b[23] == 0x56 && b[24] == (61851 >> 8));
    CHECK(v2g_sdp_parse_res(b, n, &a2, &port, &sec, &tr) == 0);
    CHECK(ip6_addr_cmp(&a, &a2) && port == 61851);
}

static void test_exi_roundtrip(void)
{
    uint8_t buf[1024];
    uint32_t len, plen;
    uint16_t type;
    struct din_BodyType *b;

    printf("EXI encode/decode (supportedAppProtocol, DIN messages)\n");

    v2g_app_clear();
    g_app_doc.supportedAppProtocolReq_isUsed = 1;
    g_app_doc.supportedAppProtocolReq.AppProtocol.arrayLen = 1;
    strcpy(g_app_doc.supportedAppProtocolReq.AppProtocol.array[0].ProtocolNamespace.characters, V2G_DIN_NAMESPACE);
    g_app_doc.supportedAppProtocolReq.AppProtocol.array[0].ProtocolNamespace.charactersLen = sizeof(V2G_DIN_NAMESPACE) - 1;
    g_app_doc.supportedAppProtocolReq.AppProtocol.array[0].VersionNumberMajor = 2;
    g_app_doc.supportedAppProtocolReq.AppProtocol.array[0].SchemaID = 1;
    g_app_doc.supportedAppProtocolReq.AppProtocol.array[0].Priority = 1;
    CHECK(v2g_encode_app(buf, sizeof(buf), &len) == 0);
    CHECK(v2gtp_parse_header(buf, len, &type, &plen) == 1 && type == V2GTP_TYPE_EXI && plen + 8 == len);
    CHECK(v2g_decode_app(&buf[8], plen) == 0);
    CHECK(g_app_doc.supportedAppProtocolReq_isUsed &&
          !memcmp(g_app_doc.supportedAppProtocolReq.AppProtocol.array[0].ProtocolNamespace.characters,
                  V2G_DIN_NAMESPACE, sizeof(V2G_DIN_NAMESPACE) - 1));

    /* CurrentDemandReq with every value set */
    v2g_din_clear();
    g_din_doc.V2G_Message.Header.SessionID.bytesLen = 8;
    memset(g_din_doc.V2G_Message.Header.SessionID.bytes, 0x5A, 8);
    b = &g_din_doc.V2G_Message.Body;
    b->CurrentDemandReq_isUsed = 1;
    b->CurrentDemandReq.DC_EVStatus.EVReady = 1;
    b->CurrentDemandReq.DC_EVStatus.EVRESSSOC = 55;
    v2g_pv_set(&b->CurrentDemandReq.EVTargetCurrent, 80.0f, din_unitSymbolType_A);
    v2g_pv_set(&b->CurrentDemandReq.EVTargetVoltage, 395.5f, din_unitSymbolType_V);
    v2g_pv_set(&b->CurrentDemandReq.EVMaximumVoltageLimit, 420.0f, din_unitSymbolType_V);
    b->CurrentDemandReq.EVMaximumVoltageLimit_isUsed = 1;
    b->CurrentDemandReq.ChargingComplete = 0;
    CHECK(v2g_encode_din(buf, sizeof(buf), &len) == 0);
    if (g_verbose)
        printf("    CurrentDemandReq = %u bytes EXI\n", (unsigned)(len - 8));
    CHECK(v2g_decode_din(&buf[8], len - 8) == 0);
    b = &g_din_doc.V2G_Message.Body;
    CHECK(b->CurrentDemandReq_isUsed);
    CHECK(b->CurrentDemandReq.DC_EVStatus.EVRESSSOC == 55);
    CHECK(fabsf(v2g_pv_get(&b->CurrentDemandReq.EVTargetVoltage) - 395.5f) < 0.01f);
    CHECK(g_din_doc.V2G_Message.Header.SessionID.bytes[7] == 0x5A);

    /* garbage must not decode */
    memset(buf, 0xFF, 32);
    CHECK(v2g_decode_din(buf, 32) != 0);
}

/* ============================================================================== */
/* part 2: full session, two processes                                            */
/* ============================================================================== */

static int s_fd;

static int link_send(void *ctx, const uint8_t *frame, uint16_t len)
{
    (void)ctx;
    return send(s_fd, frame, len, MSG_DONTWAIT) == (ssize_t)len ? 0 : -1;
}

static void pump(void)
{
    uint8_t f[1600];
    ssize_t n;
    while ((n = recv(s_fd, f, sizeof(f), MSG_DONTWAIT)) > 0)
        v2g_netif_input(f, (uint16_t)n);
    v2g_netif_poll();
}

typedef struct {
    int events[16];
    int fail_arg;
} evlog_t;

static void on_event(void *user, v2g_event_t e, int arg)
{
    evlog_t *l = (evlog_t *)user;
    if ((unsigned)e < 16)
        l->events[e]++;
    if (e == V2G_EVT_FAILED)
        l->fail_arg = arg;
}

/* scenario: 0 = charge until the battery target, 1 = charger stops early */
static int run_evse(int scenario)
{
    static const uint8_t mac[6] = {0x02, 0xE5, 0x00, 0x00, 0x00, 0x01};
    v2g_sim_secc_t sim;
    secc_hw_t hw;
    din_secc_t s;
    evlog_t ev;
    uint32_t t0 = sys_now(), t_charge = 0;

    g_role = "EVSE ";
    memset(&ev, 0, sizeof(ev));
    memset(&s, 0, sizeof(s));
    v2g_sim_secc_init(&sim, &hw);
    sim.force_cp_c = 1;                          /* no CP circuit in the test */
    v2g_netif_init(mac, link_send, NULL);
    if (din_secc_start(&s, &hw, on_event, &ev) != 0)
        return 10;

    while (sys_now() - t0 < 30000u) {
        pump();
        din_secc_process(&s, sys_now());
        if (scenario == 1 && ev.events[V2G_EVT_CHARGING] && !t_charge)
            t_charge = sys_now();
        if (scenario == 1 && t_charge && sys_now() - t_charge > 1000u)
            din_secc_request_stop(&s);
        if (s.phase == SECC_DONE || s.phase == SECC_FAILED)
            break;
        usleep(500);
    }
    /* let the FIN go out */
    t0 = sys_now();
    while (sys_now() - t0 < 300u) { pump(); usleep(500); }

    din_secc_stop(&s);                           /* free the listen pcb for the next run */
    if (s.phase != SECC_DONE && s.phase != SECC_IDLE) {
        printf("  EVSE ended in %s (%s)\n", din_secc_phase_name(s.phase), v2g_fail_name(ev.fail_arg));
        return 1;
    }
    if (!ev.events[V2G_EVT_CABLE_CHECK] || !ev.events[V2G_EVT_CHARGING] || !ev.events[V2G_EVT_CHARGING_STOPPED])
        return 2;
    if (sim.on)
        return 3;                                 /* output must be off at the end */
    return 0;
}

static int run_ev(int scenario)
{
    static const uint8_t mac[6] = {0x02, 0xEF, 0x00, 0x00, 0x00, 0x02};
    v2g_sim_evcc_t sim;
    evcc_hw_t hw;
    din_evcc_t e;
    evlog_t ev;
    uint32_t t0 = sys_now();

    g_role = "EV   ";
    memset(&ev, 0, sizeof(ev));
    memset(&e, 0, sizeof(e));
    v2g_sim_evcc_init(&sim, &hw);
    if (scenario == 1)
        sim.target_soc = 90.0f;                   /* the charger stops first */
    v2g_netif_init(mac, link_send, NULL);
    din_evcc_start(&e, &hw, mac, on_event, &ev, sys_now());

    while (sys_now() - t0 < 30000u) {
        pump();
        din_evcc_process(&e, sys_now());
        if (e.phase == EVCC_DONE || e.phase == EVCC_FAILED)
            break;
        usleep(500);
    }
    t0 = sys_now();
    while (sys_now() - t0 < 300u) { pump(); usleep(500); }

    if (g_verbose)
        printf("    EV   end: %s, SOC %.1f %%, %lu CurrentDemand cycles\n",
               din_evcc_phase_name(e.phase), (double)sim.soc, (unsigned long)e.cd_count);
    if (e.phase != EVCC_DONE) {
        printf("  EV ended in %s at %s (%s)\n", din_evcc_phase_name(e.phase),
               din_evcc_req_name(e.req), v2g_fail_name(ev.fail_arg));
        return 1;
    }
    if (scenario == 0 && sim.soc < sim.target_soc)
        return 2;
    if (scenario == 1 && sim.soc >= sim.target_soc)
        return 3;                                 /* should have been stopped early */
    if (sim.s2 || sim.contactor)
        return 4;                                 /* safe state at the end */
    if (e.cd_count < 5)
        return 5;
    return 0;
}

static void test_session(int scenario)
{
    int sv[2], status = 0, r_evse;
    pid_t pid;
    uint32_t t0 = sys_now();

    printf("DIN 70121 session over lwIP/IPv6/TCP (%s)\n",
           scenario == 0 ? "EV charges to its target SOC" : "EVSE stops the session");
    fflush(stdout);
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) != 0) {
        CHECK(!"socketpair");
        return;
    }
    pid = fork();
    if (pid == 0) {
        close(sv[0]);
        s_fd = sv[1];
        srandom(2);
        _exit(run_ev(scenario));
    }
    close(sv[1]);
    s_fd = sv[0];
    srandom(1);
    r_evse = run_evse(scenario);
    waitpid(pid, &status, 0);
    close(sv[0]);
    printf("  EVSE result %d, EV result %d, %lu ms\n", r_evse,
           WIFEXITED(status) ? WEXITSTATUS(status) : -1, (unsigned long)(sys_now() - t0));
    CHECK(r_evse == 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "-v"))
        g_verbose = 1;
    setvbuf(stdout, NULL, _IOLBF, 0);

    test_pv();
    test_v2gtp();
    test_sdp_codec();
    test_exi_roundtrip();
    test_session(0);
    test_session(1);

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nall V2G tests passed\n");
    return 0;
}
