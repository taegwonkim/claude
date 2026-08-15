/**
 * app_config.c — 설정 저장/로드 (W25Q40CLS 핑퐁 섹터 + CRC32) 구현
 */
#include "app_config.h"
#include "w25qxx.h"
#include "main.h"
#include <string.h>
#include <stddef.h>

#define CFG_MAGIC     0x53474454u /* "SGDT" */
#define CFG_VERSION   1u

osEventFlagsId_t egSysStatus;
osMutexId_t      mutexFlash;
SystemConfig_t   g_sysConfig;

extern SPI_HandleTypeDef hspi2; /* CubeMX 생성 spi.c 에서 정의 */

/* --------------------------- CRC32 (poly 0xEDB88320, 반사) --------------------------- */
static uint32_t Crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

static uint32_t ConfigCrc(const SystemConfig_t *cfg)
{
    /* crc32 필드를 제외한 앞부분에 대해서만 계산 */
    return Crc32((const uint8_t *)cfg, offsetof(SystemConfig_t, crc32));
}

void App_Config_LoadDefaults(SystemConfig_t *out)
{
    memset(out, 0, sizeof(*out));
    out->magic = CFG_MAGIC;
    out->version = CFG_VERSION;
    out->seqCounter = 0;

    strncpy(out->ssid, "SurgeDetector-AP", CFG_SSID_MAXLEN - 1);
    strncpy(out->password, "changeme", CFG_PASSWORD_MAXLEN - 1);

    out->serverIp[0] = 192; out->serverIp[1] = 168; out->serverIp[2] = 0; out->serverIp[3] = 10;
    out->serverPort = 50001;

    out->dhcpEnable = 1;
    memset(out->moduleIp, 0, sizeof(out->moduleIp));
    memset(out->gateway, 0, sizeof(out->gateway));
    memset(out->netmask, 0, sizeof(out->netmask));

    out->crc32 = ConfigCrc(out);
}

/* 지정 섹터에서 설정을 읽고 magic/crc가 유효하면 true */
static bool ReadSector(uint32_t sectorAddr, SystemConfig_t *out)
{
    if (!W25Qxx_Read(&g_flashDev, sectorAddr, (uint8_t *)out, sizeof(*out)))
    {
        return false;
    }

    if (out->magic != CFG_MAGIC)
    {
        return false;
    }

    return ConfigCrc(out) == out->crc32;
}

static bool WriteSector(uint32_t sectorAddr, const SystemConfig_t *cfg)
{
    if (!W25Qxx_SectorErase(&g_flashDev, sectorAddr))
    {
        return false;
    }
    return W25Qxx_Write(&g_flashDev, sectorAddr, (const uint8_t *)cfg, sizeof(*cfg));
}

void App_Config_Init(void)
{
    const osMutexAttr_t mutexAttr = { .name = "mutexFlash" };
    mutexFlash = osMutexNew(&mutexAttr);

    egSysStatus = osEventFlagsNew(NULL);

    /* CS: PC6(FLASH_CS) 는 CubeMX가 GPIO Output으로 생성 — 매크로는 main.h 참조 */
    W25Qxx_Init(&g_flashDev, &hspi2, FLASH_CS_GPIO_Port, FLASH_CS_Pin);

    osMutexAcquire(mutexFlash, osWaitForever);

    uint32_t jedec = W25Qxx_ReadJedecId(&g_flashDev);
    (void)jedec; /* 필요 시 W25Q_JEDEC_ID_W25Q40 와 비교해 실장 확인 */
    osEventFlagsSet(egSysStatus, EVT_FLASH_READY);

    SystemConfig_t a, b;
    bool aValid = ReadSector(W25Q_CFG_SECTOR_A_ADDR, &a);
    bool bValid = ReadSector(W25Q_CFG_SECTOR_B_ADDR, &b);

    if (aValid && (!bValid || a.seqCounter >= b.seqCounter))
    {
        g_sysConfig = a;
    }
    else if (bValid)
    {
        g_sysConfig = b;
    }
    else
    {
        /* 둘 다 무효 -> 공장 초기값 생성 후 섹터 A에 기록 */
        App_Config_LoadDefaults(&g_sysConfig);
        WriteSector(W25Q_CFG_SECTOR_A_ADDR, &g_sysConfig);
    }

    osMutexRelease(mutexFlash);

    osEventFlagsSet(egSysStatus, EVT_CONFIG_LOADED);
}

void App_Config_GetCopy(SystemConfig_t *out)
{
    osMutexAcquire(mutexFlash, osWaitForever);
    *out = g_sysConfig;
    osMutexRelease(mutexFlash);
}

bool App_Config_Save(const SystemConfig_t *newCfg)
{
    bool ok;

    osMutexAcquire(mutexFlash, osWaitForever);

    SystemConfig_t toWrite = *newCfg;
    toWrite.magic = CFG_MAGIC;
    toWrite.version = CFG_VERSION;
    toWrite.seqCounter = g_sysConfig.seqCounter + 1u;
    toWrite.crc32 = ConfigCrc(&toWrite);

    /* 짝수 seq -> 섹터A, 홀수 seq -> 섹터B (핑퐁): 쓰는 동안 전원이 나가도
     * 반대편 섹터(직전 유효본)가 남아있어 복구 가능 */
    uint32_t targetAddr = (toWrite.seqCounter & 1u) ? W25Q_CFG_SECTOR_B_ADDR : W25Q_CFG_SECTOR_A_ADDR;

    ok = WriteSector(targetAddr, &toWrite);
    if (ok)
    {
        g_sysConfig = toWrite;
    }

    osMutexRelease(mutexFlash);
    return ok;
}
