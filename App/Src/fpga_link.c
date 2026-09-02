/**
  ******************************************************************************
  * @file    fpga_link.c
  * @brief   트리거(하강 에지) → 18바이트 프레임 수신 → 검증 → qSample 게시
  *
  *  프레임 규격은 docs/03_Protocol.md 1.3 참조
  *    [0]=0xA5 [1]=0x5A [2..3]=SEQ(LE) [4..15]=CH0..CH5(LE) [16]=STATUS [17]=CRC8
  ******************************************************************************
  */
#include "fpga_link.h"
#include "uart_link.h"
#include "cfg_store.h"
#include "util_crc.h"

#define FPGA_TX_TMO_MS   100u

sd_res_t fpga_send_start(void)
{
  sd_res_t r = uartlink_puts(&g_lnkFpga, "START\r\n", FPGA_TX_TMO_MS);

  if (r == SD_OK)
  {
    osEventFlagsSet(g_evtSys, SD_EVT_FPGA_RUN);
  }
  return r;
}

sd_res_t fpga_send_stop(void)
{
  sd_res_t r = uartlink_puts(&g_lnkFpga, "STOP\r\n", FPGA_TX_TMO_MS);

  osEventFlagsClear(g_evtSys, SD_EVT_FPGA_RUN);
  return r;
}

void fpga_on_trigger_isr(void)
{
  g_stat.trig_cnt++;
  (void)osSemaphoreRelease(g_semTrig);   /* 가득 차면 osErrorResource, 무시 */
}

/* ---------------------------------------------------------------------------
 * 프레임 동기화 : 링버퍼에서 0xA5 0x5A 를 찾는다.
 * -------------------------------------------------------------------------*/
static sd_res_t sync_to_sof(uint32_t deadline)
{
  uint8_t b;
  bool    got_a5 = false;

  for (;;)
  {
    uint32_t now = osKernelGetTickCount();
    if (now >= deadline) { return SD_ERR_TMO; }

    if (uartlink_getc(&g_lnkFpga, &b, deadline - now) != SD_OK)
    {
      return SD_ERR_TMO;
    }

    if (got_a5 && (b == SD_FPGA_SOF1))
    {
      return SD_OK;
    }
    got_a5 = (b == SD_FPGA_SOF0);
  }
}

static sd_res_t recv_frame(sd_sample_t *out, uint32_t timeout_ms)
{
  uint8_t  body[SD_FPGA_FRAME_LEN - 2u];   /* SOF 2바이트 제외 16바이트 */
  uint32_t deadline = osKernelGetTickCount() + timeout_ms;
  uint32_t now;

  if (sync_to_sof(deadline) != SD_OK)
  {
    return SD_ERR_TMO;
  }

  now = osKernelGetTickCount();
  if (now >= deadline) { return SD_ERR_TMO; }

  if (uartlink_read(&g_lnkFpga, body, sizeof(body), deadline - now) != SD_OK)
  {
    return SD_ERR_TMO;
  }

#if SD_FPGA_CHECK_CRC
  {
    uint8_t whole[SD_FPGA_FRAME_LEN];

    whole[0] = SD_FPGA_SOF0;
    whole[1] = SD_FPGA_SOF1;
    memcpy(&whole[2], body, sizeof(body));

    if (sd_crc8(whole, SD_FPGA_FRAME_LEN - 1u) != whole[SD_FPGA_FRAME_LEN - 1u])
    {
      return SD_ERR_CRC;
    }
  }
#endif

  out->tick_ms = osKernelGetTickCount();
  out->seq     = (uint16_t)((uint16_t)body[0] | ((uint16_t)body[1] << 8));

  for (uint32_t i = 0u; i < SD_ADC_CH_NUM; i++)
  {
    uint32_t o = 2u + (i * 2u);
    out->ch[i] = (uint16_t)((uint16_t)body[o] | ((uint16_t)body[o + 1u] << 8));
  }
  out->status = body[14];
  out->rsv[0] = 0u; out->rsv[1] = 0u; out->rsv[2] = 0u;

  return SD_OK;
}

/* ---------------------------------------------------------------------------
 * 태스크
 * -------------------------------------------------------------------------*/
void fpga_task(void *arg)
{
  sd_sample_t s;
  bool auto_start;

  (void)arg;

  /* 설정에 따라 부팅 후 한 번 START 전송 */
  if (cfgstore_lock(1000u))
  {
    auto_start = (g_cfg.auto_start != 0u);
    cfgstore_unlock();
  }
  else
  {
    auto_start = true;
  }

  osDelay(200u);                       /* FPGA 기동 대기 */
  uartlink_flush_rx(&g_lnkFpga);

  if (auto_start)
  {
    (void)fpga_send_start();
  }

  for (;;)
  {
    /* 트리거 대기 : 타임아웃을 두어 태스크가 완전히 멈추지 않도록 한다 */
    if (osSemaphoreAcquire(g_semTrig, 5000u) != osOK)
    {
      continue;                        /* 트리거 없음 — 정상(수집 정지 상태) */
    }

    sd_res_t r = recv_frame(&s, SD_FPGA_FRAME_TIMEOUT_MS);

    if (r == SD_OK)
    {
      g_stat.rx_frame++;
      (void)sd_queue_put_overwrite(g_qSample, &s, &g_stat.drop_sample);
    }
    else if (r == SD_ERR_CRC)
    {
      g_stat.rx_crcerr++;
      uartlink_flush_rx(&g_lnkFpga);   /* 동기 깨짐 방지 */
    }
    else
    {
      g_stat.rx_timeout++;
      uartlink_flush_rx(&g_lnkFpga);
    }
  }
}
