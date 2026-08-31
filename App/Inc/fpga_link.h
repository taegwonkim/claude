/**
  ******************************************************************************
  * @file    fpga_link.h
  * @brief   Cyclone IV FPGA 인터페이스 (USART2 + PA1 트리거)
  ******************************************************************************
  */
#ifndef FPGA_LINK_H
#define FPGA_LINK_H

#include "app_types.h"

/* FPGA 에 수집 시작/정지 명령 전송 */
sd_res_t fpga_send_start(void);
sd_res_t fpga_send_stop(void);

/* 트리거 태스크 (app_main.c 에서 osThreadNew) */
void     fpga_task(void *arg);

/* EXTI 콜백에서 호출 (hooks.c) */
void     fpga_on_trigger_isr(void);

#endif /* FPGA_LINK_H */
