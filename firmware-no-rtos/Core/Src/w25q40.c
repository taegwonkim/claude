#include "w25q40.h"
#include "main.h"   /* EEP_NSS_GPIO_Port / EEP_NSS_Pin (CubeMX, 핀 라벨 "EEP_NSS", PB12) */
#include <string.h>

#define W25Q40_CMD_WRITE_ENABLE   (0x06U)
#define W25Q40_CMD_WRITE_DISABLE  (0x04U)
#define W25Q40_CMD_READ_STATUS1   (0x05U)
#define W25Q40_CMD_PAGE_PROGRAM   (0x02U)
#define W25Q40_CMD_READ_DATA      (0x03U)
#define W25Q40_CMD_SECTOR_ERASE   (0x20U)
#define W25Q40_CMD_JEDEC_ID       (0x9FU)

#define W25Q40_SR1_BUSY_MASK      (0x01U)
#define W25Q40_SPI_TIMEOUT_MS     (100U)
#define W25Q40_ERASE_TIMEOUT_MS   (400U)  /* 4KB sector erase, 데이터시트 typ 45ms / max 400ms */

static void CS_Low(void)  { HAL_GPIO_WritePin(EEP_NSS_GPIO_Port, EEP_NSS_Pin, GPIO_PIN_RESET); }
static void CS_High(void) { HAL_GPIO_WritePin(EEP_NSS_GPIO_Port, EEP_NSS_Pin, GPIO_PIN_SET); }

static bool SPI_TxRx(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
    return HAL_SPI_TransmitReceive(&hspi2, (uint8_t *)tx, rx, len, W25Q40_SPI_TIMEOUT_MS) == HAL_OK;
}

static bool SPI_Tx(const uint8_t *tx, uint32_t len)
{
    return HAL_SPI_Transmit(&hspi2, (uint8_t *)tx, len, W25Q40_SPI_TIMEOUT_MS) == HAL_OK;
}

static void AddrToBytes(uint32_t addr, uint8_t out[3])
{
    out[0] = (uint8_t)(addr >> 16);
    out[1] = (uint8_t)(addr >> 8);
    out[2] = (uint8_t)(addr);
}

static bool ReadStatus1(uint8_t *status)
{
    uint8_t tx[2] = { W25Q40_CMD_READ_STATUS1, 0xFF };
    uint8_t rx[2] = { 0 };
    bool ok;

    CS_Low();
    ok = SPI_TxRx(tx, rx, 2);
    CS_High();

    if (ok) {
        *status = rx[1];
    }
    return ok;
}

/* RTOS가 없으므로 osDelay 대신 HAL_Delay(내부적으로 SysTick 기반 busy-wait, 인터럽트는 그대로
 * 동작)를 사용한다 - sector erase 대기 중에는 이 함수 호출자(App_Run 루프) 전체가 멈춘다는
 * 점에 유의(firmware-no-rtos/README.md "RTOS 미사용의 결과" 참고). */
static bool WaitBusy(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t status = 0;

    do {
        if (!ReadStatus1(&status)) {
            return false;
        }
        if ((status & W25Q40_SR1_BUSY_MASK) == 0U) {
            return true;
        }
        HAL_Delay(1);
    } while ((HAL_GetTick() - start) < timeout_ms);

    return false;
}

static bool WriteEnable(void)
{
    uint8_t cmd = W25Q40_CMD_WRITE_ENABLE;
    bool ok;

    CS_Low();
    ok = SPI_Tx(&cmd, 1);
    CS_High();
    return ok;
}

bool W25Q40_Init(void)
{
    uint8_t tx[4] = { W25Q40_CMD_JEDEC_ID, 0xFF, 0xFF, 0xFF };
    uint8_t rx[4] = { 0 };
    uint32_t id;

    CS_High();

    if (!SPI_TxRx(tx, rx, 4)) {
        return false;
    }

    id = ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
    return id == W25Q40_JEDEC_ID;
}

bool W25Q40_Read(uint32_t addr, uint8_t *data, uint32_t len)
{
    uint8_t header[4];
    bool ok;

    if (len == 0U) {
        return true;
    }

    header[0] = W25Q40_CMD_READ_DATA;
    AddrToBytes(addr, &header[1]);

    CS_Low();
    ok = SPI_Tx(header, sizeof(header));
    if (ok) {
        ok = HAL_SPI_Receive(&hspi2, data, len, W25Q40_SPI_TIMEOUT_MS + len / 10U) == HAL_OK;
    }
    CS_High();
    return ok;
}

bool W25Q40_EraseSector(uint32_t addr)
{
    uint8_t header[4];
    bool ok;
    uint32_t sector_addr = addr & ~(W25Q40_SECTOR_SIZE - 1U);

    if (!WriteEnable()) {
        return false;
    }

    header[0] = W25Q40_CMD_SECTOR_ERASE;
    AddrToBytes(sector_addr, &header[1]);

    CS_Low();
    ok = SPI_Tx(header, sizeof(header));
    CS_High();

    if (!ok) {
        return false;
    }

    return WaitBusy(W25Q40_ERASE_TIMEOUT_MS);
}

bool W25Q40_ProgramPage(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint8_t header[4];
    bool ok;

    if (len == 0U || len > W25Q40_PAGE_SIZE) {
        return false;
    }
    /* 동일 페이지 내 쓰기인지 확인 (페이지 경계를 넘으면 하위 바이트가 랩어라운드되어 데이터 손상됨) */
    if (((addr & (W25Q40_PAGE_SIZE - 1U)) + len) > W25Q40_PAGE_SIZE) {
        return false;
    }

    if (!WriteEnable()) {
        return false;
    }

    header[0] = W25Q40_CMD_PAGE_PROGRAM;
    AddrToBytes(addr, &header[1]);

    CS_Low();
    ok = SPI_Tx(header, sizeof(header));
    if (ok) {
        ok = SPI_Tx(data, len);
    }
    CS_High();

    if (!ok) {
        return false;
    }

    return WaitBusy(W25Q40_SPI_TIMEOUT_MS);
}

bool W25Q40_Write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t offset = 0;

    while (offset < len) {
        uint32_t cur_addr = addr + offset;
        uint32_t page_remain = W25Q40_PAGE_SIZE - (cur_addr & (W25Q40_PAGE_SIZE - 1U));
        uint32_t remain = len - offset;
        uint32_t chunk = (remain < page_remain) ? remain : page_remain;

        if (!W25Q40_ProgramPage(cur_addr, &data[offset], chunk)) {
            return false;
        }
        offset += chunk;
    }
    return true;
}
