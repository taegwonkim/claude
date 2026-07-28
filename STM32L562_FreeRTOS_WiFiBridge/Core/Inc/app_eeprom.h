/**
 * app_eeprom.h
 *
 * Wireless/server configuration storage in external SPI NOR flash
 * (Winbond W25Q40CL). Parameters are received from the PC over the
 * config UART (see app_pc_uart.c) and persisted here so they survive
 * power cycles.
 */
#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_MAGIC   (0x53544C35UL) /* 'STL5' */

typedef struct {
    uint32_t magic;                          /* validity marker            */
    char     ap_ssid[WIFI_SSID_MAX_LEN + 1];
    char     ap_pass[WIFI_PASS_MAX_LEN + 1];
    uint8_t  dhcp_enable;                    /* 1 = DHCP, 0 = static IP    */
    char     static_ip[IP_STR_MAX_LEN];
    char     static_gw[IP_STR_MAX_LEN];
    char     static_mask[IP_STR_MAX_LEN];
    char     server_ip[IP_STR_MAX_LEN];
    uint16_t server_port;
    uint32_t crc32;                          /* CRC of all fields above    */
} AppConfig_t;

/**
 * Initialize the EEPROM module. Must be called once before any other
 * App_Eeprom_* function (typically from App_Init()).
 */
void App_Eeprom_Init(void);

/**
 * Load configuration from EEPROM into *out_cfg.
 * Returns true if a valid (magic + CRC OK) configuration was found,
 * false if the EEPROM was blank/corrupt - in that case *out_cfg is
 * filled with sane defaults (empty SSID, DHCP enabled).
 */
bool App_Eeprom_LoadConfig(AppConfig_t *out_cfg);

/**
 * Persist *cfg to EEPROM. Fills in magic/crc32 fields internally.
 * Returns true on success.
 */
bool App_Eeprom_SaveConfig(AppConfig_t *cfg);

/** Fill *cfg with factory defaults (empty SSID/PASS, DHCP on). */
void App_Eeprom_SetDefaults(AppConfig_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* APP_EEPROM_H */
