/**
 ******************************************************************************
 * @file    fx_user.h
 * @brief   FileX 사용자 설정 – Standalone (ThreadX 미사용)
 ******************************************************************************
 */
#ifndef FX_USER_H
#define FX_USER_H

#define FX_STANDALONE_ENABLE            /* RTOS 없이 FileX 단독 동작     */
#define FX_MAX_SECTOR_CACHE     4U      /* 내부 섹터 캐시 수             */
#define FX_MAXIMUM_PATH         256U    /* 최대 경로 길이                */
#define FX_NO_TIMER                     /* RTC 미사용 → 타임스탬프 OFF   */
#define FX_NO_LOCAL_PATH                /* 로컬 경로 비활성화            */

#endif /* FX_USER_H */
