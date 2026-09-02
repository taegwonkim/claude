/**
  ******************************************************************************
  * @file    w25q40.c
  ******************************************************************************
  */
#include "w25q40.h"
#include "main.h"

extern SPI_HandleTypeDef hspi2;

/* 명령어 */
#define CMD_WRITE_ENABLE      0x06u
#define CMD_WRITE_DISABLE     0x04u
#define CMD_READ_SR1          0x05u
#define CMD_READ_DATA         0x03u
#define CMD_PAGE_PROGRAM      0x02u
#define CMD_SECTOR_ERASE_4K   0x20u
#define CMD_JEDEC_ID          0x9Fu
#define CMD_RELEASE_PWRDOWN   0xABu

#define SR1_BUSY              0x01u
#define SR1_WEL               0x02u

#define SPI_TMO_MS            100u
#define ERASE_TMO_MS          1000u    /* 4KB sector erase : typ 45ms, max 400ms */
#define PROG_TMO_MS           50u      /* page program : typ 0.7ms, max 3ms      */

/* ---------------------------------------------------------------------------
 * 저수준
 * -------------------------------------------------------------------------*/
static inline void cs_low(void)
{
  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
}

static inline void cs_high(void)
{
  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
}

static sd_res_t spi_tx(const uint8_t *d, uint16_t n)
{
  return (HAL_SPI_Transmit(&hspi2, (uint8_t *)d, n, SPI_TMO_MS) == HAL_OK)
           ? SD_OK : SD_ERR_HW;
}

static sd_res_t spi_rx(uint8_t *d, uint16_t n)
{
  return (HAL_SPI_Receive(&hspi2, d, n, SPI_TMO_MS) == HAL_OK) ? SD_OK : SD_ERR_HW;
}

static sd_res_t write_enable(void)
{
  const uint8_t cmd = CMD_WRITE_ENABLE;
  sd_res_t r;

  cs_low();
  r = spi_tx(&cmd, 1u);
  cs_high();
  return r;
}

static sd_res_t read_sr1(uint8_t *sr)
{
  const uint8_t cmd = CMD_READ_SR1;
  sd_res_t r;

  cs_low();
  r = spi_tx(&cmd, 1u);
  if (r == SD_OK) { r = spi_rx(sr, 1u); }
  cs_high();
  return r;
}

/* BUSY 가 내려갈 때까지 대기. FreeRTOS 태스크 컨텍스트에서만 호출한다. */
static sd_res_t wait_ready(uint32_t timeout_ms)
{
  uint32_t start = osKernelGetTickCount();
  uint8_t  sr;

  for (;;)
  {
    if (read_sr1(&sr) != SD_OK) { return SD_ERR_HW; }
    if ((sr & SR1_BUSY) == 0u)  { return SD_OK;     }

    if ((osKernelGetTickCount() - start) >= timeout_ms)
    {
      return SD_ERR_TMO;
    }
    osDelay(1u);
  }
}

/* ---------------------------------------------------------------------------
 * 공개 API  (호출자가 g_mtxFlash 로 보호한다 — cfg_store.c / cli.c 참조)
 * -------------------------------------------------------------------------*/
sd_res_t w25q_init(void)
{
  const uint8_t cmd = CMD_RELEASE_PWRDOWN;
  uint32_t id = 0u;

  cs_high();

  /* Deep power-down 상태일 수 있으므로 해제 */
  cs_low();
  (void)spi_tx(&cmd, 1u);
  cs_high();
  osDelay(1u);

  if (w25q_read_id(&id) != SD_OK)
  {
    return SD_ERR_HW;
  }

  /* 제조사(EF) + 용량(13 = 4Mbit) 만 확인 : 타입 바이트는 파트별로 다를 수 있음 */
  if ((((id >> 16) & 0xFFu) != SD_W25Q40_MFG_ID) ||
      (( id        & 0xFFu) != SD_W25Q40_CAP_ID))
  {
    return SD_ERR_HW;
  }
  return SD_OK;
}

sd_res_t w25q_read_id(uint32_t *id)
{
  const uint8_t cmd = CMD_JEDEC_ID;
  uint8_t  rx[3] = {0};
  sd_res_t r;

  if (id == NULL) { return SD_ERR_PARAM; }

  cs_low();
  r = spi_tx(&cmd, 1u);
  if (r == SD_OK) { r = spi_rx(rx, 3u); }
  cs_high();

  if (r == SD_OK)
  {
    *id = ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | (uint32_t)rx[2];
  }
  return r;
}

sd_res_t w25q_read(uint32_t addr, uint8_t *dst, uint32_t len)
{
  uint8_t  hdr[4];
  sd_res_t r;

  if ((dst == NULL) || (len == 0u) || ((addr + len) > SD_FLASH_TOTAL_SIZE))
  {
    return SD_ERR_PARAM;
  }

  hdr[0] = CMD_READ_DATA;
  hdr[1] = (uint8_t)(addr >> 16);
  hdr[2] = (uint8_t)(addr >> 8);
  hdr[3] = (uint8_t)(addr);

  cs_low();
  r = spi_tx(hdr, 4u);
  while ((r == SD_OK) && (len > 0u))
  {
    uint16_t chunk = (len > 512u) ? 512u : (uint16_t)len;
    r = spi_rx(dst, chunk);
    dst += chunk;
    len -= chunk;
  }
  cs_high();
  return r;
}

sd_res_t w25q_erase_sector(uint32_t addr)
{
  uint8_t  hdr[4];
  sd_res_t r;

  if (addr >= SD_FLASH_TOTAL_SIZE) { return SD_ERR_PARAM; }

  addr &= ~(SD_FLASH_SECTOR_SIZE - 1u);

  r = wait_ready(ERASE_TMO_MS);
  if (r != SD_OK) { return r; }

  r = write_enable();
  if (r != SD_OK) { return r; }

  hdr[0] = CMD_SECTOR_ERASE_4K;
  hdr[1] = (uint8_t)(addr >> 16);
  hdr[2] = (uint8_t)(addr >> 8);
  hdr[3] = (uint8_t)(addr);

  cs_low();
  r = spi_tx(hdr, 4u);
  cs_high();
  if (r != SD_OK) { return r; }

  return wait_ready(ERASE_TMO_MS);
}

static sd_res_t page_program(uint32_t addr, const uint8_t *src, uint16_t len)
{
  uint8_t  hdr[4];
  sd_res_t r;

  r = wait_ready(PROG_TMO_MS);
  if (r != SD_OK) { return r; }

  r = write_enable();
  if (r != SD_OK) { return r; }

  hdr[0] = CMD_PAGE_PROGRAM;
  hdr[1] = (uint8_t)(addr >> 16);
  hdr[2] = (uint8_t)(addr >> 8);
  hdr[3] = (uint8_t)(addr);

  cs_low();
  r = spi_tx(hdr, 4u);
  if (r == SD_OK) { r = spi_tx(src, len); }
  cs_high();
  if (r != SD_OK) { return r; }

  return wait_ready(PROG_TMO_MS);
}

sd_res_t w25q_write(uint32_t addr, const uint8_t *src, uint32_t len)
{
  if ((src == NULL) || (len == 0u) || ((addr + len) > SD_FLASH_TOTAL_SIZE))
  {
    return SD_ERR_PARAM;
  }

  while (len > 0u)
  {
    /* 페이지 경계를 넘지 않도록 분할 */
    uint32_t page_off = addr % SD_FLASH_PAGE_SIZE;
    uint32_t space    = SD_FLASH_PAGE_SIZE - page_off;
    uint16_t chunk    = (len < space) ? (uint16_t)len : (uint16_t)space;

    sd_res_t r = page_program(addr, src, chunk);
    if (r != SD_OK) { return r; }

    addr += chunk;
    src  += chunk;
    len  -= chunk;
  }
  return SD_OK;
}
