/**
 ******************************************************************************
 * @file    app_s19_scan.h
 * @brief   SD 카드에서 .s19 확장자 파일을 추출하여 UART 로 파일명 전송
 *
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │  .s19 (Motorola S-record) 파일 스캔 전용 모듈              │
 *  │                                                             │
 *  │  기능:                                                      │
 *  │    1. SD 카드 마운트 (FileX Standalone, ThreadX 미사용)     │
 *  │    2. 지정 디렉터리에서 .s19 확장자 파일 전부 추출          │
 *  │    3. 하위 디렉터리 재귀 탐색 지원 (depth 제한)             │
 *  │    4. 매치된 파일명을 UART 로 실시간 전송                   │
 *  │    5. 콜백 방식 → 사용자 커스텀 처리도 가능                 │
 *  └─────────────────────────────────────────────────────────────┘
 *
 *  사용 예 (최소):
 *    S19_Init();
 *    S19_ScanAndSendUART("/", &huart3, 1);    // 루트 재귀 스캔
 *    S19_DeInit();
 *
 *  대상 MCU : STM32H563ZI (NUCLEO-H563ZI)
 ******************************************************************************
 */
#ifndef APP_S19_SCAN_H
#define APP_S19_SCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fx_api.h"
#include "stm32h5xx_hal.h"

/* ============================================================
 *  설정 상수
 * ============================================================ */
#define S19_MEDIA_BUF_SIZE  (512U * 4U)   /**< 미디어 I/O 버퍼 2 KB      */
#define S19_PATH_MAX        256U          /**< 전체 경로 최대 길이         */
#define S19_NAME_MAX        256U          /**< 파일 이름 최대 길이         */
#define S19_MAX_DEPTH       8U            /**< 기본 최대 재귀 깊이         */
#define S19_TARGET_EXT      ".s19"        /**< 탐색 대상 확장자            */

/* ============================================================
 *  스캔 결과 구조체
 * ============================================================ */
typedef struct
{
    UINT  found;            /**< .s19 매치 파일 수                        */
    UINT  dirs_scanned;     /**< 탐색한 디렉터리 수                       */
    UINT  total_entries;    /**< 순회한 전체 FAT 엔트리 수                */
    ULONG total_bytes;      /**< .s19 파일 총 크기 합계 (바이트)          */
} S19_ScanResult;

/* ============================================================
 *  콜백 타입
 * ============================================================ */

/**
 * @brief  .s19 파일 발견 시 호출되는 콜백
 *
 * @param  path       전체 경로 (예: "/FW/v3/app.s19")
 * @param  name       파일 이름만 (예: "app.s19")
 * @param  size       파일 크기 (바이트)
 * @param  user_ctx   사용자 포인터
 */
typedef void (*S19_FoundCb)(const CHAR *path,
                             const CHAR *name,
                             ULONG       size,
                             void       *user_ctx);

/* ============================================================
 *  외부 공개 변수
 * ============================================================ */
extern FX_MEDIA sd_media;

/* ============================================================
 *  공개 API
 * ============================================================ */

/**
 * @brief  FileX 초기화 + SD FAT 마운트
 * @retval FX_SUCCESS / FX 오류 코드
 */
UINT S19_Init(void);

/**
 * @brief  미디어 flush + close
 */
void S19_DeInit(void);

/**
 * @brief  .s19 파일 스캔 – 콜백 방식
 *
 * @param  root_dir   탐색 시작 경로 (NULL/"" → 루트 "/")
 * @param  recursive  1 = 하위 디렉터리 재귀, 0 = 현재만
 * @param  max_depth  최대 깊이 (0 = 기본 S19_MAX_DEPTH)
 * @param  cb         파일 발견 콜백 (NULL 가능 → 카운트만)
 * @param  user_ctx   콜백에 전달할 사용자 포인터
 * @param  result     결과 요약 (NULL 가능)
 * @retval FX_SUCCESS / FX 오류 코드
 */
UINT S19_Scan(const CHAR    *root_dir,
              UINT           recursive,
              UINT           max_depth,
              S19_FoundCb    cb,
              void          *user_ctx,
              S19_ScanResult *result);

/**
 * @brief  .s19 파일 스캔 + UART 자동 전송 (원스톱 API)
 *
 * @param  root_dir   탐색 시작 경로
 * @param  huart      UART 핸들
 * @param  recursive  1 = 재귀, 0 = 현재만
 * @param  result     결과 요약 (NULL 가능)
 * @retval FX_SUCCESS / FX 오류 코드
 */
UINT S19_ScanAndSendUART(const CHAR         *root_dir,
                          UART_HandleTypeDef *huart,
                          UINT                recursive,
                          S19_ScanResult     *result);

#ifdef __cplusplus
}
#endif
#endif /* APP_S19_SCAN_H */
