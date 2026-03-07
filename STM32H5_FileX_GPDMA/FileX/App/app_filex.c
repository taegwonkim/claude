/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_filex.c
  * @brief   FileX 애플리케이션 구현
  *
  *  RTOS(ThreadX) 없이 standalone FileX를 사용하는 예제.
  *  SD 카드 드라이버(fx_stm32_sd_driver)를 통해 SDMMC1 + GPDMA1로 파일 I/O.
  *
  *  지원 기능:
  *   - SD 미디어 마운트 / 언마운트
  *   - 파일 쓰기 / 읽기 / 검증
  *   - 디렉터리 목록 출력
  *   - CSV 로그 파일 추가 쓰기
  ******************************************************************************
  */
/* USER CODE END Header */

#include "app_filex.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 *  정적 버퍼 및 제어 블록
 *  (RTOS 없이 정적 할당 사용)
 * ============================================================================ */

/* FileX 미디어 제어 블록 */
FX_MEDIA g_sd_media;

/*
 * 미디어 섹터 캐시 버퍼
 * GPDMA Word 전송 정렬 필수 (4바이트 정렬)
 */
static uint8_t media_cache_buf[FX_MEDIA_CACHE_SIZE]
    __attribute__((aligned(4)));

/*
 * 파일 I/O 작업용 버퍼
 * (파일 읽기/쓰기 시 임시 데이터 저장)
 */
static uint8_t io_buf[FX_IO_BUFFER_SIZE]
    __attribute__((aligned(4)));

/* 파일 제어 블록 */
static FX_FILE g_file;

/* ============================================================================
 *  내부 헬퍼
 * ============================================================================ */

static const char *fx_error_str(UINT err)
{
    switch (err)
    {
    case FX_SUCCESS:           return "SUCCESS";
    case FX_MEDIA_NOT_OPEN:    return "MEDIA_NOT_OPEN";
    case FX_NOT_FOUND:         return "NOT_FOUND";
    case FX_NOT_A_FILE:        return "NOT_A_FILE";
    case FX_ACCESS_ERROR:      return "ACCESS_ERROR";
    case FX_FILE_CORRUPT:      return "FILE_CORRUPT";
    case FX_SECTOR_INVALID:    return "SECTOR_INVALID";
    case FX_FAT_READ_ERROR:    return "FAT_READ_ERROR";
    case FX_NO_MORE_SPACE:     return "NO_MORE_SPACE";
    case FX_MEDIA_INVALID:     return "MEDIA_INVALID";
    case FX_IO_ERROR:          return "IO_ERROR";
    case FX_WRITE_PROTECT:     return "WRITE_PROTECT";
    case FX_INVALID_NAME:      return "INVALID_NAME";
    case FX_ALREADY_CREATED:   return "ALREADY_CREATED";
    default:                   return "UNKNOWN";
    }
}

/* ============================================================================
 *  MX_FileX_Init
 *  FileX 초기화 및 SD 미디어 마운트
 * ============================================================================ */
UINT MX_FileX_Init(void)
{
    UINT status;

    /* FileX 커널 초기화 (RTOS 없는 모드에서도 호출 필요) */
    fx_system_initialize();

    printf("[FileX] Opening SD media...\r\n");

    /*
     * fx_media_open():
     *   - media_ptr    : FX_MEDIA 제어 블록
     *   - media_name   : 내부 식별 이름 (문자열)
     *   - media_driver : 드라이버 함수 (fx_stm32_sd_driver)
     *   - driver_info  : 드라이버에 전달되는 추가 정보 (NULL 가능)
     *   - memory_ptr   : 섹터 캐시 버퍼
     *   - memory_size  : 캐시 버퍼 크기 (512 배수 권장)
     */
    status = fx_media_open(
        &g_sd_media,
        "SD_CARD",
        fx_stm32_sd_driver,
        NULL,
        media_cache_buf,
        sizeof(media_cache_buf)
    );

    if (status != FX_SUCCESS)
    {
        printf("[FileX ERROR] fx_media_open failed: %s (0x%02X)\r\n",
               fx_error_str(status), status);

        /*
         * SD 카드가 포맷되지 않은 경우 FX_MEDIA_INVALID 반환.
         * 필요 시 아래 포맷 코드를 활성화:
         */
#if 0
        if (status == FX_MEDIA_INVALID || status == FX_FAT_READ_ERROR)
        {
            printf("[FileX] Formatting SD card as FAT32...\r\n");
            status = fx_media_format(
                &g_sd_media,
                fx_stm32_sd_driver,
                NULL,
                media_cache_buf,
                sizeof(media_cache_buf),
                "SD_CARD",         /* 볼륨 이름 */
                2U,                /* FAT 테이블 수 */
                512U,              /* 루트 디렉터리 항목 수 */
                0U,                /* 히든 섹터 수 */
                0U,                /* 총 섹터 수 (0=자동) */
                FX_SD_SECTOR_SIZE, /* 섹터 크기 */
                1U,                /* 섹터/클러스터 (최소 단위) */
                1U,                /* 헤드 수 */
                1U                 /* 섹터/트랙 */
            );

            if (status != FX_SUCCESS)
            {
                printf("[FileX ERROR] Format failed: %s\r\n",
                       fx_error_str(status));
                return status;
            }

            /* 포맷 후 다시 마운트 */
            status = fx_media_open(
                &g_sd_media,
                "SD_CARD",
                fx_stm32_sd_driver,
                NULL,
                media_cache_buf,
                sizeof(media_cache_buf)
            );

            if (status != FX_SUCCESS)
            {
                printf("[FileX ERROR] Re-open after format failed\r\n");
                return status;
            }
        }
#endif
        return status;
    }

    /* 미디어 정보 출력 */
    printf("[FileX] SD media mounted OK\r\n");
    printf("  - Sector size  : %u bytes\r\n",
           (uint32_t)g_sd_media.fx_media_bytes_per_sector);
    printf("  - Total sectors: %lu\r\n",
           g_sd_media.fx_media_total_sectors);
    printf("  - Available    : %lu KB\r\n",
           (g_sd_media.fx_media_available_clusters *
            g_sd_media.fx_media_sectors_per_cluster *
            g_sd_media.fx_media_bytes_per_sector) / 1024UL);

    return FX_SUCCESS;
}

/* ============================================================================
 *  MX_FileX_DeInit
 *  미디어 플러시 + 언마운트
 * ============================================================================ */
void MX_FileX_DeInit(void)
{
    fx_media_flush(&g_sd_media);
    fx_media_close(&g_sd_media);
    printf("[FileX] SD media unmounted.\r\n");
}

/* ============================================================================
 *  FileX_WriteTest
 *  테스트 문자열을 파일에 쓰기
 * ============================================================================ */
UINT FileX_WriteTest(void)
{
    UINT   status;
    ULONG  written = 0U;
    uint32_t tick  = HAL_GetTick();

    /* 쓸 데이터 구성 */
    int len = snprintf((char *)io_buf, sizeof(io_buf),
                       "=== STM32H5 FileX GPDMA Write Test ===\r\n"
                       "Tick: %lu ms\r\n"
                       "GPDMA1: SDMMC1 TX/RX\r\n"
                       "GPDMA2: USART1 TX/RX\r\n"
                       "Hello from FileX (no RTOS)!\r\n",
                       tick);

    if (len <= 0)
    {
        return FX_IO_ERROR;
    }

    printf("[FileX] Writing '%s'...\r\n", FX_TEST_WRITE_FILE);

    /*
     * 파일 열기: 없으면 생성, 있으면 내용 덮어씀
     * FX_OPEN_FOR_WRITE: 쓰기 전용
     * FX_CREATE_FOR_WRITE: 없으면 생성, 있으면 오픈
     */
    status = fx_file_open(&g_sd_media, &g_file,
                          FX_TEST_WRITE_FILE,
                          FX_OPEN_FOR_WRITE);

    if (status == FX_NOT_FOUND)
    {
        /* 파일이 없으면 생성 */
        status = fx_file_create(&g_sd_media, FX_TEST_WRITE_FILE);
        if (status != FX_SUCCESS && status != FX_ALREADY_CREATED)
        {
            printf("[FileX ERROR] Create failed: %s\r\n", fx_error_str(status));
            return status;
        }

        status = fx_file_open(&g_sd_media, &g_file,
                              FX_TEST_WRITE_FILE,
                              FX_OPEN_FOR_WRITE);
    }

    if (status != FX_SUCCESS)
    {
        printf("[FileX ERROR] Open for write failed: %s\r\n",
               fx_error_str(status));
        return status;
    }

    /* 파일 시작으로 이동 (덮어쓰기) */
    status = fx_file_seek(&g_file, 0U);
    if (status != FX_SUCCESS)
    {
        fx_file_close(&g_file);
        return status;
    }

    /* 쓰기 (GPDMA1이 실제 SD 블록 쓰기를 처리) */
    status = fx_file_write(&g_file, io_buf, (ULONG)len);
    if (status != FX_SUCCESS)
    {
        printf("[FileX ERROR] Write failed: %s\r\n", fx_error_str(status));
        fx_file_close(&g_file);
        return status;
    }

    /* 파일 크기 잘라내기 (이전 내용이 더 길 경우) */
    fx_file_truncate(&g_file, (ULONG64)len);

    fx_file_close(&g_file);

    /* 미디어 플러시 (FAT, 디렉터리 항목 SD에 반영) */
    status = fx_media_flush(&g_sd_media);
    if (status != FX_SUCCESS)
    {
        printf("[FileX ERROR] Flush failed: %s\r\n", fx_error_str(status));
        return status;
    }

    printf("[FileX] Write OK: %d bytes, tick=%lu ms\r\n", len, tick);
    return FX_SUCCESS;
}

/* ============================================================================
 *  FileX_ReadTest
 *  파일 읽기 및 내용 출력
 * ============================================================================ */
UINT FileX_ReadTest(void)
{
    UINT  status;
    ULONG actual_read = 0U;

    printf("[FileX] Reading '%s'...\r\n", FX_TEST_READ_FILE);

    status = fx_file_open(&g_sd_media, &g_file,
                          FX_TEST_READ_FILE,
                          FX_OPEN_FOR_READ);

    if (status != FX_SUCCESS)
    {
        printf("[FileX ERROR] Open for read failed: %s\r\n",
               fx_error_str(status));
        return status;
    }

    /* 파일 크기 확인 */
    ULONG64 file_size = g_file.fx_file_current_file_size;
    printf("[FileX] File size: %lu bytes\r\n", (uint32_t)file_size);

    /* 버퍼 크기 만큼 읽기 */
    ULONG read_size = (ULONG)file_size;
    if (read_size >= sizeof(io_buf))
    {
        read_size = sizeof(io_buf) - 1U;
    }

    status = fx_file_read(&g_file, io_buf, read_size, &actual_read);
    if (status != FX_SUCCESS)
    {
        printf("[FileX ERROR] Read failed: %s\r\n", fx_error_str(status));
        fx_file_close(&g_file);
        return status;
    }

    io_buf[actual_read] = '\0';  /* NULL 종료 */
    fx_file_close(&g_file);

    printf("[FileX] Read OK: %lu bytes\r\n", actual_read);
    printf("--- 파일 내용 ---\r\n%s\r\n-----------------\r\n",
           (char *)io_buf);

    return FX_SUCCESS;
}

/* ============================================================================
 *  FileX_ListDirectory
 *  루트 디렉터리 항목 출력
 * ============================================================================ */
void FileX_ListDirectory(void)
{
    UINT   status;
    CHAR   entry_name[FX_MAX_LONG_NAME_LEN];
    UINT   attributes;
    ULONG  size;
    UINT   year, month, day, hour, minute, second;
    uint32_t count = 0U;

    printf("[FileX] Directory listing (root):\r\n");
    printf("  %-32s %10s  %s\r\n", "Name", "Size", "Date");
    printf("  %s\r\n",
           "------------------------------------------------------------");

    /* 첫 번째 항목 찾기 */
    status = fx_directory_first_full_entry_find(
        &g_sd_media,
        entry_name,
        &attributes,
        &size,
        &year, &month, &day,
        &hour, &minute, &second
    );

    while (status == FX_SUCCESS && count < FX_DIR_LIST_MAX)
    {
        char type_char = (attributes & FX_DIRECTORY) ? 'D' : 'F';

        printf("  [%c] %-30s %10lu  %04u-%02u-%02u %02u:%02u:%02u\r\n",
               type_char,
               entry_name,
               (attributes & FX_DIRECTORY) ? 0UL : size,
               year, month, day,
               hour, minute, second);

        count++;

        /* 다음 항목 */
        status = fx_directory_next_full_entry_find(
            &g_sd_media,
            entry_name,
            &attributes,
            &size,
            &year, &month, &day,
            &hour, &minute, &second
        );
    }

    if (count == 0U)
    {
        printf("  (디렉터리가 비어 있음)\r\n");
    }
    else
    {
        printf("  총 %lu개 항목\r\n", count);
    }

    /* 남은 공간 */
    ULONG64 free_bytes =
        (ULONG64)g_sd_media.fx_media_available_clusters *
        (ULONG64)g_sd_media.fx_media_sectors_per_cluster *
        (ULONG64)g_sd_media.fx_media_bytes_per_sector;

    printf("  남은 공간: %lu KB\r\n\r\n",
           (uint32_t)(free_bytes / 1024ULL));
}

/* ============================================================================
 *  FileX_AppendLog
 *  CSV 로그 파일에 한 줄 추가
 * ============================================================================ */
UINT FileX_AppendLog(uint32_t timestamp, float value)
{
    UINT  status;
    ULONG written = 0U;

    int len = snprintf((char *)io_buf, sizeof(io_buf),
                       "%lu,%.4f\r\n",
                       timestamp, (double)value);

    if (len <= 0)
    {
        return FX_IO_ERROR;
    }

    /* 파일 열기 (없으면 생성) */
    status = fx_file_open(&g_sd_media, &g_file,
                          FX_LOG_FILE,
                          FX_OPEN_FOR_WRITE);

    if (status == FX_NOT_FOUND)
    {
        status = fx_file_create(&g_sd_media, FX_LOG_FILE);
        if (status != FX_SUCCESS && status != FX_ALREADY_CREATED)
        {
            return status;
        }
        status = fx_file_open(&g_sd_media, &g_file,
                              FX_LOG_FILE,
                              FX_OPEN_FOR_WRITE);
    }

    if (status != FX_SUCCESS)
    {
        return status;
    }

    /* 파일 끝으로 이동 (추가 쓰기) */
    status = fx_file_seek(&g_file, g_file.fx_file_current_file_size);
    if (status != FX_SUCCESS)
    {
        fx_file_close(&g_file);
        return status;
    }

    /* 데이터 추가 */
    status = fx_file_write(&g_file, io_buf, (ULONG)len);
    fx_file_close(&g_file);

    if (status != FX_SUCCESS)
    {
        return status;
    }

    /* 주기적 플러시 (예: 10개마다) */
    static uint32_t log_count = 0U;
    log_count++;
    if ((log_count % 10U) == 0U)
    {
        fx_media_flush(&g_sd_media);
    }

    return FX_SUCCESS;
}
