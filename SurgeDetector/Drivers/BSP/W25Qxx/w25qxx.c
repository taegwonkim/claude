/**
 * w25qxx.c — W25Q40CLS 최소 드라이버 구현
 * 주의: 이 드라이버 함수들은 반드시 mutexFlash(app_config.c)로 보호된 컨텍스트에서만
 * 호출한다 (fpgaTask 등 시간에 민감한 태스크와 동시에 SPI2를 쓰지 않도록).
 */
#include "w25qxx.h"
#include "cmsis_os2.h"
#include <string.h>

W25Qxx_Handle_t g_flashDev;

/* --- Instruction set (표준 W25Qxx 계열) --- */
#define CMD_WRITE_ENABLE    0x06u
#define CMD_READ_STATUS1    0x05u
#define CMD_PAGE_PROGRAM    0x02u
#define CMD_SECTOR_ERASE    0x20u  /* 4KB */
#define CMD_READ_DATA       0x03u
#define CMD_JEDEC_ID        0x9Fu

#define STATUS1_BUSY_BIT    0x01u

static void CS_Low(W25Qxx_Handle_t *dev)  { HAL_GPIO_WritePin(dev->csPort, dev->csPin, GPIO_PIN_RESET); }
static void CS_High(W25Qxx_Handle_t *dev) { HAL_GPIO_WritePin(dev->csPort, dev->csPin, GPIO_PIN_SET); }

static void SendAddr24(W25Qxx_Handle_t *dev, uint32_t addr)
{
    uint8_t buf[3] = { (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };
    HAL_SPI_Transmit(dev->hspi, buf, sizeof(buf), 100);
}

void W25Qxx_Init(W25Qxx_Handle_t *dev, SPI_HandleTypeDef *hspi, GPIO_TypeDef *csPort, uint16_t csPin)
{
    dev->hspi = hspi;
    dev->csPort = csPort;
    dev->csPin = csPin;
    CS_High(dev);
}

uint32_t W25Qxx_ReadJedecId(W25Qxx_Handle_t *dev)
{
    uint8_t cmd = CMD_JEDEC_ID;
    uint8_t id[3] = {0};

    CS_Low(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, 100);
    HAL_SPI_Receive(dev->hspi, id, 3, 100);
    CS_High(dev);

    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
}

bool W25Qxx_WaitBusy(W25Qxx_Handle_t *dev, uint32_t timeoutMs)
{
    uint8_t cmd = CMD_READ_STATUS1;
    uint8_t status = 0;
    uint32_t start = HAL_GetTick();

    do
    {
        CS_Low(dev);
        HAL_SPI_Transmit(dev->hspi, &cmd, 1, 100);
        HAL_SPI_Receive(dev->hspi, &status, 1, 100);
        CS_High(dev);

        if ((status & STATUS1_BUSY_BIT) == 0u)
        {
            return true;
        }
        osDelay(1);
    } while ((HAL_GetTick() - start) < timeoutMs);

    return false;
}

static void WriteEnable(W25Qxx_Handle_t *dev)
{
    uint8_t cmd = CMD_WRITE_ENABLE;
    CS_Low(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, 100);
    CS_High(dev);
}

bool W25Qxx_SectorErase(W25Qxx_Handle_t *dev, uint32_t addr)
{
    uint8_t cmd = CMD_SECTOR_ERASE;

    WriteEnable(dev);

    CS_Low(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, 100);
    SendAddr24(dev, addr);
    CS_High(dev);

    return W25Qxx_WaitBusy(dev, 500);
}

bool W25Qxx_PageProgram(W25Qxx_Handle_t *dev, uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint8_t cmd = CMD_PAGE_PROGRAM;

    if (len == 0u || len > W25Q_PAGE_SIZE)
    {
        return false;
    }

    WriteEnable(dev);

    CS_Low(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, 100);
    SendAddr24(dev, addr);
    HAL_SPI_Transmit(dev->hspi, (uint8_t *)data, len, 200);
    CS_High(dev);

    return W25Qxx_WaitBusy(dev, 100);
}

bool W25Qxx_Write(W25Qxx_Handle_t *dev, uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t written = 0;

    while (written < len)
    {
        uint32_t curAddr = addr + written;
        uint32_t pageOffset = curAddr % W25Q_PAGE_SIZE;
        uint32_t chunk = W25Q_PAGE_SIZE - pageOffset;

        if (chunk > (len - written))
        {
            chunk = len - written;
        }

        if (!W25Qxx_PageProgram(dev, curAddr, &data[written], (uint16_t)chunk))
        {
            return false;
        }

        written += chunk;
    }

    return true;
}

bool W25Qxx_Read(W25Qxx_Handle_t *dev, uint32_t addr, uint8_t *data, uint32_t len)
{
    uint8_t cmd = CMD_READ_DATA;

    CS_Low(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, 100);
    SendAddr24(dev, addr);
    HAL_SPI_Receive(dev->hspi, data, len, HAL_MAX_DELAY);
    CS_High(dev);

    return true;
}
