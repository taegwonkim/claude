/**
 ******************************************************************************
 * @file    dma_sram2_example.c
 * @brief   STM32H5 SRAM2 + GPDMA 사용 예제
 *
 * 이 예제는 다음을 시연합니다:
 * - SRAM2에 DMA 버퍼 배치 (캐시 일관성 문제 해결)
 * - STM32H5 GPDMA(General Purpose DMA)를 이용한 메모리-메모리 전송
 * - MPU를 이용한 SRAM2 Non-Cacheable 설정
 * - 캐시 관리 함수를 이용한 SRAM1 DMA 사용
 *
 * GPDMA 특징 (STM32H5):
 * - 이전 DMA/MDMA 대체
 * - 16개 채널 (GPDMA1: 8채널, GPDMA2: 8채널)
 * - 링크드 리스트(Linked List) 지원
 * - 2D 어드레싱 지원
 ******************************************************************************
 */

#include "dma_sram2_example.h"
#include "ram_config.h"
#include "stm32h5xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ============================================================
 * SRAM2에 배치된 DMA 버퍼 (Non-Cacheable 영역)
 *
 * MPU로 SRAM2를 Non-Cacheable 설정하면 캐시 플러시/무효화
 * 작업 없이 DMA를 안전하게 사용할 수 있습니다.
 * ============================================================ */

/** UART DMA 수신 버퍼 (Non-Cacheable SRAM2) */
PLACE_IN_SRAM2_BSS CACHE_ALIGNED
uint8_t uartRxDmaBuf[UART_DMA_BUF_SIZE];

/** UART DMA 송신 버퍼 (Non-Cacheable SRAM2) */
PLACE_IN_SRAM2_BSS CACHE_ALIGNED
uint8_t uartTxDmaBuf[UART_DMA_BUF_SIZE];

/** SPI DMA 수신 버퍼 */
PLACE_IN_SRAM2_BSS CACHE_ALIGNED
uint8_t spiRxDmaBuf[SPI_DMA_BUF_SIZE];

/** SPI DMA 송신 버퍼 */
PLACE_IN_SRAM2_BSS CACHE_ALIGNED
uint8_t spiTxDmaBuf[SPI_DMA_BUF_SIZE];

/** 메모리-메모리 DMA 소스 버퍼 */
PLACE_IN_SRAM2_BSS CACHE_ALIGNED
uint32_t memDmaSrcBuf[MEM_DMA_BUF_WORDS];

/** 메모리-메모리 DMA 목적지 버퍼 */
PLACE_IN_SRAM2_BSS CACHE_ALIGNED
uint32_t memDmaDstBuf[MEM_DMA_BUF_WORDS];

/* ============================================================
 * SRAM1에 배치된 일반 작업 버퍼 (Cached - 캐시 관리 필요)
 * ============================================================ */

/** 처리된 데이터 버퍼 (SRAM1, CPU 빠른 접근용) */
static uint8_t processBuf[UART_DMA_BUF_SIZE];

/* ============================================================
 * DMA 핸들
 * ============================================================ */
static DMA_HandleTypeDef hdma_memtomem;

/* ============================================================
 * 내부 변수
 * ============================================================ */
static volatile uint8_t  dmaTransferComplete = 0;
static volatile uint32_t dmaErrorCode = 0;

/* ============================================================
 * MPU 설정 함수
 * ============================================================ */

/**
 * @brief MPU를 사용하여 SRAM2를 Non-Cacheable로 설정
 *
 * DMA 버퍼가 있는 SRAM2 영역의 캐시를 비활성화합니다.
 * 이렇게 하면 DMA 전송 전후에 캐시 관리가 필요 없습니다.
 *
 * @note SystemClock_Config() 이후, 첫 번째 DMA 전송 이전에 호출
 */
void MPU_ConfigSRAM2NonCacheable(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    /* MPU 비활성화 (설정 변경 전) */
    HAL_MPU_Disable();

    /*
     * Region 0: SRAM2 전체 - Non-Cacheable, Non-Bufferable
     *
     * 설정 의미:
     * - IsBufferable = MPU_ACCESS_NOT_BUFFERABLE: 버퍼링 비활성화
     * - IsCacheable  = MPU_ACCESS_NOT_CACHEABLE:  캐시 비활성화
     * - IsShareable  = MPU_ACCESS_SHAREABLE:       DMA 접근 허용
     * - Number       = MPU_REGION_NUMBER0:         Region 번호 (0~15)
     * - Size         = MPU_REGION_SIZE_64KB:       SRAM2 크기
     * - SubRegionDisable = 0x00:                   모든 서브리전 활성화
     */
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress      = SRAM2_BASE_ADDR;
    MPU_InitStruct.Size             = MPU_REGION_SIZE_64KB;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /*
     * Region 1: SRAM1 전체 - Write-Back, Write-Allocate (기본 캐시 설정)
     *
     * SRAM1은 CPU 전용 접근이 많으므로 캐시를 활성화합니다.
     * DMA와 함께 사용하는 SRAM1 버퍼는 캐시 관리 함수를 사용하세요.
     */
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress      = SRAM1_BASE_ADDR;
    MPU_InitStruct.Size             = MPU_REGION_SIZE_256KB;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;  /* Write-Back */
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_BUFFERABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /* MPU 활성화 (기본 맵 활성화, 권한 오류 시 HardFault) */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

    printf("[MPU] SRAM2 Non-Cacheable 설정 완료\r\n");
    printf("[MPU] SRAM2 주소: 0x%08lX, 크기: 64KB\r\n", (uint32_t)SRAM2_BASE_ADDR);
}

/* ============================================================
 * GPDMA 메모리-메모리 전송 예제
 * ============================================================ */

/**
 * @brief GPDMA 메모리-메모리 전송 초기화
 *
 * GPDMA1 채널 0을 메모리-메모리 전송으로 설정합니다.
 */
HAL_StatusTypeDef DMA_MemToMem_Init(void)
{
    /* GPDMA1 클럭 활성화 */
    __HAL_RCC_GPDMA1_CLK_ENABLE();

    hdma_memtomem.Instance = GPDMA1_Channel0;

    hdma_memtomem.Init.Request             = DMA_REQUEST_SW;          /* 소프트웨어 트리거 */
    hdma_memtomem.Init.BlkHWRequest        = DMA_BREQ_SINGLE_BURST;
    hdma_memtomem.Init.Direction           = DMA_MEMORY_TO_MEMORY;
    hdma_memtomem.Init.SrcInc              = DMA_SINC_INCREMENTED;    /* 소스 주소 증가 */
    hdma_memtomem.Init.DestInc             = DMA_DINC_INCREMENTED;    /* 목적지 주소 증가 */
    hdma_memtomem.Init.SrcDataWidth        = DMA_SRC_DATAWIDTH_WORD;  /* 32비트 단위 */
    hdma_memtomem.Init.DestDataWidth       = DMA_DEST_DATAWIDTH_WORD;
    hdma_memtomem.Init.Priority            = DMA_LOW_PRIORITY_LOW_WEIGHT;
    hdma_memtomem.Init.SrcBurstLength      = 1;
    hdma_memtomem.Init.DestBurstLength     = 1;
    hdma_memtomem.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 |
                                               DMA_DEST_ALLOCATED_PORT1;
    hdma_memtomem.Init.TransferEventMode   = DMA_TCEM_BLOCK_TRANSFER;
    hdma_memtomem.Init.Mode                = DMA_NORMAL;

    HAL_StatusTypeDef ret = HAL_DMA_Init(&hdma_memtomem);
    if (ret != HAL_OK)
    {
        printf("[DMA] 초기화 실패: %d\r\n", ret);
        return ret;
    }

    /* NVIC 인터럽트 설정 */
    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);

    printf("[DMA] GPDMA1 채널 0 초기화 완료 (메모리-메모리)\r\n");
    return HAL_OK;
}

/**
 * @brief SRAM2 내 메모리-메모리 DMA 전송 실행
 *
 * SRAM2 버퍼는 Non-Cacheable이므로 캐시 관리 없이 바로 사용 가능합니다.
 *
 * @param srcBuf   소스 버퍼 (SRAM2 내)
 * @param dstBuf   목적지 버퍼 (SRAM2 내)
 * @param numWords 전송할 워드(32비트) 수
 * @retval HAL_OK 성공
 */
HAL_StatusTypeDef DMA_MemToMem_Transfer_SRAM2(
    uint32_t *srcBuf,
    uint32_t *dstBuf,
    uint32_t  numWords)
{
    /* SRAM2 영역 확인 */
    if (!RAM_IsInSRAM2(srcBuf) || !RAM_IsInSRAM2(dstBuf))
    {
        printf("[DMA] 경고: 버퍼가 SRAM2 영역에 없습니다!\r\n");
        printf("[DMA] src=0x%08lX, dst=0x%08lX\r\n",
               (uint32_t)srcBuf, (uint32_t)dstBuf);
    }

    dmaTransferComplete = 0;
    dmaErrorCode = 0;

    /* 콜백 등록 */
    hdma_memtomem.XferCpltCallback  = DMA_TransferComplete_Callback;
    hdma_memtomem.XferErrorCallback = DMA_TransferError_Callback;

    /* DMA 전송 시작 (바이트 단위: numWords × 4) */
    HAL_StatusTypeDef ret = HAL_DMA_Start_IT(
        &hdma_memtomem,
        (uint32_t)srcBuf,
        (uint32_t)dstBuf,
        numWords * 4);

    if (ret != HAL_OK)
    {
        printf("[DMA] 전송 시작 실패: %d\r\n", ret);
        return ret;
    }

    /* 완료 대기 (폴링 방식, 타임아웃 1초) */
    uint32_t tickStart = HAL_GetTick();
    while (!dmaTransferComplete)
    {
        if ((HAL_GetTick() - tickStart) > 1000U)
        {
            printf("[DMA] 타임아웃!\r\n");
            HAL_DMA_Abort(&hdma_memtomem);
            return HAL_TIMEOUT;
        }
    }

    if (dmaErrorCode != 0)
    {
        printf("[DMA] 오류 발생: 0x%08lX\r\n", dmaErrorCode);
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @brief SRAM1 → SRAM1 DMA 전송 (캐시 관리 포함)
 *
 * SRAM1은 Cacheable이므로 DMA 전후에 캐시 관리가 필요합니다.
 *
 * @param srcBuf   소스 버퍼 (SRAM1 내, Cached)
 * @param dstBuf   목적지 버퍼 (SRAM1 내, Cached)
 * @param numBytes 전송할 바이트 수
 */
HAL_StatusTypeDef DMA_MemToMem_Transfer_SRAM1_Cached(
    uint32_t *srcBuf,
    uint32_t *dstBuf,
    uint32_t  numBytes)
{
    /*
     * 캐시 클린: CPU에서 수정한 데이터를 실제 메모리에 기록
     * DMA가 최신 데이터를 읽을 수 있도록 합니다.
     */
    Cache_CleanByAddr(srcBuf, (int32_t)numBytes);

    dmaTransferComplete = 0;

    HAL_StatusTypeDef ret = HAL_DMA_Start_IT(
        &hdma_memtomem,
        (uint32_t)srcBuf,
        (uint32_t)dstBuf,
        numBytes);

    if (ret != HAL_OK) return ret;

    uint32_t tickStart = HAL_GetTick();
    while (!dmaTransferComplete)
    {
        if ((HAL_GetTick() - tickStart) > 1000U)
        {
            HAL_DMA_Abort(&hdma_memtomem);
            return HAL_TIMEOUT;
        }
    }

    /*
     * 캐시 무효화: DMA가 쓴 데이터를 CPU가 읽을 수 있도록 캐시 무효화
     * CPU가 이전 캐시 값이 아닌 DMA가 쓴 새 값을 읽도록 합니다.
     */
    Cache_InvalidateByAddr(dstBuf, (int32_t)numBytes);

    return HAL_OK;
}

/* ============================================================
 * DMA 전송 테스트 함수
 * ============================================================ */

/**
 * @brief SRAM2 DMA 전송 기능 테스트
 *
 * 소스 버퍼에 테스트 패턴을 채우고 DMA로 복사 후 결과를 검증합니다.
 */
void DMA_SRAM2_Test(void)
{
    printf("\r\n===== SRAM2 DMA 전송 테스트 =====\r\n");

    /* 버퍼 주소 출력 */
    printf("소스 버퍼:    0x%08lX (SRAM2: %s)\r\n",
           (uint32_t)memDmaSrcBuf,
           RAM_IsInSRAM2(memDmaSrcBuf) ? "YES" : "NO");
    printf("목적지 버퍼:  0x%08lX (SRAM2: %s)\r\n",
           (uint32_t)memDmaDstBuf,
           RAM_IsInSRAM2(memDmaDstBuf) ? "YES" : "NO");

    /* 소스 버퍼에 테스트 패턴 기록 */
    for (uint32_t i = 0; i < MEM_DMA_BUF_WORDS; i++)
    {
        memDmaSrcBuf[i] = 0xA0000000UL | i;  /* 패턴: 0xA0000000, 0xA0000001, ... */
    }

    /* 목적지 버퍼 초기화 */
    memset(memDmaDstBuf, 0, sizeof(memDmaDstBuf));

    printf("테스트 패턴 작성 완료 (%lu 워드)\r\n", (uint32_t)MEM_DMA_BUF_WORDS);

    /* DMA 전송 실행 */
    HAL_StatusTypeDef result = DMA_MemToMem_Transfer_SRAM2(
        memDmaSrcBuf, memDmaDstBuf, MEM_DMA_BUF_WORDS);

    if (result != HAL_OK)
    {
        printf("DMA 전송 실패! 오류: %d\r\n", result);
        return;
    }

    /* 결과 검증 */
    uint32_t errorCount = 0;
    for (uint32_t i = 0; i < MEM_DMA_BUF_WORDS; i++)
    {
        if (memDmaDstBuf[i] != memDmaSrcBuf[i])
        {
            errorCount++;
            if (errorCount <= 5)  /* 최대 5개 오류만 출력 */
            {
                printf("  불일치[%lu]: 예상=0x%08lX, 실제=0x%08lX\r\n",
                       i, memDmaSrcBuf[i], memDmaDstBuf[i]);
            }
        }
    }

    if (errorCount == 0)
    {
        printf("DMA 전송 검증 성공! (%lu 워드 = %lu 바이트)\r\n",
               (uint32_t)MEM_DMA_BUF_WORDS,
               (uint32_t)(MEM_DMA_BUF_WORDS * 4));
    }
    else
    {
        printf("DMA 전송 오류: %lu개 불일치\r\n", errorCount);
    }

    printf("=================================\r\n");
}

/* ============================================================
 * DMA 인터럽트 핸들러 및 콜백
 * ============================================================ */

/**
 * @brief GPDMA1 채널 0 IRQ 핸들러
 * @note stm32h5xx_it.c 에서 이 함수를 호출하도록 설정
 */
void GPDMA1_Channel0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_memtomem);
}

/**
 * @brief DMA 전송 완료 콜백
 * @param hdma  DMA 핸들 포인터
 */
void DMA_TransferComplete_Callback(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    dmaTransferComplete = 1;
}

/**
 * @brief DMA 전송 오류 콜백
 * @param hdma  DMA 핸들 포인터
 */
void DMA_TransferError_Callback(DMA_HandleTypeDef *hdma)
{
    dmaErrorCode = hdma->ErrorCode;
    dmaTransferComplete = 1;  /* 대기 루프 탈출 */
}
