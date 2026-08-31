/**
  ******************************************************************************
  * @file    cfg_store.c
  ******************************************************************************
  */
#include "cfg_store.h"
#include "w25q40.h"
#include "util_crc.h"
#include <stdlib.h>

sd_cfg_t g_cfg;

#define CFG_CRC_LEN   (sizeof(sd_cfg_t) - sizeof(uint32_t))

/* ---------------------------------------------------------------------------
 * 내부
 * -------------------------------------------------------------------------*/
static void safe_copy(char *dst, const char *src, uint16_t dst_sz)
{
  uint16_t n = 0u;

  while ((src[n] != '\0') && (n < (dst_sz - 1u)))
  {
    dst[n] = src[n];
    n++;
  }
  dst[n] = '\0';
}

static bool cfg_is_valid(const sd_cfg_t *c)
{
  if (c->magic != SD_CFG_MAGIC)        { return false; }
  if (c->version != SD_CFG_VERSION)    { return false; }
  if (c->size != sizeof(sd_cfg_t))     { return false; }

  return (sd_crc32((const uint8_t *)c, CFG_CRC_LEN) == c->crc32);
}

static sd_res_t slot_read(uint32_t addr, sd_cfg_t *out)
{
  return w25q_read(addr, (uint8_t *)out, sizeof(sd_cfg_t));
}

static sd_res_t slot_write(uint32_t addr, const sd_cfg_t *in)
{
  sd_res_t r;
  sd_cfg_t verify;

  r = w25q_erase_sector(addr);
  if (r != SD_OK) { return r; }

  r = w25q_write(addr, (const uint8_t *)in, sizeof(sd_cfg_t));
  if (r != SD_OK) { return r; }

  /* 기록 검증 */
  r = slot_read(addr, &verify);
  if (r != SD_OK) { return r; }

  return (memcmp(&verify, in, sizeof(sd_cfg_t)) == 0) ? SD_OK : SD_ERR;
}

/* ---------------------------------------------------------------------------
 * 공개 API
 * -------------------------------------------------------------------------*/
void cfgstore_defaults(sd_cfg_t *c)
{
  memset(c, 0, sizeof(*c));

  c->magic   = SD_CFG_MAGIC;
  c->version = SD_CFG_VERSION;
  c->size    = (uint16_t)sizeof(sd_cfg_t);

  safe_copy(c->ssid,   SD_DEF_SSID,    sizeof(c->ssid));
  safe_copy(c->pass,   SD_DEF_PASS,    sizeof(c->pass));
  safe_copy(c->srv_ip, SD_DEF_SRV_IP,  sizeof(c->srv_ip));
  safe_copy(c->sta_ip, SD_DEF_STA_IP,  sizeof(c->sta_ip));
  safe_copy(c->gw,     SD_DEF_GW,      sizeof(c->gw));
  safe_copy(c->mask,   SD_DEF_MASK,    sizeof(c->mask));

  c->srv_port   = SD_DEF_SRV_PORT;
  c->dhcp       = SD_DEF_DHCP;
  c->sample_ms  = SD_DEF_SAMPLE_MS;
  c->out_uart   = 1u;
  c->out_usb    = 1u;
  c->out_wifi   = 1u;
  c->auto_start = 1u;

  c->crc32 = sd_crc32((const uint8_t *)c, CFG_CRC_LEN);
}

sd_res_t cfgstore_load(void)
{
  sd_cfg_t tmp;
  sd_res_t res = SD_ERR;

  if (osMutexAcquire(g_mtxFlash, 2000u) != osOK) { return SD_ERR_BUSY; }

  if ((slot_read(SD_CFG_SLOT_A_ADDR, &tmp) == SD_OK) && cfg_is_valid(&tmp))
  {
    res = SD_OK;
  }
  else if ((slot_read(SD_CFG_SLOT_B_ADDR, &tmp) == SD_OK) && cfg_is_valid(&tmp))
  {
    res = SD_OK;
  }

  osMutexRelease(g_mtxFlash);

  if (osMutexAcquire(g_mtxCfg, 1000u) != osOK) { return SD_ERR_BUSY; }

  if (res == SD_OK)
  {
    memcpy(&g_cfg, &tmp, sizeof(g_cfg));
  }
  else
  {
    cfgstore_defaults(&g_cfg);       /* 공장 초기값으로 동작은 계속한다 */
  }

  osMutexRelease(g_mtxCfg);
  return res;
}

sd_res_t cfgstore_save(void)
{
  sd_cfg_t snap;
  sd_res_t ra, rb;

  /* CRC 갱신 후 스냅샷 (플래시 기록 중 설정 뮤텍스를 오래 잡지 않기 위해) */
  if (osMutexAcquire(g_mtxCfg, 1000u) != osOK) { return SD_ERR_BUSY; }
  g_cfg.magic   = SD_CFG_MAGIC;
  g_cfg.version = SD_CFG_VERSION;
  g_cfg.size    = (uint16_t)sizeof(sd_cfg_t);
  g_cfg.crc32   = sd_crc32((const uint8_t *)&g_cfg, CFG_CRC_LEN);
  memcpy(&snap, &g_cfg, sizeof(snap));
  osMutexRelease(g_mtxCfg);

  if (osMutexAcquire(g_mtxFlash, 3000u) != osOK) { return SD_ERR_BUSY; }
  ra = slot_write(SD_CFG_SLOT_A_ADDR, &snap);
  rb = slot_write(SD_CFG_SLOT_B_ADDR, &snap);
  osMutexRelease(g_mtxFlash);

  /* 한쪽만 성공해도 다음 부팅에 복구 가능하므로 성공으로 본다 */
  return ((ra == SD_OK) || (rb == SD_OK)) ? SD_OK : SD_ERR;
}

sd_res_t cfgstore_init(void)
{
  sd_res_t r;

  if (osMutexAcquire(g_mtxFlash, 2000u) != osOK) { return SD_ERR_BUSY; }
  r = w25q_init();
  osMutexRelease(g_mtxFlash);

  if (r != SD_OK)
  {
    /* 플래시가 없어도 기본값으로 동작 */
    (void)osMutexAcquire(g_mtxCfg, osWaitForever);
    cfgstore_defaults(&g_cfg);
    osMutexRelease(g_mtxCfg);
    return r;
  }

  return cfgstore_load();
}

bool cfgstore_lock(uint32_t timeout_ms)
{
  return (osMutexAcquire(g_mtxCfg, timeout_ms) == osOK);
}

void cfgstore_unlock(void)
{
  (void)osMutexRelease(g_mtxCfg);
}

bool cfgstore_is_valid_ip(const char *s)
{
  uint8_t dots = 0u;
  uint8_t dig  = 0u;
  uint16_t val = 0u;

  if ((s == NULL) || (*s == '\0')) { return false; }

  for (const char *p = s; ; p++)
  {
    if ((*p >= '0') && (*p <= '9'))
    {
      val = (uint16_t)((val * 10u) + (uint16_t)(*p - '0'));
      dig++;
      if ((dig > 3u) || (val > 255u)) { return false; }
    }
    else if ((*p == '.') || (*p == '\0'))
    {
      if (dig == 0u) { return false; }
      if (*p == '\0') { break; }
      dots++;
      if (dots > 3u) { return false; }
      dig = 0u;
      val = 0u;
    }
    else
    {
      return false;
    }
  }
  return (dots == 3u);
}
