/**
  ******************************************************************************
  * @file    app_main.h
  * @brief   애플리케이션 진입점
  *
  *  main.c 사용 방법 (Integration/main_usercode.c 참조)
  *    1) MX_*_Init() 이후, MX_FREERTOS_Init() 전에  App_PreKernelInit()
  *    2) StartDefaultTask() 안에서                 App_Main()
  ******************************************************************************
  */
#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "app_types.h"

/* 커널 오브젝트 생성 (osKernelInitialize() 이후, osKernelStart() 이전) */
sd_res_t App_PreKernelInit(void);

/* defaultTask 본문 : 나머지 태스크 생성 후 하트비트 루프로 진입 (반환하지 않음) */
void     App_Main(void);

#endif /* APP_MAIN_H */
