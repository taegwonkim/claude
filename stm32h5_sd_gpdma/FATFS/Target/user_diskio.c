/**
 * @file    user_diskio.c
 * @brief   FatFS 디스크 I/O 인터페이스 → SD 카드 DMA 드라이버 연결
 *
 * FatFS 미들웨어와 sd_card.c 사이의 어댑터 레이어.
 * disk_read / disk_write는 HAL_SD_*_DMA 기반 비동기 전송을 사용하며
 * 완료될 때까지 블로킹(플래그 폴링)하여 FatFS에 결과를 반환함.
 */

#include "user_diskio.h"
#include "sd_card.h"
#include "main.h"

/* -----------------------------------------------------------------------
 * FatFS 드라이버 번호 (CubeMX에서 SD=0으로 할당)
 * ----------------------------------------------------------------------- */
#define SD_DRIVE_NUMBER  0

/* -----------------------------------------------------------------------
 * disk_initialize
 * ----------------------------------------------------------------------- */
DSTATUS USER_initialize(BYTE pdrv)
{
    if (pdrv != SD_DRIVE_NUMBER) return STA_NOINIT;
    if (!SD_IsPresent()) return STA_NODISK;
    return (SD_Init() == SD_OK) ? RES_OK : STA_NOINIT;
}

/* -----------------------------------------------------------------------
 * disk_status
 * ----------------------------------------------------------------------- */
DSTATUS USER_status(BYTE pdrv)
{
    if (pdrv != SD_DRIVE_NUMBER) return STA_NOINIT;
    if (!SD_IsPresent()) return STA_NODISK;

    HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd1);
    if (state == HAL_SD_CARD_TRANSFER) return RES_OK;
    return STA_NOINIT;
}

/* -----------------------------------------------------------------------
 * disk_read — DMA 읽기
 * pdrv   : 드라이브 번호
 * buff   : 읽기 버퍼 (4바이트 정렬 필요)
 * sector : 시작 섹터 (LBA)
 * count  : 섹터 수
 * ----------------------------------------------------------------------- */
DRESULT USER_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != SD_DRIVE_NUMBER) return RES_PARERR;
    if (count == 0) return RES_PARERR;

    /*
     * FatFS 내부 버퍼가 4바이트 정렬되지 않을 수 있으므로
     * 필요 시 정렬된 임시 버퍼를 사용한다.
     * 단순화를 위해 호출 측 정렬 책임으로 가정.
     * (ff_config.h에서 FF_USE_LFN=3 + WORD_ACCESS=0 사용 권장)
     */
    SD_StatusTypeDef st = SD_ReadBlocks_DMA(buff, (uint32_t)sector, (uint32_t)count);

    switch (st) {
        case SD_OK:      return RES_OK;
        case SD_TIMEOUT: return RES_ERROR;
        default:         return RES_ERROR;
    }
}

/* -----------------------------------------------------------------------
 * disk_write — DMA 쓰기
 * ----------------------------------------------------------------------- */
DRESULT USER_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != SD_DRIVE_NUMBER) return RES_PARERR;
    if (count == 0) return RES_PARERR;

    SD_StatusTypeDef st = SD_WriteBlocks_DMA(buff, (uint32_t)sector, (uint32_t)count);

    switch (st) {
        case SD_OK:      return RES_OK;
        case SD_TIMEOUT: return RES_ERROR;
        default:         return RES_ERROR;
    }
}

/* -----------------------------------------------------------------------
 * disk_ioctl — 제어 명령
 * ----------------------------------------------------------------------- */
DRESULT USER_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != SD_DRIVE_NUMBER) return RES_PARERR;

    SD_CardInfoTypeDef info;

    switch (cmd) {
        case CTRL_SYNC:
            /* 쓰기 완료 대기 */
            return (SD_WaitForWriteComplete(SD_TIMEOUT_MS) == SD_OK) ? RES_OK : RES_ERROR;

        case GET_SECTOR_COUNT:
            if (SD_GetCardInfo(&info) != SD_OK) return RES_ERROR;
            *(LBA_t *)buff = (LBA_t)info.BlockCount;
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD *)buff = SD_BLOCK_SIZE;
            return RES_OK;

        case GET_BLOCK_SIZE:
            /* 삭제 블록 크기 (섹터 단위) — SD는 통상 1 */
            *(DWORD *)buff = 1;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}
