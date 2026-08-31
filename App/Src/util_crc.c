#include "util_crc.h"

uint8_t sd_crc8(const uint8_t *data, size_t len)
{
  uint8_t crc = 0x00u;

  while (len--)
  {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8u; i++)
    {
      crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

uint32_t sd_crc32(const uint8_t *data, size_t len)
{
  uint32_t crc = 0xFFFFFFFFu;

  while (len--)
  {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8u; i++)
    {
      crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFu;
}
