/**
  ******************************************************************************
  * @file    fpga_app.h
  * @brief   FPGA 링크 사용 예 : ADC 피크 데이터 수신, 설정 명령 송신
  ******************************************************************************
  */
#ifndef FPGA_APP_H
#define FPGA_APP_H

#include <stdint.h>

typedef struct
{
  uint32_t peak_val[6];       /* AdcPeakVal  (mV 단위 환산값) */
  uint32_t peak_cnt[6];       /* AdcPeakCnt                   */
  uint32_t update_count;      /* 수신 횟수                    */
  uint32_t last_tick;         /* 마지막 수신 시각 (ms)        */
} FPGA_AdcData;

void FPGA_App_Init(void);
void FPGA_App_Process(void);                 /* 메인루프에서 호출 */

int  FPGA_App_Start(void);                   /* "START"                 */
int  FPGA_App_Stop(void);                    /* "STOP"                  */
int  FPGA_App_WriteAll(uint32_t ref_mv, uint32_t offs_mv, uint32_t interval_s);  /* "SW_ALL,..." */
int  FPGA_App_ReadAll(void);                 /* "SR_ALL" → 응답 3개 값  */

const FPGA_AdcData *FPGA_App_GetAdc(void);
void FPGA_App_GetSettings(uint32_t *ref_mv, uint32_t *offs_mv, uint32_t *interval);  /* SR_ALL 응답값 */
uint8_t FPGA_App_AdcUpdated(void);           /* 새 데이터 있으면 1 (읽으면 클리어) */

#endif /* FPGA_APP_H */
