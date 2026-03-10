/**
 ******************************************************************************
 * @file    ram_config.c
 * @brief   STM32H5 RAM 구성 초기화 및 관리
 *
 * 이 파일은 STM32H563/573의 다중 RAM 영역(SRAM1, SRAM2, BKPRAM)을
 * 초기화하고 관리하는 함수를 구현합니다.
 *
 * 링커 스크립트에서 정의된 심볼을 사용하여 각 섹션을 초기화합니다.
 ******************************************************************************
 */

#include "ram_config.h"
#include "stm32h5xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 링커 스크립트 심볼 외부 선언
 * ============================================================ */

/* .data 섹션 (FLASH → SRAM1 복사) */
extern uint32_t _sidata;    /* FLASH 내 초기값 시작 */
extern uint32_t _sdata;     /* SRAM1 데이터 시작 */
extern uint32_t _edata;     /* SRAM1 데이터 끝 */

/* .bss 섹션 */
extern uint32_t _sbss;      /* BSS 시작 */
extern uint32_t _ebss;      /* BSS 끝 */

/* .sram2_data 섹션 */
extern uint32_t _sisram2;   /* FLASH 내 SRAM2 초기값 시작 */
extern uint32_t _ssram2;    /* SRAM2 데이터 시작 */
extern uint32_t _esram2;    /* SRAM2 데이터 끝 */

/* .sram2_bss 섹션 */
extern uint32_t _ssram2_bss;
extern uint32_t _esram2_bss;

/* .bkpram_data 섹션 */
extern uint32_t _sbkpram;   /* BKPRAM 시작 */
extern uint32_t _ebkpram;   /* BKPRAM 끝 */

/* ============================================================
 * BKPRAM 헤더 (링커가 BKPRAM에 배치)
 * ============================================================ */
PLACE_IN_BKPRAM
static BKPRAM_Header_t bkpram_header;

/* ============================================================
 * RAM 초기화 함수
 * ============================================================ */

/**
 * @brief 전체 RAM 구성 초기화
 *
 * 호출 순서:
 * 1. BKPRAM 초기화 (HAL_Init 이전에 호출 가능)
 * 2. SRAM2 초기화
 *
 * @note Reset_Handler에서 자동으로 .data/.bss가 초기화됩니다.
 *       이 함수는 SRAM2, BKPRAM 등 추가 섹션을 초기화합니다.
 */
void RAM_Config_Init(void)
{
    /* SRAM2 섹션 초기화 */
    SRAM2_Init();

    /* BKPRAM 초기화 (HAL_PWR 이후 호출) */
    BKPRAM_Init();
}

/**
 * @brief SRAM2 섹션 초기화
 *
 * SRAM2 영역의 .sram2_data 섹션을 FLASH에서 복사하고
 * .sram2_bss 섹션을 0으로 초기화합니다.
 *
 * @note startup 파일에서 처리하지 않으므로 명시적 호출 필요
 */
void SRAM2_Init(void)
{
    uint32_t *pSrc, *pDest;

    /* .sram2_data: FLASH → SRAM2 복사 */
    pSrc  = &_sisram2;
    pDest = &_ssram2;

    while (pDest < &_esram2)
    {
        *pDest++ = *pSrc++;
    }

    /* .sram2_bss: 0으로 초기화 */
    pDest = &_ssram2_bss;
    while (pDest < &_esram2_bss)
    {
        *pDest++ = 0UL;
    }
}

/**
 * @brief BKPRAM 초기화
 *
 * BKPRAM 접근을 위한 클럭 및 권한 설정을 수행합니다.
 *
 * @note HAL_Init() 및 SystemClock_Config() 이후 호출 권장
 *       PWR 클럭이 활성화된 상태에서 호출해야 합니다.
 */
void BKPRAM_Init(void)
{
    /* 1. PWR 클럭 활성화 */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* 2. 백업 도메인 쓰기 보호 해제 */
    HAL_PWR_EnableBkUpAccess();

    /* 3. BKPRAM 클럭 활성화 */
    __HAL_RCC_BKPRAM_CLK_ENABLE();

    /*
     * 참고: BKPRAM은 NOLOAD 섹션이므로 부팅 시 초기화되지 않습니다.
     * BKPRAM_IsValid()를 사용하여 데이터 유효성을 확인하고,
     * 유효하지 않은 경우 애플리케이션에서 직접 초기화하세요.
     */
}

/* ============================================================
 * BKPRAM 관리 함수
 * ============================================================ */

/**
 * @brief BKPRAM 데이터 유효성 검사
 *
 * 매직 넘버와 체크섬을 확인하여 BKPRAM 데이터가 유효한지 판단합니다.
 * 최초 전원 인가 또는 VBAT 배터리 방전 후에는 false를 반환합니다.
 *
 * @retval true  데이터 유효
 * @retval false 데이터 무효 (초기화 필요)
 */
bool BKPRAM_IsValid(void)
{
    if (bkpram_header.magic != BKPRAM_MAGIC_VALID)
    {
        return false;
    }

    /* 추가 체크섬 검증 (선택사항) */
    /* uint32_t calc_checksum = BKPRAM_CalculateChecksum(...); */
    /* if (bkpram_header.checksum != calc_checksum) return false; */

    return true;
}

/**
 * @brief BKPRAM 데이터 무효화
 *
 * 매직 넘버를 지워 BKPRAM 데이터를 무효화합니다.
 * 다음 부팅 시 데이터가 재초기화됩니다.
 */
void BKPRAM_Invalidate(void)
{
    bkpram_header.magic = BKPRAM_MAGIC_INVALID;
}

/**
 * @brief 데이터 체크섬 계산 (CRC32 간이 버전)
 *
 * @param data  체크섬을 계산할 데이터 포인터
 * @param len   데이터 길이 (바이트)
 * @return      32비트 체크섬 값
 */
uint32_t BKPRAM_CalculateChecksum(const uint8_t *data, size_t len)
{
    uint32_t checksum = 0xFFFFFFFFUL;

    for (size_t i = 0; i < len; i++)
    {
        checksum ^= (uint32_t)data[i];
        for (int j = 0; j < 8; j++)
        {
            if (checksum & 1U)
            {
                checksum = (checksum >> 1) ^ 0xEDB88320UL;
            }
            else
            {
                checksum >>= 1;
            }
        }
    }

    return checksum ^ 0xFFFFFFFFUL;
}

/* ============================================================
 * 캐시 관리 함수 (DMA와 함께 사용 시 필수)
 * ============================================================ */

/**
 * @brief 특정 주소 범위의 D-Cache 클린 (CPU → 메모리 플러시)
 *
 * DMA TX 전에 호출하여 CPU 캐시의 내용을 실제 메모리에 기록합니다.
 *
 * @param addr  시작 주소 (32바이트 정렬 권장)
 * @param size  크기 (바이트)
 */
void Cache_CleanByAddr(uint32_t *addr, int32_t size)
{
    SCB_CleanDCache_by_Addr(addr, size);
}

/**
 * @brief 특정 주소 범위의 D-Cache 무효화 (메모리 → CPU 갱신)
 *
 * DMA RX 완료 후 호출하여 CPU가 DMA가 쓴 최신 데이터를 읽도록 합니다.
 *
 * @param addr  시작 주소 (32바이트 정렬 권장)
 * @param size  크기 (바이트)
 */
void Cache_InvalidateByAddr(uint32_t *addr, int32_t size)
{
    SCB_InvalidateDCache_by_Addr(addr, size);
}

/**
 * @brief D-Cache 클린 + 무효화
 *
 * @param addr  시작 주소
 * @param size  크기 (바이트)
 */
void Cache_CleanAndInvalidateByAddr(uint32_t *addr, int32_t size)
{
    SCB_CleanInvalidateDCache_by_Addr(addr, size);
}

/* ============================================================
 * 메모리 주소 검증 함수
 * ============================================================ */

/**
 * @brief 주소가 SRAM1 영역 내에 있는지 확인
 * @param addr  확인할 주소
 * @retval true  SRAM1 영역 내
 * @retval false SRAM1 영역 외
 */
bool RAM_IsInSRAM1(const void *addr)
{
    uintptr_t a = (uintptr_t)addr;
    return (a >= SRAM1_BASE_ADDR) && (a < SRAM1_END_ADDR);
}

/**
 * @brief 주소가 SRAM2 영역 내에 있는지 확인
 * @param addr  확인할 주소
 * @retval true  SRAM2 영역 내
 * @retval false SRAM2 영역 외
 */
bool RAM_IsInSRAM2(const void *addr)
{
    uintptr_t a = (uintptr_t)addr;
    return (a >= SRAM2_BASE_ADDR) && (a < SRAM2_END_ADDR);
}

/**
 * @brief 주소가 BKPRAM 영역 내에 있는지 확인
 * @param addr  확인할 주소
 * @retval true  BKPRAM 영역 내
 * @retval false BKPRAM 영역 외
 */
bool RAM_IsInBKPRAM(const void *addr)
{
    uintptr_t a = (uintptr_t)addr;
    return (a >= BKPRAM_BASE_ADDR) && (a < BKPRAM_END_ADDR);
}

/* ============================================================
 * RAM 정보 출력 함수 (디버깅용)
 * ============================================================ */

/**
 * @brief 각 RAM 영역의 사용량 정보를 구조체로 반환
 *
 * @param sram1   SRAM1 정보 출력 포인터 (NULL 가능)
 * @param sram2   SRAM2 정보 출력 포인터 (NULL 가능)
 * @param bkpram  BKPRAM 정보 출력 포인터 (NULL 가능)
 */
void RAM_GetRegionInfo(RAM_RegionInfo_t *sram1, RAM_RegionInfo_t *sram2, RAM_RegionInfo_t *bkpram)
{
    if (sram1 != NULL)
    {
        sram1->base_addr  = SRAM1_BASE_ADDR;
        sram1->end_addr   = SRAM1_END_ADDR;
        sram1->total_size = SRAM1_SIZE_BYTES;
        /* 사용량: .data + .bss 끝 주소 - 시작 주소 */
        sram1->used_size  = (uint32_t)&_ebss - SRAM1_BASE_ADDR;
        sram1->free_size  = SRAM1_SIZE_BYTES - sram1->used_size;
    }

    if (sram2 != NULL)
    {
        sram2->base_addr  = SRAM2_BASE_ADDR;
        sram2->end_addr   = SRAM2_END_ADDR;
        sram2->total_size = SRAM2_SIZE_BYTES;
        sram2->used_size  = (uint32_t)&_esram2 - SRAM2_BASE_ADDR;
        sram2->free_size  = SRAM2_SIZE_BYTES - sram2->used_size;
    }

    if (bkpram != NULL)
    {
        bkpram->base_addr  = BKPRAM_BASE_ADDR;
        bkpram->end_addr   = BKPRAM_END_ADDR;
        bkpram->total_size = BKPRAM_SIZE_BYTES;
        bkpram->used_size  = (uint32_t)&_ebkpram - BKPRAM_BASE_ADDR;
        bkpram->free_size  = BKPRAM_SIZE_BYTES - bkpram->used_size;
    }
}

/**
 * @brief RAM 사용량을 UART(printf)로 출력
 *
 * @note printf 리다이렉션(UART)이 설정된 후 호출해야 합니다.
 */
void RAM_PrintInfo(void)
{
    RAM_RegionInfo_t sram1, sram2, bkpram;
    RAM_GetRegionInfo(&sram1, &sram2, &bkpram);

    printf("\r\n========== STM32H5 RAM 사용량 ==========\r\n");
    printf("SRAM1  (0x%08lX): %5lu / %5lu KB (%.1f%%)\r\n",
           sram1.base_addr,
           sram1.used_size / 1024UL,
           sram1.total_size / 1024UL,
           (float)sram1.used_size / sram1.total_size * 100.0f);

    printf("SRAM2  (0x%08lX): %5lu / %5lu KB (%.1f%%)\r\n",
           sram2.base_addr,
           sram2.used_size / 1024UL,
           sram2.total_size / 1024UL,
           (float)sram2.used_size / sram2.total_size * 100.0f);

    printf("BKPRAM (0x%08lX): %5lu / %5lu Bytes (%.1f%%)\r\n",
           bkpram.base_addr,
           bkpram.used_size,
           bkpram.total_size,
           (float)bkpram.used_size / bkpram.total_size * 100.0f);

    printf("BKPRAM valid: %s\r\n", BKPRAM_IsValid() ? "YES" : "NO");
    printf("========================================\r\n");
}
