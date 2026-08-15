/**
 * app_config.h — 시스템 설정 저장/로드 (W25Q40CLS 외부 Flash) + 전역 상태 EventGroup
 */
#ifndef APP_INC_APP_CONFIG_H_
#define APP_INC_APP_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------- 시스템 상태 EventGroup 비트 (README 3.8.5) ---------------------- */
#define EVT_FLASH_READY       (1UL << 0)
#define EVT_CONFIG_LOADED     (1UL << 1)
#define EVT_WIFI_JOINED       (1UL << 2)
#define EVT_SERVER_CONNECTED  (1UL << 3)
#define EVT_USB_ENUMERATED    (1UL << 4)

extern osEventFlagsId_t egSysStatus;

/* -------- 설정 구조체 ------------------------------------------------------ */
#define CFG_SSID_MAXLEN      32u
#define CFG_PASSWORD_MAXLEN  64u

typedef struct
{
    uint32_t magic;         /* 'S''G''D''T' */
    uint32_t version;       /* 저장 포맷 버전 */
    uint32_t seqCounter;    /* 핑퐁 섹터 중 최신 판별용 증가 카운터 */

    char     ssid[CFG_SSID_MAXLEN];
    char     password[CFG_PASSWORD_MAXLEN];

    uint8_t  serverIp[4];
    uint16_t serverPort;

    uint8_t  dhcpEnable;     /* 1: DHCP, 0: 고정 IP(아래 필드 사용) */
    uint8_t  moduleIp[4];
    uint8_t  gateway[4];
    uint8_t  netmask[4];

    uint32_t crc32;          /* magic..netmask 전체에 대한 CRC32 (crc32 필드 제외) */
} SystemConfig_t;

/* 전역 설정 캐시 (mutexFlash로 보호되는 것은 Flash IO뿐이며, 이 캐시 자체 읽기는
 * 짧은 구조체 복사이므로 app_config_get_copy()를 통해서만 접근할 것) */
extern SystemConfig_t g_sysConfig;
extern osMutexId_t    mutexFlash;

/* 모듈 초기화: mutexFlash/egSysStatus 생성 + Flash에서 설정 로드(없으면 기본값 저장) */
void App_Config_Init(void);

/* 스레드-세이프 getter: 호출자 버퍼로 복사본 반환 */
void App_Config_GetCopy(SystemConfig_t *out);

/* 설정 갱신 + Flash 반영(핑퐁 섹터, CRC32 포함). 성공 시 true */
bool App_Config_Save(const SystemConfig_t *newCfg);

/* 기본값(공장 초기화) 반환 */
void App_Config_LoadDefaults(SystemConfig_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_INC_APP_CONFIG_H_ */
