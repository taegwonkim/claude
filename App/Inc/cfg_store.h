/**
  ******************************************************************************
  * @file    cfg_store.h
  * @brief   시스템 설정(AP/서버/출력) — RAM 캐시 + 외부 플래시 이중 슬롯 저장
  ******************************************************************************
  */
#ifndef CFG_STORE_H
#define CFG_STORE_H

#include "app_types.h"

#define SD_CFG_MAGIC     0x53444347u   /* "SDCG" */
#define SD_CFG_VERSION   1u

#pragma pack(push, 1)
typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;                       /* sizeof(sd_cfg_t) */

  char     ssid[SD_SSID_MAX + 1];      /* AP SSID            */
  char     pass[SD_PASS_MAX + 1];      /* AP 비밀번호         */

  char     srv_ip[SD_IPSTR_MAX];       /* 서버(PC) IP        */
  uint16_t srv_port;                   /* 서버 포트 (50001)   */

  uint8_t  dhcp;                       /* 1=DHCP, 0=static   */
  char     sta_ip[SD_IPSTR_MAX];       /* 모듈 고정 IP        */
  char     gw[SD_IPSTR_MAX];           /* 게이트웨이          */
  char     mask[SD_IPSTR_MAX];         /* 넷마스크            */

  uint32_t sample_ms;                  /* FPGA 샘플 주기(참고) */

  uint8_t  out_uart;                   /* RS485 출력 on/off  */
  uint8_t  out_usb;                    /* USB CDC 출력       */
  uint8_t  out_wifi;                   /* WiFi 출력          */
  uint8_t  auto_start;                 /* 부팅 시 FPGA START */

  uint8_t  rsv[8];
  uint32_t crc32;                      /* 이 필드 앞까지의 CRC32 */
} sd_cfg_t;
#pragma pack(pop)

extern sd_cfg_t g_cfg;                 /* RAM 캐시 (g_mtxCfg 로 보호) */

/* 기본값으로 채운다 (플래시 접근 없음) */
void     cfgstore_defaults(sd_cfg_t *c);

/* 플래시에서 적재. 슬롯 A 실패 시 B, 둘 다 실패면 기본값 적용 후 SD_ERR 반환 */
sd_res_t cfgstore_load(void);

/* RAM 캐시를 슬롯 A/B 양쪽에 기록 */
sd_res_t cfgstore_save(void);

/* 초기화 : w25q_init() + cfgstore_load() */
sd_res_t cfgstore_init(void);

/* 뮤텍스 헬퍼 */
bool     cfgstore_lock(uint32_t timeout_ms);
void     cfgstore_unlock(void);

/* "a.b.c.d" 형식 검사 */
bool     cfgstore_is_valid_ip(const char *s);

#endif /* CFG_STORE_H */
