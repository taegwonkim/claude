#ifndef UTIL_CRC_H
#define UTIL_CRC_H

#include <stdint.h>
#include <stddef.h>

/* CRC-8 : poly 0x07, init 0x00, no reflect, no xorout  (FPGA 프레임 검증용) */
uint8_t  sd_crc8(const uint8_t *data, size_t len);

/* CRC-32 : poly 0x04C11DB7 reflected(0xEDB88320), init 0xFFFFFFFF,
 *          xorout 0xFFFFFFFF  (설정 구조체 무결성 검증용)                */
uint32_t sd_crc32(const uint8_t *data, size_t len);

#endif /* UTIL_CRC_H */
