/**
 ******************************************************************************
 * @file    bkpram_example.h
 * @brief   BKPRAM 예제 헤더
 ******************************************************************************
 */

#ifndef BKPRAM_EXAMPLE_H
#define BKPRAM_EXAMPLE_H

#include <stdint.h>

void     BKPRAM_Example_Init(void);
void     BKPRAM_Example_Reset(void);
void     BKPRAM_Example_Save(void);
uint32_t BKPRAM_Example_GetAppMode(void);
void     BKPRAM_Example_SetAppMode(uint32_t mode);
void     BKPRAM_Example_PrintStats(void);

#endif /* BKPRAM_EXAMPLE_H */
