/*
 * hpgp.h - HomePlug Green PHY / ISO 15118-3 management messages (MME)
 *
 * Message codes and field layouts follow Qualcomm Atheros open-plc-utils
 * (mme/homeplug.h, mme/qualcomm.h, slac/slac.h; Copyright (c) 2013 Qualcomm
 * Atheros, Inc., BSD-style licence) and ISO 15118-3 Annex A.
 *
 * Frames are built with byte offsets instead of packed structs so the code is
 * independent of compiler packing and alignment rules.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef HPGP_H
#define HPGP_H

#include <stdint.h>
#include "plc_util.h"

#define ETH_P_HPAV          0x88E1u

#define HPGP_MMV_HOMEPLUG   0x01u     /* HomePlug AV 1.1 (with FMSN/FMID) */
#define HPGP_MMV_VENDOR     0x00u     /* Qualcomm vendor specific */

#define MMTYPE_REQ          0x0000u
#define MMTYPE_CNF          0x0001u
#define MMTYPE_IND          0x0002u
#define MMTYPE_RSP          0x0003u

/* HomePlug AV / GP */
#define CM_SET_KEY          0x6008u
#define CM_AMP_MAP          0x601Cu
#define CM_SLAC_PARAM       0x6064u
#define CM_START_ATTEN_CHAR 0x6068u
#define CM_ATTEN_CHAR       0x606Cu
#define CM_MNBC_SOUND       0x6074u
#define CM_VALIDATE         0x6078u
#define CM_SLAC_MATCH       0x607Cu
#define CM_ATTEN_PROFILE    0x6084u

/* Qualcomm vendor specific (OUI 00:B0:52) */
#define VS_SW_VER           0xA000u
#define VS_NW_INFO          0xA038u

/* sizes */
#define SLAC_RUNID_LEN      8u
#define SLAC_UNIQUE_ID_LEN  17u
#define SLAC_RND_LEN        16u
#define SLAC_NID_LEN        7u
#define SLAC_NMK_LEN        16u
#define SLAC_GROUPS         58u

#define HPGP_FRAME_MAX      256u      /* largest MME built here (ATTEN_CHAR.IND = 129) */

/* ---- header offsets -------------------------------------------------------- */
#define HPGP_OFS_ODA        0
#define HPGP_OFS_OSA        6
#define HPGP_OFS_MTYPE      12
#define HPGP_OFS_MMV        14
#define HPGP_OFS_MMTYPE     15
#define HPGP_OFS_PAYLOAD    19        /* MMV=1: MMV, MMTYPE, FMSN, FMID */
#define HPGP_OFS_VS_OUI     17
#define HPGP_OFS_VS_PAYLOAD 20        /* MMV=0: MMV, MMTYPE, OUI[3] */

/* ---- CM_SLAC_PARAM.REQ ---------------------------------------------------- */
#define SP_REQ_APP          19
#define SP_REQ_SEC          20
#define SP_REQ_RUNID        21        /* .. 28 */
#define SP_REQ_LEN          29
/* ---- CM_SLAC_PARAM.CNF ---------------------------------------------------- */
#define SP_CNF_MSOUND_TGT   19        /* 6 */
#define SP_CNF_NUM_SOUNDS   25
#define SP_CNF_TIME_OUT     26
#define SP_CNF_RESP_TYPE    27
#define SP_CNF_FWD_STA      28        /* 6 */
#define SP_CNF_APP          34
#define SP_CNF_SEC          35
#define SP_CNF_RUNID        36        /* 8 */
#define SP_CNF_LEN          44
/* ---- CM_START_ATTEN_CHAR.IND ---------------------------------------------- */
#define SA_IND_APP          19
#define SA_IND_SEC          20
#define SA_IND_NUM_SOUNDS   21
#define SA_IND_TIME_OUT     22
#define SA_IND_RESP_TYPE    23
#define SA_IND_FWD_STA      24        /* 6 */
#define SA_IND_RUNID        30        /* 8 */
#define SA_IND_LEN          38
/* ---- CM_MNBC_SOUND.IND ---------------------------------------------------- */
#define MS_IND_APP          19
#define MS_IND_SEC          20
#define MS_IND_SENDER_ID    21        /* 17 */
#define MS_IND_CNT          38
#define MS_IND_RUNID        39        /* 8 */
#define MS_IND_RSVD         47        /* 8 */
#define MS_IND_RND          55        /* 16 */
#define MS_IND_LEN          71
/* ---- CM_ATTEN_PROFILE.IND (local EVSE modem -> EVSE host) ------------------ */
#define AP_IND_PEV_MAC      19        /* 6 */
#define AP_IND_NUM_GROUPS   25
#define AP_IND_RSVD         26
#define AP_IND_AAG          27        /* NumGroups */
/* ---- CM_ATTEN_CHAR.IND ---------------------------------------------------- */
#define AC_IND_APP          19
#define AC_IND_SEC          20
#define AC_IND_SRC_ADDR     21        /* 6 */
#define AC_IND_RUNID        27        /* 8 */
#define AC_IND_SRC_ID       35        /* 17 */
#define AC_IND_RESP_ID      52        /* 17 */
#define AC_IND_NUM_SOUNDS   69
#define AC_IND_NUM_GROUPS   70
#define AC_IND_AAG          71        /* 58 */
#define AC_IND_LEN          (AC_IND_AAG + SLAC_GROUPS)   /* 129 */
/* ---- CM_ATTEN_CHAR.RSP ---------------------------------------------------- */
#define AC_RSP_APP          19
#define AC_RSP_SEC          20
#define AC_RSP_SRC_ADDR     21
#define AC_RSP_RUNID        27
#define AC_RSP_SRC_ID       35
#define AC_RSP_RESP_ID      52
#define AC_RSP_RESULT       69
#define AC_RSP_LEN          70
/* ---- CM_SLAC_MATCH.REQ / CNF ---------------------------------------------- */
#define SM_APP              19
#define SM_SEC              20
#define SM_MVF_LEN          21        /* LE16 */
#define SM_PEV_ID           23        /* 17 */
#define SM_PEV_MAC          40        /* 6 */
#define SM_EVSE_ID          46        /* 17 */
#define SM_EVSE_MAC         63        /* 6 */
#define SM_RUNID            69        /* 8 */
#define SM_RSVD1            77        /* 8 */
#define SM_REQ_LEN          85
#define SM_REQ_MVF_LEN      (SM_REQ_LEN - SM_PEV_ID)   /* 62 = 0x3E */
#define SM_CNF_NID          85        /* 7 */
#define SM_CNF_RSVD2        92
#define SM_CNF_NMK          93        /* 16 */
#define SM_CNF_LEN          109
#define SM_CNF_MVF_LEN      (SM_CNF_LEN - SM_PEV_ID)   /* 86 = 0x56 */
/* ---- CM_SET_KEY.REQ ------------------------------------------------------- */
#define SK_REQ_KEYTYPE      19
#define SK_REQ_MYNONCE      20        /* 4 */
#define SK_REQ_YOURNONCE    24        /* 4 */
#define SK_REQ_PID          28
#define SK_REQ_PRN          29        /* LE16 */
#define SK_REQ_PMN          31
#define SK_REQ_CCOCAP       32
#define SK_REQ_NID          33        /* 7 */
#define SK_REQ_NEWEKS       40
#define SK_REQ_NEWKEY       41        /* 16 */
#define SK_REQ_LEN          60
#define SK_CNF_RESULT       19
/* ---- VS_SW_VER.CNF -------------------------------------------------------- */
#define SW_CNF_MSTATUS      20
#define SW_CNF_DEVICEID     21
#define SW_CNF_VERLEN       22
#define SW_CNF_VERSION      23
/* ---- VS_NW_INFO.CNF (MMV 0) ------------------------------------------------ */
#define NW_CNF_NUMAVLNS     20
#define NW_NET_LEN          18        /* NID7 SNID TEI ROLE CCO_MAC6 CCO_TEI NUMSTAS */
#define NW_NET_NUMSTAS      17
#define NW_STA_LEN          15        /* MAC6 TEI BDA6 AVGTX AVGRX */

/* ISO 15118-3 values */
#define SLAC_APP_TYPE_PEV_EVSE  0x00u
#define SLAC_SECURITY_NONE      0x00u
#define SLAC_RESP_TYPE_OTHER    0x01u   /* "other GP station" (EVSE host) */

/* Qualcomm local management address: frames to it are consumed by the
 * local modem (open-plc-utils LOCALCAST) */
extern const uint8_t HPGP_MAC_LOCAL[PLC_MAC_LEN];
extern const uint8_t HPGP_MAC_BROADCAST[PLC_MAC_LEN];

/* write ethernet + MMV1 header, returns payload offset (19) */
uint16_t hpgp_hdr(uint8_t *f, const uint8_t dst[6], const uint8_t src[6], uint16_t mmtype);
/* write ethernet + vendor header (MMV 0, OUI 00:B0:52), returns 20 */
uint16_t hpgp_vs_hdr(uint8_t *f, const uint8_t dst[6], const uint8_t src[6], uint16_t mmtype);

/* if frame is a HomePlug MME return its MMTYPE and MMV, else -1 */
int32_t  hpgp_mmtype(const uint8_t *f, uint16_t len, uint8_t *mmv);

/* NID from NMK (HomePlug AV: 5x SHA-256, security level in bits 4..5 of byte 6) */
void     hpgp_nid_from_nmk(uint8_t nid[SLAC_NID_LEN], const uint8_t nmk[SLAC_NMK_LEN], uint8_t level);

/* CM_SET_KEY.REQ to the local modem. returns frame length */
uint16_t hpgp_build_set_key(uint8_t *f, const uint8_t src[6],
                            const uint8_t nid[SLAC_NID_LEN], const uint8_t nmk[SLAC_NMK_LEN]);
uint16_t hpgp_build_sw_ver(uint8_t *f, const uint8_t src[6]);
uint16_t hpgp_build_nw_info(uint8_t *f, const uint8_t src[6]);

/* parse VS_NW_INFO.CNF: number of other stations in the first AVLN, -1 if bad.
 * nid (optional) receives the NID of that network. */
int      hpgp_parse_nw_info(const uint8_t *f, uint16_t len, uint8_t nid[SLAC_NID_LEN]);

const char *hpgp_mmtype_name(uint16_t mmtype);

#endif /* HPGP_H */
