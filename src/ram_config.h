/**
 ******************************************************************************
 * @file    ram_config.h
 * @brief   STM32H5 RAM 구성 설정 헤더
 *
 * 이 헤더 파일은 STM32H5 시리즈의 다중 RAM 영역을 관리하기 위한
 * 매크로, 타입 정의, 함수 선언을 제공합니다.
 *
 * 지원 디바이스:
 *   - STM32H563/573 (256KB SRAM1 + 64KB SRAM2 + 4KB BKPRAM)
 *   - STM32H562     (256KB SRAM1 + 64KB SRAM2 + 4KB BKPRAM)
 *   - STM32H503     (32KB SRAM1 + 2KB BKPRAM)
 ******************************************************************************
 */

#ifndef RAM_CONFIG_H
#define RAM_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 * 메모리 영역 기본 주소 및 크기 매크로
 * ============================================================ */

/* STM32H563/573 기준 */
#define SRAM1_BASE_ADDR     (0x20000000UL)
#define SRAM1_SIZE_BYTES    (256UL * 1024UL)    /* 256 KB */
#define SRAM1_END_ADDR      (SRAM1_BASE_ADDR + SRAM1_SIZE_BYTES)

#define SRAM2_BASE_ADDR     (0x20040000UL)
#define SRAM2_SIZE_BYTES    (64UL * 1024UL)     /* 64 KB */
#define SRAM2_END_ADDR      (SRAM2_BASE_ADDR + SRAM2_SIZE_BYTES)

#define BKPRAM_BASE_ADDR    (0x40003C00UL)
#define BKPRAM_SIZE_BYTES   (4UL * 1024UL)      /* 4 KB */
#define BKPRAM_END_ADDR     (BKPRAM_BASE_ADDR + BKPRAM_SIZE_BYTES)

/* ============================================================
 * 섹션 배치 속성 매크로
 * ============================================================ */

/**
 * @brief SRAM1에 변수 배치
 * @example PLACE_IN_SRAM1 uint32_t myBuffer[256];
 */
#define PLACE_IN_SRAM1 \
    __attribute__((section(".sram1_data")))

/**
 * @brief SRAM2에 변수 배치 (DMA 버퍼, Non-Cacheable 권장)
 * @example PLACE_IN_SRAM2 CACHE_ALIGNED uint8_t dmaBuffer[512];
 */
#define PLACE_IN_SRAM2 \
    __attribute__((section(".sram2_data")))

/**
 * @brief SRAM2 BSS 섹션에 변수 배치 (초기화 없음)
 * @example PLACE_IN_SRAM2_BSS uint8_t rxBuffer[1024];
 */
#define PLACE_IN_SRAM2_BSS \
    __attribute__((section(".sram2_bss")))

/**
 * @brief BKPRAM에 변수 배치 (배터리 백업 RAM)
 * @example PLACE_IN_BKPRAM uint32_t resetCount;
 */
#define PLACE_IN_BKPRAM \
    __attribute__((section(".bkpram_data")))

/**
 * @brief SRAM에서 실행되는 함수 (시간 임계 함수)
 * @example RAMFUNC void criticalISR(void);
 */
#define RAMFUNC \
    __attribute__((section(".RamFunc"), noinline))

/* ============================================================
 * 정렬 매크로
 * ============================================================ */

/** D-Cache 라인 크기 정렬 (32바이트) */
#define CACHE_ALIGNED   __attribute__((aligned(32)))

/** 4바이트 정렬 (DMA 최소 요구사항) */
#define DMA_ALIGNED     __attribute__((aligned(4)))

/** 8바이트 정렬 */
#define ALIGNED_8       __attribute__((aligned(8)))

/* ============================================================
 * BKPRAM 매직 넘버 (데이터 유효성 검사)
 * ============================================================ */
#define BKPRAM_MAGIC_VALID      (0xDEADBEEFUL)
#define BKPRAM_MAGIC_INVALID    (0x00000000UL)

/* ============================================================
 * RAM 영역 정보 구조체
 * ============================================================ */
typedef struct {
    uint32_t base_addr;     /**< 시작 주소 */
    uint32_t end_addr;      /**< 끝 주소 */
    uint32_t total_size;    /**< 전체 크기 (바이트) */
    uint32_t used_size;     /**< 사용 중인 크기 (바이트) */
    uint32_t free_size;     /**< 남은 크기 (바이트) */
} RAM_RegionInfo_t;

/* ============================================================
 * BKPRAM 헤더 구조체 (유효성 검사 포함)
 * ============================================================ */
typedef struct {
    uint32_t magic;         /**< 유효성 검사 매직 값 */
    uint32_t data_version;  /**< 데이터 구조 버전 */
    uint32_t checksum;      /**< 데이터 체크섬 */
    uint32_t reset_count;   /**< 리셋 카운터 */
    uint32_t timestamp;     /**< 마지막 저장 타임스탬프 */
} BKPRAM_Header_t;

/* ============================================================
 * 함수 선언
 * ============================================================ */

/* 초기화 */
void RAM_Config_Init(void);
void BKPRAM_Init(void);
void SRAM2_Init(void);

/* SRAM 영역 정보 */
void RAM_GetRegionInfo(RAM_RegionInfo_t *sram1, RAM_RegionInfo_t *sram2, RAM_RegionInfo_t *bkpram);
void RAM_PrintInfo(void);

/* BKPRAM 관리 */
bool BKPRAM_IsValid(void);
void BKPRAM_Invalidate(void);
uint32_t BKPRAM_CalculateChecksum(const uint8_t *data, size_t len);

/* 캐시 관리 */
void Cache_CleanByAddr(uint32_t *addr, int32_t size);
void Cache_InvalidateByAddr(uint32_t *addr, int32_t size);
void Cache_CleanAndInvalidateByAddr(uint32_t *addr, int32_t size);

/* 메모리 주소 검증 */
bool RAM_IsInSRAM1(const void *addr);
bool RAM_IsInSRAM2(const void *addr);
bool RAM_IsInBKPRAM(const void *addr);

#endif /* RAM_CONFIG_H */
