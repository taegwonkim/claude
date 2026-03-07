/**
 * @file    fatfs.c
 * @brief   FatFS 드라이버 등록 및 초기화
 *
 * CubeMX 생성 파일 형식 준수.
 * USER_Driver를 드라이브 0 ("0:")으로 등록한다.
 */

#include "fatfs.h"

/* -----------------------------------------------------------------------
 * 드라이버 테이블 (FatFS 내부용)
 * ----------------------------------------------------------------------- */
Diskio_drvTypeDef USER_Driver = {
    .disk_initialize = USER_initialize,
    .disk_status     = USER_status,
    .disk_read       = USER_read,
    .disk_write      = USER_write,
    .disk_ioctl      = USER_ioctl,
};

static char s_SDPath[4];   /* 드라이브 경로 ("0:") */

/* -----------------------------------------------------------------------
 * MX_FATFS_Init
 * main.c에서 SDMMC1 초기화 이후 호출됨.
 * ----------------------------------------------------------------------- */
void MX_FATFS_Init(void)
{
    if (FATFS_LinkDriver(&USER_Driver, s_SDPath) != 0) {
        /* 드라이버 등록 실패 */
        Error_Handler();
    }
}
