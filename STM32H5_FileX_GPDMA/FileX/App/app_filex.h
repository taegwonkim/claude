/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_filex.h
  * @brief   FileX 애플리케이션 헤더
  *          (RTOS 없이 standalone FileX 사용)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef APP_FILEX_H
#define APP_FILEX_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "fx_api.h"
#include "fx_stm32_sd_driver.h"

/* Exported defines ----------------------------------------------------------*/

/** FileX 미디어 섹터 캐시 크기 (섹터 수 × 512 바이트)
 *  RAM이 충분하면 늘릴수록 성능 향상 */
#define FX_MEDIA_CACHE_SECTORS   16U
#define FX_MEDIA_CACHE_SIZE      (FX_MEDIA_CACHE_SECTORS * FX_SD_SECTOR_SIZE)

/** 파일 읽기/쓰기용 I/O 버퍼 크기 */
#define FX_IO_BUFFER_SIZE        4096U

/** 테스트 파일명 */
#define FX_TEST_WRITE_FILE       "TESTWRITE.TXT"
#define FX_TEST_READ_FILE        "TESTWRITE.TXT"
#define FX_LOG_FILE              "DATALOG.CSV"

/** 최대 디렉터리 항목 출력 수 */
#define FX_DIR_LIST_MAX          32U

/* Exported types ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
extern FX_MEDIA  g_sd_media;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief FileX 초기화 및 SD 미디어 마운트
 * @retval FX_SUCCESS 또는 FileX 에러 코드
 */
UINT MX_FileX_Init(void);

/**
 * @brief FileX SD 미디어 마운트 해제 (종료 전 호출)
 */
void MX_FileX_DeInit(void);

/**
 * @brief 테스트 파일에 데이터 쓰기
 * @retval FX_SUCCESS 또는 FileX 에러 코드
 */
UINT FileX_WriteTest(void);

/**
 * @brief 테스트 파일에서 데이터 읽기 및 검증
 * @retval FX_SUCCESS 또는 FileX 에러 코드
 */
UINT FileX_ReadTest(void);

/**
 * @brief 루트 디렉터리 목록 출력 (UART)
 */
void FileX_ListDirectory(void);

/**
 * @brief CSV 로그 파일에 데이터 추가
 * @param timestamp  타임스탬프 (ms)
 * @param value      로그 값
 * @retval FX_SUCCESS 또는 FileX 에러 코드
 */
UINT FileX_AppendLog(uint32_t timestamp, float value);

#ifdef __cplusplus
}
#endif

#endif /* APP_FILEX_H */
