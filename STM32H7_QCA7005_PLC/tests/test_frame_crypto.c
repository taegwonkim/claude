/* tests/test_frame_crypto.c - framing state machine, SHA-256 and NID derivation */
#include <string.h>
#include "test_common.h"
#include "qca7k_frame.h"
#include "sha256.h"
#include "hpgp.h"

static void test_sha256(void)
{
    static const uint8_t abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    static const uint8_t two_blocks[32] = { /* "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" */
        0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
        0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1
    };
    uint8_t d[32];

    printf("sha256\n");
    sha256("abc", 3, d);
    CHECK(memcmp(d, abc, 32) == 0);
    sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, d);
    CHECK(memcmp(d, two_blocks, 32) == 0);
}

static void test_nid(void)
{
    /* HomePlug AV default network password "HomePlugAV" gives this NMK, and
     * open-plc-utils / QCA tools report NID B0:F2:E6:95:66:6B:03 for it */
    static const uint8_t nmk[16] = {
        0x50, 0xD3, 0xE4, 0x93, 0x3F, 0x85, 0x5B, 0x70, 0x40, 0x78, 0x4D, 0xF8, 0x15, 0xAA, 0x8D, 0xB7
    };
    static const uint8_t nid_expected[7] = {0xB0, 0xF2, 0xE6, 0x95, 0x66, 0x6B, 0x03};
    uint8_t nid[7];

    printf("nid from nmk\n");
    hpgp_nid_from_nmk(nid, nmk, 0);
    if (memcmp(nid, nid_expected, 7)) {
        printf("  got %02X%02X%02X%02X%02X%02X%02X\n", nid[0], nid[1], nid[2], nid[3], nid[4], nid[5], nid[6]);
    }
    CHECK(memcmp(nid, nid_expected, 7) == 0);
}

/* build one packet as the QCA7K puts it into its read buffer */
static unsigned put_packet(uint8_t *out, const uint8_t *eth, uint16_t len)
{
    unsigned n = 0;
    uint16_t total = (uint16_t)(QCAFRM_HEADER_LEN + len + QCAFRM_FOOTER_LEN);
    out[n++] = 0; out[n++] = 0;                     /* HW length, big endian 32 bit */
    out[n++] = (uint8_t)(total >> 8); out[n++] = (uint8_t)total;
    n += qcafrm_create_header(&out[n], len);
    memcpy(&out[n], eth, len); n += len;
    n += qcafrm_create_footer(&out[n]);
    return n;
}

static void test_framing(void)
{
    uint8_t eth1[60], eth2[100], stream[400], buf[QCAFRM_MAX_LEN];
    unsigned n = 0, i, got = 0;
    int lens[4];
    qcafrm_handle_t h;

    printf("framing\n");
    for (i = 0; i < sizeof(eth1); i++) eth1[i] = (uint8_t)i;
    for (i = 0; i < sizeof(eth2); i++) eth2[i] = (uint8_t)(0xA0 + i);

    n += put_packet(&stream[n], eth1, sizeof(eth1));
    stream[n++] = 0x13;                              /* garbage between packets */
    n += put_packet(&stream[n], eth2, sizeof(eth2));

    qcafrm_fsm_init_spi(&h);
    for (i = 0; i < n; i++) {
        int32_t r = qcafrm_fsm_decode(&h, buf, sizeof(buf), stream[i]);
        if (r > 0) {
            lens[got] = r;
            if (got == 0) CHECK(memcmp(buf, eth1, sizeof(eth1)) == 0);
            if (got == 1) CHECK(memcmp(buf, eth2, sizeof(eth2)) == 0);
            got++;
        }
    }
    CHECK_EQ(got, 2);
    CHECK_EQ(lens[0], 60);
    CHECK_EQ(lens[1], 100);

    /* too short length field -> INVLEN, decoder recovers */
    {
        uint8_t bad[] = {0, 0, 0, 20, 0xAA, 0xAA, 0xAA, 0xAA, 10, 0, 0, 0};
        int seen_inv = 0;
        qcafrm_fsm_init_spi(&h);
        for (i = 0; i < sizeof(bad); i++)
            if (qcafrm_fsm_decode(&h, buf, sizeof(buf), bad[i]) == QCAFRM_INVLEN)
                seen_inv = 1;
        CHECK(seen_inv);
        CHECK_EQ(h.state, h.init);
    }
}

void test_frame_crypto(void)
{
    test_sha256();
    test_nid();
    test_framing();
}
