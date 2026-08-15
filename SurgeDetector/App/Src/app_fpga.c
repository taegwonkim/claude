/**
 * app_fpga.c — FPGA 서지 데이터 취득 태스크 구현
 */
#include "app_fpga.h"
#include "app_wifi.h"
#include "app_pccomm.h"
#include "app_config.h"
#include "protocol.h"
#include "main.h"
#include <string.h>

extern UART_HandleTypeDef huart2;

osSemaphoreId_t semFpgaTrigger;
osSemaphoreId_t semUart2RxCplt;

static osThreadId_t s_fpgaTaskHandle;
static uint8_t      s_frameBuf[FPGA_FRAME_SIZE];
static uint16_t     s_seqCounter;

void App_FPGA_OnTrigger(void)
{
    osSemaphoreRelease(semFpgaTrigger);
}

void App_FPGA_OnUartRxCplt(void)
{
    osSemaphoreRelease(semUart2RxCplt);
}

/* 큐가 가득 차면 가장 오래된 항목을 버리고 최신 데이터를 넣는다 (실시간성 우선) */
static void PushOrDropOldest(osMessageQueueId_t q, const SurgeData_t *data)
{
    if (osMessageQueuePut(q, data, 0, 0) == osOK)
    {
        return;
    }

    SurgeData_t dummy;
    (void)osMessageQueueGet(q, &dummy, NULL, 0); /* 가장 오래된 항목 제거 */
    (void)osMessageQueuePut(q, data, 0, 0);
}

static bool ParseFrame(const uint8_t *buf, SurgeData_t *out)
{
    if (buf[0] != FPGA_FRAME_SYNC0 || buf[1] != FPGA_FRAME_SYNC1)
    {
        return false;
    }
    if (buf[FPGA_FRAME_SIZE - 2] != FPGA_FRAME_TAIL0 || buf[FPGA_FRAME_SIZE - 1] != FPGA_FRAME_TAIL1)
    {
        return false;
    }

    uint16_t crcRx = ((uint16_t)buf[FPGA_FRAME_SIZE - 4] << 8) | buf[FPGA_FRAME_SIZE - 3];
    uint16_t crcCalc = Protocol_CRC16(&buf[2], 2u + (FPGA_ADC_CHANNELS * 2u));
    if (crcRx != crcCalc)
    {
        return false;
    }

    out->timestamp = osKernelGetTickCount();
    out->seq = ((uint16_t)buf[2] << 8) | buf[3];

    for (uint32_t ch = 0; ch < FPGA_ADC_CHANNELS; ch++)
    {
        uint32_t off = 4u + (ch * 2u);
        out->channel[ch] = ((uint16_t)buf[off] << 8) | buf[off + 1];
    }

    return true;
}

static void App_FPGA_Task(void *argument)
{
    (void)argument;
    SurgeData_t data;

    osEventFlagsWait(egSysStatus, EVT_CONFIG_LOADED, osFlagsWaitAll, osWaitForever);

    /* FPGA에 취득 시작 명령을 최초 1회만 전송 */
    HAL_UART_Transmit(&huart2, (uint8_t *)"START\r\n", 7, 500);

    for (;;)
    {
        /* FPGA 트리거(falling edge) 대기 */
        if (osSemaphoreAcquire(semFpgaTrigger, osWaitForever) != osOK)
        {
            continue;
        }

        /* 트리거 직후 20바이트 데이터 프레임 수신 시작 */
        if (HAL_UART_Receive_DMA(&huart2, s_frameBuf, FPGA_FRAME_SIZE) != HAL_OK)
        {
            continue;
        }

        if (osSemaphoreAcquire(semUart2RxCplt, 200u) != osOK)
        {
            HAL_UART_AbortReceive(&huart2);
            continue; /* 프레임 timeout -> 다음 트리거까지 대기 */
        }

        if (!ParseFrame(s_frameBuf, &data))
        {
            continue; /* CRC/헤더 불일치 프레임 폐기 */
        }

        s_seqCounter = data.seq;
        (void)s_seqCounter;

        PushOrDropOldest(qFpgaToWifi, &data);
        PushOrDropOldest(qFpgaToPc, &data);
    }
}

void App_FPGA_Init(void)
{
    const osSemaphoreAttr_t sAttr1 = { .name = "semFpgaTrigger" };
    semFpgaTrigger = osSemaphoreNew(1, 0, &sAttr1);

    const osSemaphoreAttr_t sAttr2 = { .name = "semUart2RxCplt" };
    semUart2RxCplt = osSemaphoreNew(1, 0, &sAttr2);

    const osThreadAttr_t tAttr = {
        .name = "fpgaTask",
        .priority = osPriorityHigh,
        .stack_size = 512 * 4,
    };
    s_fpgaTaskHandle = osThreadNew(App_FPGA_Task, NULL, &tAttr);
}
