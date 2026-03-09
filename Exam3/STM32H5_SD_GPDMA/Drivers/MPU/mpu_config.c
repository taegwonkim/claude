/**
 ******************************************************************************
 * @file    mpu_config.c
 * @brief   MPU Non-Cacheable 영역 설정 구현
 ******************************************************************************
 */
#include "mpu_config.h"

void MPU_ConfigDMARegion(void)
{
    /* ── 1. 기존 MPU 및 캐시 일시 비활성화 ──────────────────────────── */
    HAL_MPU_Disable();

    /* ── 2. DMA 버퍼 영역 (SRAM2 0x20030000, 8 KB) 설정 ────────────── */
    MPU_Region_InitTypeDef region = {0};

    region.Enable           = MPU_REGION_ENABLE;
    region.Number           = MPU_REGION_NUMBER0;
    region.BaseAddress      = DMA_BUF_BASE;           /* 0x20030000     */
    region.Size             = MPU_REGION_SIZE_8KB;
    region.SubRegionDisable = 0x00U;                  /* 전체 8 서브영역 활성 */

    /* Normal Memory, Non-Cacheable, Non-Bufferable:
     *   TEX=1, C=0, B=0  →  Outer/Inner = Non-Cacheable
     *   S=1               →  Shareable (DMA 에서 접근 가능)  */
    region.TypeExtField     = MPU_TEX_LEVEL1;
    region.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    region.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    region.IsShareable      = MPU_ACCESS_SHAREABLE;

    /* 읽기/쓰기 허용, 코드 실행 금지 */
    region.AccessPermission = MPU_REGION_FULL_ACCESS;
    region.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;

    HAL_MPU_ConfigRegion(&region);

    /* ── 3. MPU 재활성화 (배경 영역 포함) ──────────────────────────── */
    /*
     * MPU_HFNMI_PRIVDEF_NONE:
     *   HardFault / NMI 핸들러도 MPU 적용
     *   권한 없는 영역은 기본 맵 사용하지 않음
     *
     * 실제 프로젝트에서는 프로세서 전체 기본 메모리 맵을
     * MPU 배경 영역(PrivilegedDefault)으로 허용하는 경우가 많음:
     *   HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT)
     * 여기서는 최소 설정으로 DMA 영역만 지정.
     */
    HAL_MPU_Enable(MPU_HFNMI_PRIVDEF_NONE);
}
