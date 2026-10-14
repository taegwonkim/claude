/**
  ******************************************************************************
  * @file    fpga_app.c
  * @brief   FPGA 링크 사용 예
  *
  *  main.c 에 추가할 것
  *    #include "fpga_app.h"
  *    ...  MX_xxx_Init() 들 다음에   FPGA_App_Init();
  *    ...  while (1) { ...           FPGA_App_Process(); ... }
  ******************************************************************************
  */
#include "fpga_app.h"
#include "fpga_link.h"
#include <stdio.h>

static FPGA_AdcData adc;
static uint8_t      adc_updated;
static uint8_t      setting_pending;     /* SR_ALL 응답 대기 중 */

static uint32_t     fpga_ref_mv, fpga_offs_mv, fpga_interval;

void FPGA_App_Init(void)
{
  (void)FPGA_Link_Init();
  (void)FPGA_App_ReadAll();             /* 현재 설정 확인 */
  (void)FPGA_App_Start();               /* 피크 검출 / 주기 전송 시작 */
}

static void handle_packet(const FPGA_Packet *pkt)
{
  long v[16];
  int  n = FPGA_Proto_ParseNumbers(pkt, v, 16);
  int  ch;

  if ((n == 12) && (pkt->data[0] == '"'))
  {
    /* ADC_Data_to_MCU_All() : "\"peak0,cnt0,peak1,cnt1,...,peak5,cnt5\"" */
    for (ch = 0; ch < 6; ch++)
    {
      adc.peak_val[ch] = (uint32_t)v[ch * 2];
      adc.peak_cnt[ch] = (uint32_t)v[ch * 2 + 1];
    }
    adc.update_count++;
    adc.last_tick = HAL_GetTick();
    adc_updated = 1U;
  }
  else if (setting_pending && (n == 3))
  {
    /* SR_ALL 응답 : "ref,offs,interval" */
    fpga_ref_mv   = (uint32_t)v[0];
    fpga_offs_mv  = (uint32_t)v[1];
    fpga_interval = (uint32_t)v[2];
    setting_pending = 0U;
  }
  /* 그 외 응답(SR_REFV 등)은 필요에 따라 추가 */
}

void FPGA_App_Process(void)
{
  FPGA_Packet pkt;

  FPGA_Link_Process();

  while (FPGA_Link_GetPacket(&pkt))
  {
    handle_packet(&pkt);
  }
}

int FPGA_App_Start(void) { return FPGA_Link_SendCommand("START"); }
int FPGA_App_Stop(void)  { return FPGA_Link_SendCommand("STOP"); }

int FPGA_App_WriteAll(uint32_t ref_mv, uint32_t offs_mv, uint32_t interval_s)
{
  char cmd[48];

  (void)snprintf(cmd, sizeof(cmd), "SW_ALL,%lu,%lu,%lu",
                 (unsigned long)ref_mv, (unsigned long)offs_mv, (unsigned long)interval_s);
  return FPGA_Link_SendCommand(cmd);
}

int FPGA_App_ReadAll(void)
{
  setting_pending = 1U;
  return FPGA_Link_SendCommand("SR_ALL");
}

const FPGA_AdcData *FPGA_App_GetAdc(void)
{
  return &adc;
}

void FPGA_App_GetSettings(uint32_t *ref_mv, uint32_t *offs_mv, uint32_t *interval)
{
  *ref_mv   = fpga_ref_mv;
  *offs_mv  = fpga_offs_mv;
  *interval = fpga_interval;
}

uint8_t FPGA_App_AdcUpdated(void)
{
  uint8_t u = adc_updated;
  adc_updated = 0U;
  return u;
}
