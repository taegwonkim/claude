/*
 * hpgp.c - HomePlug Green PHY management message helpers
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include "hpgp.h"
#include "sha256.h"

const uint8_t HPGP_MAC_LOCAL[PLC_MAC_LEN]     = {0x00, 0xB0, 0x52, 0x00, 0x00, 0x01};
const uint8_t HPGP_MAC_BROADCAST[PLC_MAC_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void eth_hdr(uint8_t *f, const uint8_t dst[6], const uint8_t src[6])
{
    memcpy(&f[HPGP_OFS_ODA], dst, 6);
    memcpy(&f[HPGP_OFS_OSA], src, 6);
    plc_put_be16(&f[HPGP_OFS_MTYPE], ETH_P_HPAV);
}

uint16_t hpgp_hdr(uint8_t *f, const uint8_t dst[6], const uint8_t src[6], uint16_t mmtype)
{
    eth_hdr(f, dst, src);
    f[HPGP_OFS_MMV] = HPGP_MMV_HOMEPLUG;
    plc_put_le16(&f[HPGP_OFS_MMTYPE], mmtype);
    f[17] = 0;   /* FMSN */
    f[18] = 0;   /* FMID */
    return HPGP_OFS_PAYLOAD;
}

uint16_t hpgp_vs_hdr(uint8_t *f, const uint8_t dst[6], const uint8_t src[6], uint16_t mmtype)
{
    eth_hdr(f, dst, src);
    f[HPGP_OFS_MMV] = HPGP_MMV_VENDOR;
    plc_put_le16(&f[HPGP_OFS_MMTYPE], mmtype);
    f[HPGP_OFS_VS_OUI + 0] = 0x00;
    f[HPGP_OFS_VS_OUI + 1] = 0xB0;
    f[HPGP_OFS_VS_OUI + 2] = 0x52;
    return HPGP_OFS_VS_PAYLOAD;
}

int32_t hpgp_mmtype(const uint8_t *f, uint16_t len, uint8_t *mmv)
{
    if (len < HPGP_OFS_PAYLOAD)
        return -1;
    if (plc_get_be16(&f[HPGP_OFS_MTYPE]) != ETH_P_HPAV)
        return -1;
    if (mmv)
        *mmv = f[HPGP_OFS_MMV];
    return plc_get_le16(&f[HPGP_OFS_MMTYPE]);
}

void hpgp_nid_from_nmk(uint8_t nid[SLAC_NID_LEN], const uint8_t nmk[SLAC_NMK_LEN], uint8_t level)
{
    uint8_t d[SHA256_DIGEST_LEN];
    int i;

    /* same as open-plc-utils key/HPAVKeyNID.c: hash, then rehash 4 times */
    sha256(nmk, SLAC_NMK_LEN, d);
    for (i = 0; i < 4; i++)
        sha256(d, sizeof(d), d);
    d[SLAC_NID_LEN - 1] = (uint8_t)((d[SLAC_NID_LEN - 1] >> 4) | (uint8_t)(level << 4));
    memcpy(nid, d, SLAC_NID_LEN);
}

uint16_t hpgp_build_set_key(uint8_t *f, const uint8_t src[6],
                            const uint8_t nid[SLAC_NID_LEN], const uint8_t nmk[SLAC_NMK_LEN])
{
    memset(f, 0, SK_REQ_LEN);
    hpgp_hdr(f, HPGP_MAC_LOCAL, src, CM_SET_KEY | MMTYPE_REQ);
    f[SK_REQ_KEYTYPE] = 0x01;                 /* NMK */
    memset(&f[SK_REQ_MYNONCE], 0xAA, 4);
    memset(&f[SK_REQ_YOURNONCE], 0x00, 4);
    f[SK_REQ_PID] = 0x04;                     /* HLE protocol */
    plc_put_le16(&f[SK_REQ_PRN], 0x0000);
    f[SK_REQ_PMN] = 0x00;
    f[SK_REQ_CCOCAP] = 0x00;
    memcpy(&f[SK_REQ_NID], nid, SLAC_NID_LEN);
    f[SK_REQ_NEWEKS] = 0x01;
    memcpy(&f[SK_REQ_NEWKEY], nmk, SLAC_NMK_LEN);
    return SK_REQ_LEN;
}

uint16_t hpgp_build_sw_ver(uint8_t *f, const uint8_t src[6])
{
    memset(f, 0, 60);
    hpgp_vs_hdr(f, HPGP_MAC_LOCAL, src, VS_SW_VER | MMTYPE_REQ);
    return 60;
}

uint16_t hpgp_build_nw_info(uint8_t *f, const uint8_t src[6])
{
    memset(f, 0, 60);
    hpgp_vs_hdr(f, HPGP_MAC_LOCAL, src, VS_NW_INFO | MMTYPE_REQ);
    return 60;
}

int hpgp_parse_nw_info(const uint8_t *f, uint16_t len, uint8_t nid[SLAC_NID_LEN])
{
    const uint8_t *net;

    if (len < NW_CNF_NUMAVLNS + 1)
        return -1;
    if (f[NW_CNF_NUMAVLNS] == 0)
        return 0;
    if (len < NW_CNF_NUMAVLNS + 1 + NW_NET_LEN)
        return -1;
    net = &f[NW_CNF_NUMAVLNS + 1];
    if (nid)
        memcpy(nid, net, SLAC_NID_LEN);
    return net[NW_NET_NUMSTAS];
}

const char *hpgp_mmtype_name(uint16_t mmtype)
{
    static const struct { uint16_t base; const char *name; } t[] = {
        {CM_SET_KEY, "CM_SET_KEY"}, {CM_AMP_MAP, "CM_AMP_MAP"},
        {CM_SLAC_PARAM, "CM_SLAC_PARAM"}, {CM_START_ATTEN_CHAR, "CM_START_ATTEN_CHAR"},
        {CM_ATTEN_CHAR, "CM_ATTEN_CHAR"}, {CM_MNBC_SOUND, "CM_MNBC_SOUND"},
        {CM_VALIDATE, "CM_VALIDATE"}, {CM_SLAC_MATCH, "CM_SLAC_MATCH"},
        {CM_ATTEN_PROFILE, "CM_ATTEN_PROFILE"}, {VS_SW_VER, "VS_SW_VER"},
        {VS_NW_INFO, "VS_NW_INFO"},
    };
    unsigned i;
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
        if ((mmtype & ~3u) == t[i].base)
            return t[i].name;
    return "?";
}
