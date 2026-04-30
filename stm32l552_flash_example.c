#include "stm32l552_flash.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    char     device_name[16];
    uint32_t sensor_calibration[4];
    uint32_t checksum;
} __attribute__((packed)) UserConfig_t;

#define CONFIG_MAGIC    0xDEADBEEFU
#define CONFIG_VERSION  1U

static uint32_t calc_checksum(const UserConfig_t *cfg)
{
    const uint32_t *p = (const uint32_t *)cfg;
    uint32_t sum = 0;
    /* Checksum covers all fields except the checksum field itself */
    size_t words = (sizeof(UserConfig_t) - sizeof(uint32_t)) / sizeof(uint32_t);
    for (size_t i = 0; i < words; i++)
        sum ^= p[i];
    return sum;
}

static void pad_to_8bytes(uint8_t *buf, size_t *padded_len, const void *data, size_t data_len)
{
    *padded_len = (data_len + 7U) & ~7U;
    memcpy(buf, data, data_len);
    if (*padded_len > data_len)
        memset(buf + data_len, 0xFF, *padded_len - data_len);
}

int example_write_config(void)
{
    UserConfig_t cfg = {
        .magic               = CONFIG_MAGIC,
        .version             = CONFIG_VERSION,
        .device_name         = "STM32L552-Demo",
        .sensor_calibration  = {1000, 2000, 3000, 4000},
    };
    cfg.checksum = calc_checksum(&cfg);

    /* Pad to 8-byte boundary required by flash programming */
    uint8_t  buf[((sizeof(UserConfig_t) + 7U) & ~7U)];
    size_t   padded_len;
    pad_to_8bytes(buf, &padded_len, &cfg, sizeof(cfg));

    flash_status_t status = flash_write_user_data(0, buf, padded_len);
    if (status != FLASH_OK) {
        printf("Flash write failed: %d\n", status);
        return -1;
    }

    printf("Config written successfully (%zu bytes)\n", padded_len);
    return 0;
}

int example_read_config(void)
{
    UserConfig_t cfg;

    flash_status_t status = flash_read_user_data(0, &cfg, sizeof(cfg));
    if (status != FLASH_OK) {
        printf("Flash read failed: %d\n", status);
        return -1;
    }

    if (cfg.magic != CONFIG_MAGIC) {
        printf("Invalid magic: 0x%08lX\n", (unsigned long)cfg.magic);
        return -1;
    }

    uint32_t expected = calc_checksum(&cfg);
    if (cfg.checksum != expected) {
        printf("Checksum mismatch: got 0x%08lX, expected 0x%08lX\n",
               (unsigned long)cfg.checksum, (unsigned long)expected);
        return -1;
    }

    printf("Config loaded:\n");
    printf("  version   : %lu\n",    (unsigned long)cfg.version);
    printf("  device    : %s\n",     cfg.device_name);
    printf("  calib[0]  : %lu\n",    (unsigned long)cfg.sensor_calibration[0]);
    printf("  calib[1]  : %lu\n",    (unsigned long)cfg.sensor_calibration[1]);
    printf("  calib[2]  : %lu\n",    (unsigned long)cfg.sensor_calibration[2]);
    printf("  calib[3]  : %lu\n",    (unsigned long)cfg.sensor_calibration[3]);
    return 0;
}

int main(void)
{
    printf("=== STM32L552 Flash Example ===\n");

    if (example_write_config() != 0)
        return 1;

    if (example_read_config() != 0)
        return 1;

    return 0;
}
