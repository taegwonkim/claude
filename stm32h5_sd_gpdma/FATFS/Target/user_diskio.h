#ifndef __USER_DISKIO_H
#define __USER_DISKIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff_gen_drv.h"

extern Diskio_drvTypeDef USER_Driver;

DSTATUS USER_initialize(BYTE pdrv);
DSTATUS USER_status(BYTE pdrv);
DRESULT USER_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count);
DRESULT USER_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count);
DRESULT USER_ioctl(BYTE pdrv, BYTE cmd, void *buff);

#ifdef __cplusplus
}
#endif

#endif /* __USER_DISKIO_H */
