/**
 ******************************************************************************
 * @file    bkpram_example.c
 * @brief   STM32H5 BKPRAM(배터리 백업 RAM) 사용 예제
 *
 * 이 예제는 다음을 시연합니다:
 * - 전원 차단 후에도 유지되는 데이터 저장
 * - 리셋 카운터 (소프트 리셋 횟수 추적)
 * - 어플리케이션 상태 저장/복원
 * - BKPRAM 데이터 유효성 검사
 *
 * 하드웨어 요구사항:
 * - VBAT 핀에 3V 코인 배터리(CR2032) 또는 슈퍼캐패시터 연결
 * - 배터리 없이도 VDD 공급 중 데이터 유지 (VDD 차단 시 손실)
 ******************************************************************************
 */

#include "bkpram_example.h"
#include "ram_config.h"
#include "stm32h5xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ============================================================
 * BKPRAM에 배치될 애플리케이션 데이터 구조체
 * ============================================================ */

/**
 * @brief 전원 차단 후에도 유지할 애플리케이션 설정
 *
 * PLACE_IN_BKPRAM 매크로로 BKPRAM 섹션에 배치됩니다.
 * 링커 스크립트의 .bkpram_data 섹션에 할당됩니다.
 */
typedef struct {
    /* 유효성 검사 */
    uint32_t magic;                 /**< BKPRAM_MAGIC_VALID = 유효 */
    uint32_t version;               /**< 데이터 구조 버전 */
    uint32_t checksum;              /**< 데이터 무결성 체크섬 */

    /* 시스템 상태 */
    uint32_t reset_count;           /**< 전체 리셋 횟수 */
    uint32_t soft_reset_count;      /**< 소프트 리셋 횟수 */
    uint32_t power_cycle_count;     /**< 전원 ON/OFF 횟수 */
    uint32_t last_reset_reason;     /**< 마지막 리셋 원인 */

    /* 애플리케이션 설정 */
    uint32_t app_mode;              /**< 동작 모드 */
    uint8_t  device_id[16];         /**< 디바이스 고유 ID */
    uint8_t  wifi_ssid[32];         /**< WiFi SSID (예시) */
    uint8_t  reserved[32];          /**< 향후 확장용 예약 공간 */
} AppBackupData_t;

/* BKPRAM에 배치 (전원 차단 후에도 유지) */
PLACE_IN_BKPRAM
static AppBackupData_t appBackup;

/* ============================================================
 * 버전 및 매직 넘버
 * ============================================================ */
#define APP_BACKUP_VERSION      (0x00000001UL)
#define APP_BACKUP_MAGIC        (0xA5A5BEEFUL)

/* ============================================================
 * 리셋 원인 플래그 (RCC_RSR 레지스터 기반)
 * ============================================================ */
typedef enum {
    RESET_REASON_UNKNOWN    = 0x00,
    RESET_REASON_POWER_ON   = 0x01,  /**< 전원 인가 (POR) */
    RESET_REASON_EXTERNAL   = 0x02,  /**< 외부 리셋 핀 */
    RESET_REASON_WATCHDOG   = 0x03,  /**< 워치독 타임아웃 */
    RESET_REASON_SOFTWARE   = 0x04,  /**< 소프트웨어 리셋 (SCB->AIRCR) */
    RESET_REASON_BROWNOUT   = 0x05,  /**< 전압 강하 (BOR) */
} ResetReason_t;

/* ============================================================
 * 내부 함수 프로토타입
 * ============================================================ */
static ResetReason_t GetResetReason(void);
static uint32_t CalcBackupChecksum(void);

/* ============================================================
 * BKPRAM 예제 함수 구현
 * ============================================================ */

/**
 * @brief BKPRAM 초기화 및 데이터 복원
 *
 * 시스템 초기화 과정에서 호출합니다.
 * - BKPRAM 데이터가 유효하면: 저장된 데이터로 상태 복원
 * - BKPRAM 데이터가 무효하면: 기본값으로 초기화 후 저장
 */
void BKPRAM_Example_Init(void)
{
    /* BKPRAM 하드웨어 초기화 */
    BKPRAM_Init();

    /* 리셋 원인 확인 */
    ResetReason_t reason = GetResetReason();

    /* BKPRAM 데이터 유효성 확인 */
    if ((appBackup.magic   == APP_BACKUP_MAGIC) &&
        (appBackup.version == APP_BACKUP_VERSION))
    {
        /* 체크섬 검증 */
        uint32_t stored_checksum = appBackup.checksum;
        appBackup.checksum = 0;
        uint32_t calc_checksum = CalcBackupChecksum();
        appBackup.checksum = stored_checksum;

        if (stored_checksum == calc_checksum)
        {
            /* 유효한 데이터 - 카운터 업데이트 */
            printf("[BKPRAM] 데이터 복원 성공\r\n");
            printf("[BKPRAM] 누적 리셋 횟수: %lu\r\n", appBackup.reset_count);

            appBackup.reset_count++;

            if (reason == RESET_REASON_SOFTWARE)
            {
                appBackup.soft_reset_count++;
                printf("[BKPRAM] 소프트 리셋 감지 (%lu회)\r\n",
                       appBackup.soft_reset_count);
            }
            else if (reason == RESET_REASON_POWER_ON)
            {
                appBackup.power_cycle_count++;
                printf("[BKPRAM] 전원 ON 감지 (%lu회)\r\n",
                       appBackup.power_cycle_count);
            }
        }
        else
        {
            /* 체크섬 불일치 - 데이터 손상 */
            printf("[BKPRAM] 체크섬 오류! 데이터 재초기화\r\n");
            BKPRAM_Example_Reset();
        }
    }
    else
    {
        /* 첫 부팅 또는 배터리 방전 후 */
        printf("[BKPRAM] 최초 초기화 또는 배터리 방전\r\n");
        BKPRAM_Example_Reset();
    }

    appBackup.last_reset_reason = (uint32_t)reason;

    /* 변경된 데이터 저장 */
    BKPRAM_Example_Save();
}

/**
 * @brief BKPRAM 데이터를 기본값으로 초기화
 */
void BKPRAM_Example_Reset(void)
{
    memset(&appBackup, 0, sizeof(appBackup));

    appBackup.magic         = APP_BACKUP_MAGIC;
    appBackup.version       = APP_BACKUP_VERSION;
    appBackup.reset_count   = 1;
    appBackup.app_mode      = 0;  /* 기본 모드 */

    /* 기본 디바이스 ID 설정 (실제로는 STM32 UID 레지스터에서 읽음) */
    uint32_t uid[3];
    uid[0] = HAL_GetUIDw0();
    uid[1] = HAL_GetUIDw1();
    uid[2] = HAL_GetUIDw2();
    memcpy(appBackup.device_id, uid, sizeof(uid));

    printf("[BKPRAM] 기본값으로 초기화 완료\r\n");
}

/**
 * @brief BKPRAM 데이터 저장 (체크섬 갱신)
 */
void BKPRAM_Example_Save(void)
{
    /* 체크섬 계산 전 필드를 0으로 */
    appBackup.checksum = 0;
    appBackup.checksum = CalcBackupChecksum();

    printf("[BKPRAM] 저장 완료 (체크섬: 0x%08lX)\r\n",
           appBackup.checksum);
}

/**
 * @brief BKPRAM에서 앱 모드 읽기
 * @return 현재 저장된 앱 모드
 */
uint32_t BKPRAM_Example_GetAppMode(void)
{
    return appBackup.app_mode;
}

/**
 * @brief BKPRAM에 앱 모드 저장
 * @param mode  저장할 앱 모드
 */
void BKPRAM_Example_SetAppMode(uint32_t mode)
{
    appBackup.app_mode = mode;
    BKPRAM_Example_Save();
}

/**
 * @brief BKPRAM 통계 출력
 */
void BKPRAM_Example_PrintStats(void)
{
    printf("\r\n===== BKPRAM 통계 =====\r\n");
    printf("매직:           0x%08lX (%s)\r\n",
           appBackup.magic,
           (appBackup.magic == APP_BACKUP_MAGIC) ? "유효" : "무효");
    printf("데이터 버전:    %lu\r\n", appBackup.version);
    printf("누적 리셋:      %lu회\r\n", appBackup.reset_count);
    printf("소프트 리셋:    %lu회\r\n", appBackup.soft_reset_count);
    printf("전원 ON:        %lu회\r\n", appBackup.power_cycle_count);
    printf("마지막 리셋:    0x%02lX\r\n", appBackup.last_reset_reason);
    printf("앱 모드:        %lu\r\n", appBackup.app_mode);
    printf("디바이스 ID:    ");
    for (int i = 0; i < 12; i++) {
        printf("%02X", appBackup.device_id[i]);
        if (i < 11) printf(":");
    }
    printf("\r\n");
    printf("체크섬:         0x%08lX\r\n", appBackup.checksum);
    printf("=======================\r\n");
}

/* ============================================================
 * 내부 함수 구현
 * ============================================================ */

/**
 * @brief RCC_RSR 레지스터를 읽어 리셋 원인 반환
 */
static ResetReason_t GetResetReason(void)
{
    ResetReason_t reason = RESET_REASON_UNKNOWN;

    uint32_t rsr = RCC->RSR;

    if (rsr & RCC_RSR_PORRSTF)    reason = RESET_REASON_POWER_ON;
    else if (rsr & RCC_RSR_SFTRSTF) reason = RESET_REASON_SOFTWARE;
    else if (rsr & RCC_RSR_IWDGRSTF ||
             rsr & RCC_RSR_WWDGRSTF) reason = RESET_REASON_WATCHDOG;
    else if (rsr & RCC_RSR_PINRSTF)  reason = RESET_REASON_EXTERNAL;
    else if (rsr & RCC_RSR_BORRSTF)  reason = RESET_REASON_BROWNOUT;

    /* 리셋 플래그 클리어 */
    __HAL_RCC_CLEAR_RESET_FLAGS();

    return reason;
}

/**
 * @brief appBackup 구조체 전체에 대한 체크섬 계산
 * @note  checksum 필드는 0으로 설정 후 계산해야 합니다
 */
static uint32_t CalcBackupChecksum(void)
{
    return BKPRAM_CalculateChecksum(
        (const uint8_t *)&appBackup,
        sizeof(appBackup));
}
