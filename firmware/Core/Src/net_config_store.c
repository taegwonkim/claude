#include "net_config_store.h"
#include "w25q40.h"
#include "app_config.h"
#include <string.h>

static uint32_t Crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;

    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = -(crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

void NetConfig_SetDefaults(NetConfig_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = NET_CONFIG_MAGIC;
    cfg->version = NET_CONFIG_VERSION;
    cfg->server_port = APP_DEFAULT_SERVER_PORT;
    cfg->dhcp_enable = 1U;
}

bool NetConfig_Load(NetConfig_t *cfg)
{
    NetConfig_t tmp;
    uint32_t crc_offset = (uint32_t)((uint8_t *)&tmp.crc32 - (uint8_t *)&tmp);
    uint32_t calc_crc;

    if (!W25Q40_Read(APP_FLASH_CONFIG_SECTOR_ADDR, (uint8_t *)&tmp, sizeof(tmp))) {
        NetConfig_SetDefaults(cfg);
        return false;
    }

    if (tmp.magic != NET_CONFIG_MAGIC || tmp.version != NET_CONFIG_VERSION) {
        NetConfig_SetDefaults(cfg);
        return false;
    }

    calc_crc = Crc32((const uint8_t *)&tmp, crc_offset);
    if (calc_crc != tmp.crc32) {
        NetConfig_SetDefaults(cfg);
        return false;
    }

    /* NUL 종단 강제 보장(플래시 손상/구버전 대비) */
    tmp.ssid[NET_CONFIG_SSID_MAX - 1U] = '\0';
    tmp.pass[NET_CONFIG_PASS_MAX - 1U] = '\0';
    tmp.server_ip[sizeof(tmp.server_ip) - 1U] = '\0';
    tmp.static_ip[sizeof(tmp.static_ip) - 1U] = '\0';
    tmp.gateway[sizeof(tmp.gateway) - 1U] = '\0';
    tmp.netmask[sizeof(tmp.netmask) - 1U] = '\0';

    *cfg = tmp;
    return true;
}

bool NetConfig_Save(NetConfig_t *cfg)
{
    uint32_t crc_offset = (uint32_t)((uint8_t *)&cfg->crc32 - (uint8_t *)cfg);

    cfg->magic = NET_CONFIG_MAGIC;
    cfg->version = NET_CONFIG_VERSION;
    cfg->crc32 = Crc32((const uint8_t *)cfg, crc_offset);

    if (!W25Q40_EraseSector(APP_FLASH_CONFIG_SECTOR_ADDR)) {
        return false;
    }
    return W25Q40_Write(APP_FLASH_CONFIG_SECTOR_ADDR, (const uint8_t *)cfg, sizeof(*cfg));
}
