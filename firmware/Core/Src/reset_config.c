#include "reset_config.h"
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

void ResetConfig_SetDefaults(ResetConfig_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = RESET_CONFIG_MAGIC;
    cfg->version = RESET_CONFIG_VERSION;
    cfg->period_sec = APP_RESET_DEFAULT_PERIOD_SEC;
}

bool ResetConfig_Load(ResetConfig_t *cfg)
{
    ResetConfig_t tmp;
    uint32_t crc_offset = (uint32_t)((uint8_t *)&tmp.crc32 - (uint8_t *)&tmp);
    uint32_t calc_crc;

    if (!W25Q40_Read(APP_FLASH_RESET_CONFIG_SECTOR_ADDR, (uint8_t *)&tmp, sizeof(tmp))) {
        ResetConfig_SetDefaults(cfg);
        return false;
    }

    if (tmp.magic != RESET_CONFIG_MAGIC || tmp.version != RESET_CONFIG_VERSION) {
        ResetConfig_SetDefaults(cfg);
        return false;
    }

    calc_crc = Crc32((const uint8_t *)&tmp, crc_offset);
    if (calc_crc != tmp.crc32) {
        ResetConfig_SetDefaults(cfg);
        return false;
    }

    if (tmp.period_sec < APP_RESET_MIN_PERIOD_SEC || tmp.period_sec > APP_RESET_MAX_PERIOD_SEC) {
        ResetConfig_SetDefaults(cfg);
        return false;
    }

    *cfg = tmp;
    return true;
}

bool ResetConfig_Save(ResetConfig_t *cfg)
{
    uint32_t crc_offset = (uint32_t)((uint8_t *)&cfg->crc32 - (uint8_t *)cfg);

    cfg->magic = RESET_CONFIG_MAGIC;
    cfg->version = RESET_CONFIG_VERSION;
    cfg->crc32 = Crc32((const uint8_t *)cfg, crc_offset);

    if (!W25Q40_EraseSector(APP_FLASH_RESET_CONFIG_SECTOR_ADDR)) {
        return false;
    }
    return W25Q40_Write(APP_FLASH_RESET_CONFIG_SECTOR_ADDR, (const uint8_t *)cfg, sizeof(*cfg));
}
