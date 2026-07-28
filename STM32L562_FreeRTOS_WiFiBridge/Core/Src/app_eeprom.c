#include <string.h>
#include <stddef.h>
#include "app_eeprom.h"
#include "cmsis_os2.h"

static osMutexId_t s_eepromMutex;

/* ------------------------------------------------------------------------ *
 * Software CRC32 (poly 0xEDB88320), no lookup table - config struct is
 * small (<200 bytes) so this is cheap enough to compute on demand.
 * ------------------------------------------------------------------------ */
static uint32_t Crc32_Compute(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

/* ------------------------------------------------------------------------ *
 * Low level page-aware EEPROM byte access (24LC256-class: 64 byte pages,
 * 16-bit internal address, ~5ms write cycle).
 * ------------------------------------------------------------------------ */
static bool Eeprom_WriteBytes(uint16_t addr, const uint8_t *data, uint16_t len)
{
    while (len > 0U) {
        uint16_t page_offset = addr % EEPROM_PAGE_SIZE_BYTES;
        uint16_t chunk = EEPROM_PAGE_SIZE_BYTES - page_offset;
        if (chunk > len) {
            chunk = len;
        }

        HAL_StatusTypeDef st = HAL_I2C_Mem_Write(EEPROM_I2C_HANDLE,
                                                  EEPROM_I2C_ADDR_HAL,
                                                  addr,
                                                  I2C_MEMADD_SIZE_16BIT,
                                                  (uint8_t *)data,
                                                  chunk,
                                                  EEPROM_I2C_TIMEOUT_MS);
        if (st != HAL_OK) {
            return false;
        }

        /* Wait for internal write cycle to finish (ACK polling). */
        uint32_t start = HAL_GetTick();
        while (HAL_I2C_IsDeviceReady(EEPROM_I2C_HANDLE, EEPROM_I2C_ADDR_HAL, 1,
                                      EEPROM_I2C_TIMEOUT_MS) != HAL_OK) {
            if ((HAL_GetTick() - start) > EEPROM_I2C_TIMEOUT_MS) {
                return false;
            }
            osDelay(1);
        }

        data += chunk;
        addr += chunk;
        len  -= chunk;
    }
    return true;
}

static bool Eeprom_ReadBytes(uint16_t addr, uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(EEPROM_I2C_HANDLE,
                                             EEPROM_I2C_ADDR_HAL,
                                             addr,
                                             I2C_MEMADD_SIZE_16BIT,
                                             data,
                                             len,
                                             EEPROM_I2C_TIMEOUT_MS);
    return (st == HAL_OK);
}

/* ------------------------------------------------------------------------ */

void App_Eeprom_Init(void)
{
    s_eepromMutex = osMutexNew(NULL);
}

void App_Eeprom_SetDefaults(AppConfig_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = CONFIG_MAGIC;
    cfg->dhcp_enable = 1U;
    cfg->server_port = 0U;
}

bool App_Eeprom_LoadConfig(AppConfig_t *out_cfg)
{
    AppConfig_t tmp;
    bool ok = false;

    osMutexAcquire(s_eepromMutex, osWaitForever);
    if (Eeprom_ReadBytes(EEPROM_CONFIG_BASE_ADDR, (uint8_t *)&tmp, sizeof(tmp))) {
        size_t crc_field_offset = offsetof(AppConfig_t, crc32);
        uint32_t crc = Crc32_Compute((const uint8_t *)&tmp, crc_field_offset);
        if (tmp.magic == CONFIG_MAGIC && crc == tmp.crc32) {
            /* make sure strings are NUL-terminated regardless of EEPROM content */
            tmp.ap_ssid[WIFI_SSID_MAX_LEN] = '\0';
            tmp.ap_pass[WIFI_PASS_MAX_LEN] = '\0';
            *out_cfg = tmp;
            ok = true;
        }
    }
    osMutexRelease(s_eepromMutex);

    if (!ok) {
        App_Eeprom_SetDefaults(out_cfg);
    }
    return ok;
}

bool App_Eeprom_SaveConfig(AppConfig_t *cfg)
{
    bool ok;

    cfg->magic = CONFIG_MAGIC;
    size_t crc_field_offset = offsetof(AppConfig_t, crc32);
    cfg->crc32 = Crc32_Compute((const uint8_t *)cfg, crc_field_offset);

    osMutexAcquire(s_eepromMutex, osWaitForever);
    ok = Eeprom_WriteBytes(EEPROM_CONFIG_BASE_ADDR, (const uint8_t *)cfg, sizeof(*cfg));
    osMutexRelease(s_eepromMutex);

    return ok;
}
