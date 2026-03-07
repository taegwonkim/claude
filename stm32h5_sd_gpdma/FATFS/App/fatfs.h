#ifndef __FATFS_H
#define __FATFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"
#include "ff_gen_drv.h"
#include "user_diskio.h"

void MX_FATFS_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __FATFS_H */
