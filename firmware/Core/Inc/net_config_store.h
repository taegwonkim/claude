/**
 * net_config_store.h
 *
 * WiFi/서버 설정 구조체 정의 및 W25Q40 플래시 저장/로드.
 * 저장 위치: docs/프로토콜_명세.md §5 (섹터0, APP_FLASH_CONFIG_SECTOR_ADDR).
 */
#ifndef NET_CONFIG_STORE_H
#define NET_CONFIG_STORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NET_CONFIG_MAGIC     (0x4E434647UL) /* "NCFG" */
#define NET_CONFIG_VERSION   (1U)

#define NET_CONFIG_SSID_MAX  (32U) /* NUL 포함 */
#define NET_CONFIG_PASS_MAX  (64U) /* NUL 포함 */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    char     ssid[NET_CONFIG_SSID_MAX];
    char     pass[NET_CONFIG_PASS_MAX];
    char     server_ip[16];   /* "255.255.255.255" + NUL */
    uint16_t server_port;
    uint8_t  dhcp_enable;     /* 1: DHCP, 0: 정적 IP */
    char     static_ip[16];
    char     gateway[16];
    char     netmask[16];
    uint32_t crc32;           /* magic..netmask까지의 CRC32 (crc32 필드 자신은 제외) */
} NetConfig_t;

/**
 * @brief 공장 초기값(빈 SSID/DHCP on 등)으로 in-RAM 구조체를 채운다.
 */
void NetConfig_SetDefaults(NetConfig_t *cfg);

/**
 * @brief 플래시에서 설정을 읽고 CRC를 검증한다.
 * @return true: 유효한 설정을 cfg에 채움, false: 저장된 설정 없음/손상됨(cfg는 SetDefaults 상태로 채워짐)
 */
bool NetConfig_Load(NetConfig_t *cfg);

/**
 * @brief cfg의 CRC를 재계산해 채운 뒤 플래시 섹터를 지우고 기록한다.
 * @return true: 성공
 */
bool NetConfig_Save(NetConfig_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* NET_CONFIG_STORE_H */
