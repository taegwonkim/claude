/* PC 단위 시험 : veh_frame.c / someip.c   (make -C tests) */
#include <stdio.h>
#include <string.h>
#include "veh_frame.h"
#include "someip.h"
#include "veh_ecu.h"
#include "vcan_proto.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void test_frame(void)
{
  uint8_t f[64] = {0};
  veh_frame_info_t fi;

  f[12] = 0x08; f[13] = 0x00;
  CHECK(veh_frame_parse(f, 60, &fi) == 0 && !fi.tagged && fi.ethertype == 0x0800 && fi.vid == VEH_VLAN_NONE);

  f[12] = 0x81; f[13] = 0x00; f[14] = 0xA0; f[15] = 0x2A; f[16] = 0x08; f[17] = 0x06; /* PCP5 VID42 ARP */
  CHECK(veh_frame_parse(f, 60, &fi) == 0 && fi.tagged && fi.pcp == 5 && fi.vid == 42 && fi.ethertype == 0x0806 && fi.header_len == 18);
  CHECK(veh_frame_parse(f, 16, &fi) == -1);
  CHECK(veh_vlan_tci(5, 42) == 0xA02A);
}

static void test_someip(void)
{
  uint8_t b[128];
  someip_msg_t m;
  someip_sd_t sd;
  someip_sd_entry_t e;
  someip_sd_option_t o;
  uint32_t n;

  /* 일반 메시지 2 개가 붙은 데이터그램 */
  uint8_t two[] = { 0x12,0x34,0x80,0x01, 0,0,0,10, 0,1,0,2, 1,1,2,0, 0xAA,0xBB,
                    0x12,0x34,0x00,0x05, 0,0,0,8,  0,1,0,3, 1,1,0x80,0 };
  int32_t used = someip_parse(two, sizeof(two), &m);
  CHECK(used == 18 && m.service_id == 0x1234 && someip_is_event(&m) && m.payload_len == 2 && m.payload[1] == 0xBB);
  used = someip_parse(two + 18, sizeof(two) - 18, &m);
  CHECK(used == 16 && m.payload_len == 0 && m.message_type == SOMEIP_MT_RESPONSE);
  CHECK(someip_parse(two, 10, &m) == SOMEIP_ERR_SHORT);
  two[7] = 200; CHECK(someip_parse(two, sizeof(two), &m) == SOMEIP_ERR_LENGTH);

  /* FindService 생성 → 다시 해석 */
  n = someip_sd_build_find(b, sizeof(b), 0, 7, 0xC0, 0xFFFF, 0xFFFF, 0xFF, 0xFFFFFFFF, 3);
  CHECK(n == 44);
  CHECK(someip_parse(b, n, &m) == 44 && someip_is_sd(&m));
  CHECK(someip_sd_parse(m.payload, m.payload_len, &sd) == 0 && sd.flags == 0xC0);
  CHECK(someip_sd_entry_count(&sd) == 1 && someip_sd_get_entry(&sd, 0, &e) == 0);
  CHECK(e.type == SOMEIP_SD_FIND_SERVICE && e.service_id == 0xFFFF && e.ttl == 3 && e.minor_version == 0xFFFFFFFF);
  CHECK(someip_sd_option_count(&sd) == 0);

  /* OfferService + IPv4 endpoint 옵션 */
  {
    uint8_t sdp[] = { 0xC0,0,0,0,  0,0,0,16,
                      0x01, 0,0, 0x10, 0x10,0x01, 0x00,0x02, 0x01, 0x00,0x00,0x05, 0,0,0,3,
                      0,0,0,12,
                      0x00,0x09, 0x04, 0x00, 10,0,0,5, 0x00, 0x11, 0x77,0x25 };
    CHECK(someip_sd_parse(sdp, sizeof(sdp), &sd) == 0);
    CHECK(someip_sd_get_entry(&sd, 0, &e) == 0 && e.type == SOMEIP_SD_OFFER_SERVICE && e.num1 == 1);
    CHECK(e.service_id == 0x1001 && e.instance_id == 2 && e.major_version == 1 && e.ttl == 5 && e.minor_version == 3);
    CHECK(strcmp(someip_sd_entry_name(&e), "OfferService") == 0);
    CHECK(someip_sd_option_count(&sd) == 1);
    CHECK(someip_sd_entry_endpoint(&sd, &e, &o) == 0 && o.ipv4[0] == 10 && o.ipv4[3] == 5 && o.l4_proto == 0x11 && o.port == 30501);
    sdp[7] = 15; CHECK(someip_sd_parse(sdp, sizeof(sdp), &sd) == -1);   /* entries 길이가 16 의 배수 아님 */
  }
}

static void test_ipv4(void)
{
  /* IPv4 + UDP 10.0.0.5:30501 -> 10.0.0.9:30502 */
  uint8_t ip[28] = { 0x45,0,0,28, 0,0,0x40,0, 64,17,0,0, 10,0,0,5, 10,0,0,9,
                     0x77,0x25, 0x77,0x26, 0,8,0,0 };
  veh_ipv4_info_t ii;

  CHECK(veh_ipv4_parse(ip, sizeof(ip), &ii) == 0 && ii.src == 0x0A000005 && ii.dst == 0x0A000009);
  CHECK(ii.proto == 17 && ii.has_ports && ii.sport == 30501 && ii.dport == 30502 && ii.total_len == 28);
  ip[6] = 0x00; ip[7] = 0x10;                     /* 두 번째 조각 : 포트 없음 */
  CHECK(veh_ipv4_parse(ip, sizeof(ip), &ii) == 0 && !ii.has_ports);
  ip[0] = 0x65; CHECK(veh_ipv4_parse(ip, sizeof(ip), &ii) == -1);   /* IPv6 아님 */
  CHECK(veh_ipv4_parse(ip, 10, &ii) == -1);
}

static void test_ecu(void)
{
  veh_frame_info_t fi;
  veh_ipv4_info_t  ii;
  const veh_ecu_t *e;
  uint8_t pl[4] = { 1, 2, 3, 4 };
  uint32_t t, i;

  veh_ecu_reset();
  memset(&fi, 0, sizeof(fi));
  memset(&ii, 0, sizeof(ii));
  fi.src[0] = 0x02; fi.src[5] = 0x01; fi.vid = 2; fi.ethertype = 0x0800;
  ii.src = 0x0A000005; ii.proto = 17; ii.has_ports = 1; ii.sport = 30501; ii.dport = 30501; ii.total_len = 100;

  /* 10 ms 주기 이벤트 100 개 (±200 us 흔들림) */
  for (i = 0, t = 1000000U; i < 100U; i++)
  {
    uint32_t jit = (i % 3U == 0U) ? 200U : ((i % 3U == 1U) ? 0U : (uint32_t)-200);
    t += 10000U + jit;
    veh_ecu_on_frame(&fi, &ii, t);
    veh_ecu_on_someip(ii.src, 0x1001, 0x8001, SOMEIP_MT_NOTIFICATION, pl, sizeof(pl), t);
  }
  veh_ecu_on_offer(ii.src, 0x1001, 0x0002, 1, t);
  veh_ecu_on_offer(ii.src, 0x1001, 0x0002, 1, t);   /* 중복은 1 개로 */

  CHECK(veh_ecu_count() == 1);
  e = veh_ecu_find(0x0A000005);
  CHECK(e != NULL && e->packets == 100 && e->bytes == 10000 && e->vid == 2 && !e->mac_changed);
  CHECK(e->num_offers == 1 && e->offers[0].instance_id == 2);
  CHECK(e->flows[0].iv.count == 100 && e->flows[1].iv.count == 0);
  CHECK(e->events[0].iv.count == 100 && e->events[0].last_len == 4 && e->events[0].last_payload[3] == 4);
  CHECK(e->events[0].iv.avg_us > 9700 && e->events[0].iv.avg_us < 10300);
  CHECK(e->events[0].iv.min_us == 9800 && e->events[0].iv.max_us == 10200);

  /* 같은 IP 가 다른 MAC 으로 (게이트웨이 경유) */
  fi.src[5] = 0x99;
  veh_ecu_on_frame(&fi, &ii, t + 10000U);
  CHECK(veh_ecu_find(0x0A000005)->mac_changed == 1);

  /* 다른 IP, 흐름 테이블 넘침 */
  ii.src = 0x0A000006;
  for (i = 0; i < APP_ECU_MAX_FLOWS + 3U; i++)
  {
    ii.dport = (uint16_t)(40000U + i);
    veh_ecu_on_frame(&fi, &ii, t + i);
  }
  e = veh_ecu_find(0x0A000006);
  CHECK(veh_ecu_count() == 2 && e != NULL && e->flow_overflow == 3);

  /* ECU 테이블 넘침 */
  for (i = 0; i < APP_ECU_MAX + 2U; i++)
  {
    ii.src = 0xC0A80000U + i;
    veh_ecu_on_frame(&fi, &ii, t);
  }
  CHECK(veh_ecu_count() == APP_ECU_MAX && veh_ecu_dropped() == 4);

  /* 출력 함수가 죽지 않는지 */
  CHECK(veh_ecu_print_next(t + 20000U, 5000000U) == 1);
  veh_ecu_print_summary(t + 20000U, 5000000U);
}

static FILE *vec;

static void vector(const char *name, uint8_t id, const uint8_t *b, uint8_t n)
{
  uint8_t i;

  fprintf(vec, "%s %02X ", name, id);
  for (i = 0; i < n; i++)
  {
    fprintf(vec, "%02X", b[i]);
  }
  fprintf(vec, "\n");
}

static void test_vcan(void)
{
  uint8_t b[64], n;
  vcan_status_t st;
  vcan_eth_stats_t es;
  vcan_service_t sv;
  vcan_command_t c;
  veh_frame_info_t fi;
  veh_ipv4_info_t ii;
  const veh_ecu_t *e;
  uint8_t pl[10] = { 0xDE, 0xAD, 0xBE, 0xEF, 5, 6, 7, 8, 9, 10 };
  uint32_t i;

  /* CAN FD 길이 */
  CHECK(vcan_round_len(9) == 12 && vcan_round_len(33) == 48 && vcan_round_len(64) == 64 && vcan_round_len(0) == 0);
  CHECK(vcan_len_to_dlc(8) == 8 && vcan_len_to_dlc(12) == 9 && vcan_len_to_dlc(48) == 14 && vcan_len_to_dlc(64) == 15);
  CHECK(vcan_dlc_to_len(13) == 32 && vcan_dlc_to_len(15) == 64);

  vec = fopen("vcan_vectors.txt", "w");
  CHECK(vec != NULL);
  if (vec == NULL) return;
  fprintf(vec, "# generated by tests/test_parsers.c (C encoder) - checked by test_vcan_decode.py\n");

  memset(&st, 0, sizeof(st));
  st.flags = VCAN_ST_LINK_UP | VCAN_ST_PHY_READY | VCAN_ST_SOMEIP_ON;
  st.phy_role = 1; st.sqi = 6; st.uptime_ms = 123456; st.ecu_count = 3; st.service_count = 2;
  st.rx_frames = 1000; st.tx_frames = 20; st.rx_lost = 7; st.someip_messages = 500; st.ip = 0xC0A80A0A;
  n = vcan_encode_status(b, &st);
  CHECK(n == 32 && b[0] == 1 && b[1] == 0x0B && b[28] == 192 && b[31] == 10 && b[4] == 0x40 && b[5] == 0xE2);
  vector("STATUS", VCAN_ID_STATUS, b, n);

  for (i = 0; i < 12U; i++) ((uint32_t *)&es)[i] = 100U + i;
  n = vcan_encode_eth_stats(b, &es);
  CHECK(n == 48 && b[8] == 102 && b[44] == 111);
  vector("ETH_STATS", VCAN_ID_ETH_STATS, b, n);

  /* ECU 테이블에서 ECU / 흐름 / 이벤트 */
  veh_ecu_reset();
  memset(&fi, 0, sizeof(fi)); memset(&ii, 0, sizeof(ii));
  fi.src[0] = 0x02; fi.src[1] = 0x1A; fi.src[5] = 0x01; fi.vid = 2;
  ii.src = 0x0A000005; ii.proto = 17; ii.sport = 30501; ii.dport = 30502; ii.total_len = 60;
  for (i = 0; i < 5U; i++)
  {
    veh_ecu_on_frame(&fi, &ii, 1000000U + i * 10000U);
    veh_ecu_on_someip(ii.src, 0x1001, 0x8001, SOMEIP_MT_NOTIFICATION, pl, sizeof(pl), 1000000U + i * 10000U);
  }
  veh_ecu_on_offer(ii.src, 0x1001, 0x0002, 1, 1040000U);
  e = veh_ecu_find(0x0A000005);
  n = vcan_encode_ecu(b, e, 0, 1, 1050000U, 5000000U);
  CHECK(n == 48 && b[2] == 10 && b[5] == 5 && b[7] == 0x1A && b[12] == 2 && b[13] == 0 && b[15] == 1 && b[16] == 5);
  CHECK(b[24] == 10 && b[28] == 1 && b[29] == 1 && b[32] == 0x01 && b[33] == 0x10 && b[34] == 0x02);
  vector("ECU", VCAN_ID_ECU, b, n);
  n = vcan_encode_flow(b, e->ip, &e->flows[0]);
  CHECK(n == 24 && b[4] == 17 && b[6] == 0x25 && b[7] == 0x77 && b[12] == 5 && b[16] == 0x10 && b[17] == 0x27);
  vector("FLOW", VCAN_ID_FLOW, b, n);
  n = vcan_encode_event(b, e->ip, &e->events[0]);
  CHECK(n == 48 && b[4] == 0x01 && b[5] == 0x10 && b[7] == 0x80 && b[8] == 2 && b[9] == 8 && b[28] == 10 && b[32] == 0xDE && b[39] == 8);
  vector("EVENT", VCAN_ID_EVENT, b, n);

  memset(&sv, 0, sizeof(sv));
  sv.service_id = 0x1234; sv.instance_id = 1; sv.major = 2; sv.l4_proto = 0x11; sv.minor = 7;
  sv.ttl = 0xFFFFFF; sv.ep_ip = 0x0A000005; sv.ep_port = 30501; sv.from_ip = 0x0A000005; sv.offers = 42;
  n = vcan_encode_service(b, &sv);
  CHECK(n == 32 && b[0] == 0x34 && b[1] == 0x12 && b[16] == 10 && b[20] == 0x25 && b[28] == 42);
  vector("SERVICE", VCAN_ID_SERVICE, b, n);

  n = vcan_encode_dump_end(b, 9, 1);
  vector("DUMP_END", VCAN_ID_DUMP_END, b, n);
  n = vcan_encode_response(b, VCAN_CMD_PING, VCAN_RES_OK, 5000, 77);
  CHECK(n == 16 && b[0] == 1 && b[4] == 0x00 && b[5] == 0x01 && b[12] == 77);
  vector("RESPONSE", VCAN_ID_RESPONSE, b, n);
  fclose(vec);

  /* 명령 해석 */
  {
    const uint8_t cmd[8] = { VCAN_CMD_SET_STATUS_MS, 0xF4, 0x01, 0, 0, 0, 0, 0 };
    CHECK(vcan_decode_command(cmd, 8, &c) == 0 && c.cmd == VCAN_CMD_SET_STATUS_MS && c.len == 7 && c.arg[0] == 0xF4 && c.arg[1] == 0x01);
    CHECK(vcan_decode_command(cmd, 0, &c) == -1);
  }
}

int main(void)
{
  test_frame();
  test_someip();
  test_ipv4();
  test_ecu();
  test_vcan();
  printf(fails ? "%d FAILED\n" : "all tests passed\n", fails);
  return fails != 0;
}
