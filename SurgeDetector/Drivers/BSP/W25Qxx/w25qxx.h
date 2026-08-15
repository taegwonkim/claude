/**
 * w25qxx.h — W25Q40CLS (4Mbit NOR SPI Flash) 최소 드라이버
 * SPI2 + 소프트웨어 CS(FLASH_CS, PC6) 사용. HAL_SPI 폴링 방식(설정 저장용, 저빈도 접근).
 */
#ifndef DRIVERS_BSP_W25QXX_W25QXX_H_
#define DRIVERS_BSP_W25QXX_W25QXX_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32l5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25Q_PAGE_SIZE     256u
#define W25Q_SECTOR_SIZE   4096u   /* W25Q40: 128 sectors x 4KB = 512KB.. 실제 40=4Mbit=512KB? */
#define W25Q_JEDEC_ID_W25Q40 0xEF3013u

/* 설정 저장용 핑퐁 섹터 오프셋 (섹터0/섹터1) */
#define W25Q_CFG_SECTOR_A_ADDR   0x000000u
#define W25Q_CFG_SECTOR_B_ADDR   0x001000u /* +4KB */

typedef struct
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef       *csPort;
    uint16_t            csPin;
} W25Qxx_Handle_t;

void     W25Qxx_Init(W25Qxx_Handle_t *dev, SPI_HandleTypeDef *hspi, GPIO_TypeDef *csPort, uint16_t csPin);
uint32_t W25Qxx_ReadJedecId(W25Qxx_Handle_t *dev);
bool     W25Qxx_SectorErase(W25Qxx_Handle_t *dev, uint32_t addr);
bool     W25Qxx_PageProgram(W25Qxx_Handle_t *dev, uint32_t addr, const uint8_t *data, uint16_t len);
bool     W25Qxx_Write(W25Qxx_Handle_t *dev, uint32_t addr, const uint8_t *data, uint32_t len);
bool     W25Qxx_Read(W25Qxx_Handle_t *dev, uint32_t addr, uint8_t *data, uint32_t len);
bool     W25Qxx_WaitBusy(W25Qxx_Handle_t *dev, uint32_t timeoutMs);

extern W25Qxx_Handle_t g_flashDev;

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_BSP_W25QXX_W25QXX_H_ */
