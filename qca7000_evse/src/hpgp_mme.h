/*
 * hpgp_mme.h - HomePlug Green PHY / SLAC MME definitions (ISO 15118-3)
 *
 * Field layout follows open-plc-utils (slac/slac.h, mme/homeplug.h).
 * Multi-byte fields are byte arrays so that byte order is explicit:
 *   - EtherType : big endian
 *   - MMTYPE, MVFLength, PRN : little endian
 */
#ifndef HPGP_MME_H
#define HPGP_MME_H

#include <stdint.h>

#define HPGP_PACKED __attribute__((packed))

#define ETH_ALEN               6u
#define ETH_HDR_LEN            14u
#define ETHERTYPE_HOMEPLUG     0x88E1u
#define ETHERTYPE_IPV6         0x86DDu

/* MMTYPE suffixes */
#define MMTYPE_REQ             0x0000u
#define MMTYPE_CNF             0x0001u
#define MMTYPE_IND             0x0002u
#define MMTYPE_RSP             0x0003u

/* HomePlug AV / Green PHY standard MMEs (MMV = 0x01) */
#define CM_SET_KEY             0x6008u
#define CM_SLAC_PARAM          0x6064u
#define CM_START_ATTEN_CHAR    0x6068u
#define CM_ATTEN_CHAR          0x606Cu
#define CM_MNBC_SOUND          0x6074u
#define CM_VALIDATE            0x6078u
#define CM_SLAC_MATCH          0x607Cu
#define CM_ATTEN_PROFILE       0x6084u

/* Qualcomm vendor specific MMEs (MMV = 0x00, OUI 00:B0:52) */
#define VS_SW_VER              0xA000u

#define HPGP_MMV_AV11          0x01u
#define QCA_MMV_VS             0x00u

#define SLAC_RUNID_LEN         8u
#define SLAC_UNIQUE_ID_LEN     17u
#define SLAC_NID_LEN           7u
#define SLAC_NMK_LEN           16u
#define SLAC_GROUPS            58u
#define SLAC_RSVD_LEN          8u

#define SLAC_MVF_REQ_LEN       0x3Eu   /* 62 */
#define SLAC_MVF_CNF_LEN       0x56u   /* 86 */

/* --------------------------------------------------------------------- */

typedef struct HPGP_PACKED {
    uint8_t oda[ETH_ALEN];
    uint8_t osa[ETH_ALEN];
    uint8_t ethertype[2];          /* BE */
} eth_hdr_t;

/* HomePlug AV 1.1 header with fragmentation info (used by all CM_* SLAC MMEs) */
typedef struct HPGP_PACKED {
    eth_hdr_t eth;
    uint8_t   mmv;                 /* 0x01 */
    uint8_t   mmtype[2];           /* LE */
    uint8_t   fmsn;
    uint8_t   fmid;
} hpgp_hdr_t;

/* Qualcomm vendor-specific header */
typedef struct HPGP_PACKED {
    eth_hdr_t eth;
    uint8_t   mmv;                 /* 0x00 */
    uint8_t   mmtype[2];           /* LE */
    uint8_t   oui[3];              /* 00 B0 52 */
} qca_vs_hdr_t;

typedef struct HPGP_PACKED {
    qca_vs_hdr_t h;
    uint8_t mstatus;
    uint8_t mdeviceid;
    uint8_t mverlength;
    uint8_t mversion[128];         /* actual length = mverlength */
} vs_sw_ver_cnf_t;

/* ---- CM_SET_KEY --------------------------------------------------------- */
typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t key_type;              /* 0x01 = NMK */
    uint8_t my_nonce[4];
    uint8_t your_nonce[4];
    uint8_t pid;                   /* 0x04 = HLE protocol */
    uint8_t prn[2];
    uint8_t pmn;
    uint8_t cco_capability;
    uint8_t nid[SLAC_NID_LEN];
    uint8_t new_eks;               /* 0x01 */
    uint8_t new_key[SLAC_NMK_LEN];
} cm_set_key_req_t;

typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t result;
    uint8_t my_nonce[4];
    uint8_t your_nonce[4];
    uint8_t pid;
    uint8_t prn[2];
    uint8_t pmn;
    uint8_t cco_capability;
} cm_set_key_cnf_t;

/* ---- CM_SLAC_PARM ------------------------------------------------------- */
typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t application_type;      /* 0x00 = PEV-EVSE matching */
    uint8_t security_type;         /* 0x00 = no security */
    uint8_t run_id[SLAC_RUNID_LEN];
    /* CipherSuite fields follow only if security_type == 1 */
} cm_slac_parm_req_t;

typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t msound_target[ETH_ALEN];   /* FF:FF:FF:FF:FF:FF */
    uint8_t num_sounds;                /* C_EV_match_MNBC */
    uint8_t time_out;                  /* TT_EVSE_match_MNBC, unit 100 ms */
    uint8_t resp_type;                 /* 0x01 */
    uint8_t forwarding_sta[ETH_ALEN];  /* EV MAC */
    uint8_t application_type;
    uint8_t security_type;
    uint8_t run_id[SLAC_RUNID_LEN];
} cm_slac_parm_cnf_t;

/* ---- CM_START_ATTEN_CHAR ------------------------------------------------ */
typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t application_type;
    uint8_t security_type;
    uint8_t num_sounds;
    uint8_t time_out;
    uint8_t resp_type;
    uint8_t forwarding_sta[ETH_ALEN];
    uint8_t run_id[SLAC_RUNID_LEN];
} cm_start_atten_char_ind_t;

/* ---- CM_ATTEN_PROFILE (local QCA7000 firmware -> host) ------------------ */
typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t pev_mac[ETH_ALEN];
    uint8_t num_groups;
    uint8_t rsvd;
    uint8_t aag[SLAC_GROUPS];
} cm_atten_profile_ind_t;

/* ---- CM_ATTEN_CHAR ------------------------------------------------------ */
typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t application_type;
    uint8_t security_type;
    uint8_t source_address[ETH_ALEN];
    uint8_t run_id[SLAC_RUNID_LEN];
    uint8_t source_id[SLAC_UNIQUE_ID_LEN];
    uint8_t resp_id[SLAC_UNIQUE_ID_LEN];
    uint8_t num_sounds;
    uint8_t num_groups;
    uint8_t aag[SLAC_GROUPS];
} cm_atten_char_ind_t;

typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t application_type;
    uint8_t security_type;
    uint8_t source_address[ETH_ALEN];
    uint8_t run_id[SLAC_RUNID_LEN];
    uint8_t source_id[SLAC_UNIQUE_ID_LEN];
    uint8_t resp_id[SLAC_UNIQUE_ID_LEN];
    uint8_t result;                    /* 0x00 = success */
} cm_atten_char_rsp_t;

/* ---- CM_SLAC_MATCH ------------------------------------------------------ */
typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t application_type;
    uint8_t security_type;
    uint8_t mvf_length[2];             /* LE, 0x003E */
    uint8_t pev_id[SLAC_UNIQUE_ID_LEN];
    uint8_t pev_mac[ETH_ALEN];
    uint8_t evse_id[SLAC_UNIQUE_ID_LEN];
    uint8_t evse_mac[ETH_ALEN];
    uint8_t run_id[SLAC_RUNID_LEN];
    uint8_t rsvd[SLAC_RSVD_LEN];
} cm_slac_match_req_t;

typedef struct HPGP_PACKED {
    hpgp_hdr_t h;
    uint8_t application_type;
    uint8_t security_type;
    uint8_t mvf_length[2];             /* LE, 0x0056 */
    uint8_t pev_id[SLAC_UNIQUE_ID_LEN];
    uint8_t pev_mac[ETH_ALEN];
    uint8_t evse_id[SLAC_UNIQUE_ID_LEN];
    uint8_t evse_mac[ETH_ALEN];
    uint8_t run_id[SLAC_RUNID_LEN];
    uint8_t rsvd1[SLAC_RSVD_LEN];
    uint8_t nid[SLAC_NID_LEN];
    uint8_t rsvd2;
    uint8_t nmk[SLAC_NMK_LEN];
} cm_slac_match_cnf_t;

/* Layout sanity checks */
_Static_assert(sizeof(hpgp_hdr_t) == 19, "hpgp_hdr_t");
_Static_assert(sizeof(qca_vs_hdr_t) == 20, "qca_vs_hdr_t");
_Static_assert(sizeof(cm_slac_parm_cnf_t) == 44, "parm_cnf");
_Static_assert(sizeof(cm_atten_char_ind_t) == 129, "atten_char_ind");
_Static_assert(sizeof(cm_slac_match_req_t) == 19 + 4 + SLAC_MVF_REQ_LEN, "match_req");
_Static_assert(sizeof(cm_slac_match_cnf_t) == 19 + 4 + SLAC_MVF_CNF_LEN, "match_cnf");

/* Byte helpers */
static inline uint16_t hpgp_rd_le16(const uint8_t p[2]) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint16_t hpgp_rd_be16(const uint8_t p[2]) { return (uint16_t)((p[0] << 8) | p[1]); }
static inline void hpgp_wr_le16(uint8_t p[2], uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void hpgp_wr_be16(uint8_t p[2], uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

#endif /* HPGP_MME_H */
