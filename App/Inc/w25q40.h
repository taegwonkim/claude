/**
  ******************************************************************************
  * @file    w25q40.h
  * @brief   W25Q40CLS (4Mbit SPI NOR) 드라이버 — SPI2, CS = FLASH_CS(PB12)
  ******************************************************************************
  */
#ifndef W25Q40_H
#define W25Q40_H

#include "app_types.h"

sd_res_t w25q_init(void);

/* JEDEC ID (9Fh) : 0xMMTTCC (제조사/타입/용량) */
sd_res_t w25q_read_id(uint32_t *id);

sd_res_t w25q_read(uint32_t addr, uint8_t *dst, uint32_t len);

/* 4KB 섹터 소거 (addr 는 섹터 경계로 내림 정렬됨) */
sd_res_t w25q_erase_sector(uint32_t addr);

/* 페이지 경계를 자동으로 넘기며 기록. 대상 영역은 미리 소거되어 있어야 한다. */
sd_res_t w25q_write(uint32_t addr, const uint8_t *src, uint32_t len);

#endif /* W25Q40_H */
