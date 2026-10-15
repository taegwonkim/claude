/* PC 단위 시험 : veh_frame.c / someip.c   (make -C tests) */
#include <stdio.h>
#include <string.h>
#include "veh_frame.h"
#include "someip.h"

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

int main(void)
{
  test_frame();
  test_someip();
  printf(fails ? "%d FAILED\n" : "all tests passed\n", fails);
  return fails != 0;
}
