#include <string.h>
#include <stddef.h>
#include "app_eeprom.h"
#include "cmsis_os2.h"

/* The whole config struct must fit in a single 256-byte flash page so a
 * save is always exactly "erase sector 0, program page 0". */
_Static_assert(sizeof(AppConfig_t) <= EEPROM_PAGE_SIZE_BYTES,
               "AppConfig_t must fit in one W25Q40CL page (256 bytes)");

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
 * W25Q40CL low level SPI NOR flash access.
 * Chip select is a plain GPIO (software controlled) so the exchange for
 * each command is: CS low -> opcode [+ 24-bit addr] [+ data] -> CS high.
 * ------------------------------------------------------------------------ */
static inline void Flash_CsLow(void)
{
    HAL_GPIO_WritePin(EEPROM_CS_GPIO_PORT, EEPROM_CS_GPIO_PIN, GPIO_PIN_RESET);
}

static inline void Flash_CsHigh(void)
{
    HAL_GPIO_WritePin(EEPROM_CS_GPIO_PORT, EEPROM_CS_GPIO_PIN, GPIO_PIN_SET);
}

static void Flash_BuildAddrCmd(uint8_t *hdr, uint8_t opcode, uint32_t addr)
{
    hdr[0] = opcode;
    hdr[1] = (uint8_t)((addr >> 16) & 0xFFU);
    hdr[2] = (uint8_t)((addr >> 8) & 0xFFU);
    hdr[3] = (uint8_t)(addr & 0xFFU);
}

static uint8_t Flash_ReadStatus1(void)
{
    uint8_t cmd = W25Q_CMD_READ_STATUS1;
    uint8_t status = 0xFFU;

    Flash_CsLow();
    HAL_SPI_Transmit(EEPROM_SPI_HANDLE, &cmd, 1U, EEPROM_SPI_TIMEOUT_MS);
    HAL_SPI_Receive(EEPROM_SPI_HANDLE, &status, 1U, EEPROM_SPI_TIMEOUT_MS);
    Flash_CsHigh();

    return status;
}

static bool Flash_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while ((Flash_ReadStatus1() & W25Q_STATUS1_BUSY_MASK) != 0U) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            return false;
        }
        osDelay(1);
    }
    return true;
}

static void Flash_WriteEnable(void)
{
    uint8_t cmd = W25Q_CMD_WRITE_ENABLE;

    Flash_CsLow();
    HAL_SPI_Transmit(EEPROM_SPI_HANDLE, &cmd, 1U, EEPROM_SPI_TIMEOUT_MS);
    Flash_CsHigh();
}

static bool Flash_SectorErase(uint32_t addr)
{
    uint8_t hdr[4];
    Flash_BuildAddrCmd(hdr, W25Q_CMD_SECTOR_ERASE, addr);

    Flash_WriteEnable();
    Flash_CsLow();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(EEPROM_SPI_HANDLE, hdr, sizeof(hdr), EEPROM_SPI_TIMEOUT_MS);
    Flash_CsHigh();
    if (st != HAL_OK) {
        return false;
    }

    return Flash_WaitReady(EEPROM_ERASE_TIMEOUT_MS);
}

static bool Flash_PageProgram(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint8_t hdr[4];
    Flash_BuildAddrCmd(hdr, W25Q_CMD_PAGE_PROGRAM, addr);

    Flash_WriteEnable();
    Flash_CsLow();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(EEPROM_SPI_HANDLE, hdr, sizeof(hdr), EEPROM_SPI_TIMEOUT_MS);
    if (st == HAL_OK) {
        st = HAL_SPI_Transmit(EEPROM_SPI_HANDLE, (uint8_t *)data, len, EEPROM_SPI_TIMEOUT_MS);
    }
    Flash_CsHigh();
    if (st != HAL_OK) {
        return false;
    }

    return Flash_WaitReady(EEPROM_PROGRAM_TIMEOUT_MS);
}

static bool Flash_ReadData(uint32_t addr, uint8_t *data, uint16_t len)
{
    uint8_t hdr[4];
    Flash_BuildAddrCmd(hdr, W25Q_CMD_READ_DATA, addr);

    Flash_CsLow();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(EEPROM_SPI_HANDLE, hdr, sizeof(hdr), EEPROM_SPI_TIMEOUT_MS);
    if (st == HAL_OK) {
        st = HAL_SPI_Receive(EEPROM_SPI_HANDLE, data, len, EEPROM_SPI_TIMEOUT_MS);
    }
    Flash_CsHigh();

    return (st == HAL_OK);
}

/* ------------------------------------------------------------------------ */

void App_Eeprom_Init(void)
{
    s_eepromMutex = osMutexNew(NULL);
    Flash_CsHigh(); /* deselect */
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
    if (Flash_ReadData(EEPROM_CONFIG_BASE_ADDR, (uint8_t *)&tmp, sizeof(tmp))) {
        size_t crc_field_offset = offsetof(AppConfig_t, crc32);
        uint32_t crc = Crc32_Compute((const uint8_t *)&tmp, crc_field_offset);
        if (tmp.magic == CONFIG_MAGIC && crc == tmp.crc32) {
            /* make sure strings are NUL-terminated regardless of flash content */
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
    /* NOR flash: bits can only be cleared by Page Program, so the sector
     * must be erased (all bits -> 1) before every save. */
    ok = Flash_SectorErase(EEPROM_CONFIG_BASE_ADDR);
    if (ok) {
        ok = Flash_PageProgram(EEPROM_CONFIG_BASE_ADDR, (const uint8_t *)cfg, sizeof(*cfg));
    }
    osMutexRelease(s_eepromMutex);

    return ok;
}
