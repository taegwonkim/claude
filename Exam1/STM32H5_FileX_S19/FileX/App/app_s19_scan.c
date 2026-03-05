/**
 ******************************************************************************
 * @file    app_s19_scan.c
 * @brief   SD 카드 .s19(Motorola S-record) 파일 추출 + UART 파일명 전송
 *
 *  동작 원리:
 *    1. fx_directory_default_set()  → 대상 디렉터리 진입
 *    2. fx_directory_first/next_full_entry_find()  → FAT 엔트리 순회
 *    3. 파일 이름에서 마지막 '.' 이후 확장자 추출 → 소문자 변환
 *    4. ".s19" 와 비교 (대소문자 무관: .S19, .s19, .S19 모두 매치)
 *    5. 매치 시 콜백 호출 (UART 전송 등)
 *    6. 디렉터리 엔트리 + recursive → 재귀 진입 (depth 제한)
 *
 *  스택 사용량:
 *    재귀 1단계당 약 540 바이트 (entry_name[256] + child_path[256] + 로컬)
 *    MAX_DEPTH=8 → 최대 ~4.3 KB 스택 추가 사용
 ******************************************************************************
 */
#include "app_s19_scan.h"
#include "fx_stm32_sd_driver.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ── FileX 미디어 인스턴스 ─────────────────────────────────────────────── */
FX_MEDIA sd_media;

static UCHAR media_buf[S19_MEDIA_BUF_SIZE]
    __attribute__((aligned(32), section(".DMABufferSection")));

/* ============================================================
 *  내부 유틸리티
 * ============================================================ */

/**
 * @brief  파일 이름에서 확장자를 소문자로 추출
 * @param  name 파일 이름
 * @param  ext  결과 버퍼 (최소 16바이트)
 * @retval 1=확장자 있음, 0=없음
 *
 * 예: "firmware.S19" → ext = ".s19", return 1
 *     "README"       → ext = "",     return 0
 */
static int ExtractExtLower(const CHAR *name, CHAR ext[16])
{
    const CHAR *dot = NULL;
    const CHAR *p   = name;

    /* 마지막 '.' 위치 탐색 */
    while (*p)
    {
        if (*p == '.')
            dot = p;
        p++;
    }

    if (dot == NULL)
    {
        ext[0] = '\0';
        return 0;
    }

    /* '.' 포함하여 소문자로 복사 */
    UINT i = 0U;
    while (dot[i] != '\0' && i < 15U)
    {
        ext[i] = (CHAR)tolower((unsigned char)dot[i]);
        i++;
    }
    ext[i] = '\0';
    return 1;
}

/**
 * @brief  경로 결합: base + "/" + child → dest
 * @retval 0=성공, -1=버퍼 초과
 */
static int PathJoin(CHAR       *dest,
                     UINT        dest_sz,
                     const CHAR *base,
                     const CHAR *child)
{
    UINT blen = (UINT)strlen(base);
    UINT clen = (UINT)strlen(child);
    UINT need = blen + 1U + clen + 1U;

    if (need > dest_sz)
        return -1;

    memcpy(dest, base, blen);

    if (blen > 0U && dest[blen - 1U] != '/')
        dest[blen++] = '/';

    memcpy(dest + blen, child, clen);
    dest[blen + clen] = '\0';
    return 0;
}

/* ============================================================
 *  재귀 스캔 엔진
 * ============================================================ */

/**
 * @brief  디렉터리 재귀 순회하며 .s19 파일 탐색
 *
 * @param  cur_path  현재 디렉터리 절대 경로
 * @param  depth     현재 깊이 (루트 = 0)
 * @param  max_d     최대 재귀 깊이
 * @param  recursive 재귀 여부
 * @param  cb        콜백 (NULL 가능)
 * @param  user_ctx  콜백 사용자 데이터
 * @param  res       누적 결과
 */
static UINT ScanDir(const CHAR    *cur_path,
                     UINT           depth,
                     UINT           max_d,
                     UINT           recursive,
                     S19_FoundCb    cb,
                     void          *user_ctx,
                     S19_ScanResult *res)
{
    UINT  ret;
    CHAR  entry[S19_NAME_MAX];
    UINT  attr;
    ULONG size;
    UINT  year, month, day, hour, minute, second;
    CHAR  ext[16];
    CHAR  child[S19_PATH_MAX];

    /* ── 디렉터리 진입 ───────────────────────────────────────────────── */
    ret = fx_directory_default_set(&sd_media, (CHAR *)cur_path);
    if (ret != FX_SUCCESS)
        return ret;

    res->dirs_scanned++;

    /* ── 엔트리 순회 시작 ────────────────────────────────────────────── */
    ret = fx_directory_first_full_entry_find(
              &sd_media, entry, &attr, &size,
              &year, &month, &day, &hour, &minute, &second);

    while (ret == FX_SUCCESS)
    {
        res->total_entries++;

        /* 볼륨 레이블 / 시스템 파일 건너뛰기 */
        if ((attr & FX_VOLUME) || (attr & FX_SYSTEM))
            goto NEXT;

        /* ── 디렉터리 → 재귀 ─────────────────────────────────────── */
        if (attr & FX_DIRECTORY)
        {
            if (recursive && depth < max_d)
            {
                if (PathJoin(child, S19_PATH_MAX, cur_path, entry) == 0)
                {
                    UINT sub = ScanDir(child, depth + 1U, max_d,
                                       recursive, cb, user_ctx, res);

                    /* 재귀 복귀 후 현재 디렉터리 재설정 */
                    fx_directory_default_set(&sd_media, (CHAR *)cur_path);

                    if (sub != FX_SUCCESS)
                    {
                        ret = sub;
                        break;
                    }
                }
            }
            goto NEXT;
        }

        /* ── 파일: .s19 확장자 매치 검사 ─────────────────────────── */
        if (ExtractExtLower(entry, ext) &&
            strcmp(ext, S19_TARGET_EXT) == 0)
        {
            res->found++;
            res->total_bytes += size;

            /* 전체 경로 생성 */
            if (PathJoin(child, S19_PATH_MAX, cur_path, entry) != 0)
            {
                /* 경로 초과 시 이름만 사용 */
                strncpy(child, entry, S19_PATH_MAX - 1U);
                child[S19_PATH_MAX - 1U] = '\0';
            }

            /* 콜백 호출 */
            if (cb != NULL)
                cb(child, entry, size, user_ctx);
        }

NEXT:
        ret = fx_directory_next_full_entry_find(
                  &sd_media, entry, &attr, &size,
                  &year, &month, &day, &hour, &minute, &second);
    }

    return (ret == FX_NO_MORE_ENTRIES) ? FX_SUCCESS : ret;
}

/* ============================================================
 *  공개 API
 * ============================================================ */

UINT S19_Init(void)
{
    fx_system_initialize();
    return fx_media_open(&sd_media, "SD_MEDIA",
                         fx_stm32_sd_driver, NULL,
                         media_buf, sizeof(media_buf));
}

void S19_DeInit(void)
{
    fx_media_flush(&sd_media);
    fx_media_close(&sd_media);
}

UINT S19_Scan(const CHAR    *root_dir,
              UINT           recursive,
              UINT           max_depth,
              S19_FoundCb    cb,
              void          *user_ctx,
              S19_ScanResult *result)
{
    S19_ScanResult local = {0U, 0U, 0U, 0UL};

    const CHAR *root = (root_dir && root_dir[0]) ? root_dir : "/";
    UINT        maxd = max_depth ? max_depth : S19_MAX_DEPTH;

    UINT ret = ScanDir(root, 0U, maxd, recursive, cb, user_ctx, &local);

    if (result)
        *result = local;

    return ret;
}

/* ============================================================
 *  UART 전송 기능
 * ============================================================ */

/** UART 전송 콜백 컨텍스트 */
typedef struct
{
    UART_HandleTypeDef *huart;
    UINT                seq;        /**< 순번 카운터 */
} UartCtx;

#define UART_TX_TIMEOUT  1000U

/** 문자열 UART 전송 (블로킹) */
static void UartStr(UART_HandleTypeDef *h, const char *s)
{
    HAL_UART_Transmit(h, (const uint8_t *)s,
                      (uint16_t)strlen(s), UART_TX_TIMEOUT);
}

/**
 * @brief  .s19 파일 발견 시 UART 전송 콜백
 *
 *  출력 형식:
 *    [001]      4096 B  firmware_v1.s19        /FW/firmware_v1.s19
 */
static void UartFoundCb(const CHAR *path,
                         const CHAR *name,
                         ULONG       size,
                         void       *user_ctx)
{
    UartCtx *ctx = (UartCtx *)user_ctx;
    char line[S19_PATH_MAX + 80];

    ctx->seq++;

    snprintf(line, sizeof(line),
             "  [%03u] %9lu B  %-30s  %s\r\n",
             ctx->seq, size, name, path);

    UartStr(ctx->huart, line);
}

UINT S19_ScanAndSendUART(const CHAR         *root_dir,
                          UART_HandleTypeDef *huart,
                          UINT                recursive,
                          S19_ScanResult     *result)
{
    char buf[S19_PATH_MAX + 64];
    UartCtx ctx = { huart, 0U };

    const CHAR *root = (root_dir && root_dir[0]) ? root_dir : "/";

    /* ── 헤더 ────────────────────────────────────────────────────────── */
    snprintf(buf, sizeof(buf),
             "\r\n"
             "============================================================\r\n"
             "  .s19 File Scanner\r\n"
             "  Root path : %s\r\n"
             "  Recursive : %s\r\n"
             "  Extension : %s\r\n"
             "============================================================\r\n"
             "   #       Size      Filename                        Full Path\r\n"
             "  ---  ---------  ------------------------------  ---------------\r\n",
             root,
             recursive ? "ON" : "OFF",
             S19_TARGET_EXT);
    UartStr(huart, buf);

    /* ── 스캔 실행 ───────────────────────────────────────────────────── */
    S19_ScanResult res = {0U, 0U, 0U, 0UL};
    UINT ret = S19_Scan(root, recursive, 0U,
                        UartFoundCb, &ctx, &res);

    /* ── 결과 요약 ───────────────────────────────────────────────────── */
    snprintf(buf, sizeof(buf),
             "  ---  ---------  ------------------------------  ---------------\r\n"
             "  Found .s19 files : %u\r\n"
             "  Total size       : %lu bytes\r\n"
             "  Dirs scanned     : %u\r\n"
             "  FAT entries read : %u\r\n"
             "============================================================\r\n"
             "\r\n",
             res.found, res.total_bytes,
             res.dirs_scanned, res.total_entries);
    UartStr(huart, buf);

    if (result)
        *result = res;

    return ret;
}
