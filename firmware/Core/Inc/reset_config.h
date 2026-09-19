/**
 * reset_config.h
 *
 * RTC Wakeup Timer 주기적 리셋 설정(주기, 초 단위) 구조체 정의 및 W25Q40 플래시 저장/로드.
 * NetConfig(net_config_store.h)와 동일한 매직/버전/CRC32 패턴을 사용하되 별도 섹터에 저장한다
 * (APP_FLASH_RESET_CONFIG_SECTOR_ADDR, docs/프로토콜_명세.md §6).
 */
#ifndef RESET_CONFIG_H
#define RESET_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RESET_CONFIG_MAGIC   (0x52534346UL) /* "RSCF" */
#define RESET_CONFIG_VERSION (1U)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    uint32_t period_sec; /* RTC Wakeup Timer 주기(초). RESET_W_ALL/RESET_R_ALL 값 그대로 */
    uint32_t crc32;      /* magic..period_sec까지의 CRC32 (crc32 필드 자신은 제외) */
} ResetConfig_t;

/**
 * @brief 공장 초기값(APP_RESET_DEFAULT_PERIOD_SEC)으로 in-RAM 구조체를 채운다.
 */
void ResetConfig_SetDefaults(ResetConfig_t *cfg);

/**
 * @brief 플래시에서 설정을 읽고 CRC를 검증한다.
 * @return true: 유효한 설정을 cfg에 채움, false: 저장된 설정 없음/손상됨(cfg는 SetDefaults 상태로 채워짐)
 */
bool ResetConfig_Load(ResetConfig_t *cfg);

/**
 * @brief cfg의 CRC를 재계산해 채운 뒤 플래시 섹터를 지우고 기록한다.
 * @return true: 성공
 */
bool ResetConfig_Save(ResetConfig_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* RESET_CONFIG_H */
