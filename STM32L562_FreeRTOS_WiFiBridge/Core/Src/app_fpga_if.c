#include "app_fpga_if.h"
#include "app_esp32.h"
#include "cmsis_os2.h"

static uint8_t  s_frameBuf[FPGA_FRAME_MAX_LEN];
static volatile uint16_t s_frameLen;

static osSemaphoreId_t s_triggerSem;  /* given on FPGA trigger falling edge */
static osSemaphoreId_t s_rxDoneSem;   /* given when USART3 idle-line RX completes */

void App_FpgaIf_Init(void)
{
    s_triggerSem = osSemaphoreNew(1, 0, NULL);
    s_rxDoneSem  = osSemaphoreNew(1, 0, NULL);
    s_frameLen = 0;
}

void App_FpgaIf_TriggerISR(void)
{
    osSemaphoreRelease(s_triggerSem);
}

void App_FpgaIf_RxEventISR(uint16_t size)
{
    s_frameLen = size;
    osSemaphoreRelease(s_rxDoneSem);
}

void App_FpgaIf_Task(void *argument)
{
    (void)argument;

    for (;;) {
        /* Wait for Cyclone IV to pulse the trigger line low. */
        osSemaphoreAcquire(s_triggerSem, osWaitForever);

        s_frameLen = 0;
        HAL_UARTEx_ReceiveToIdle_DMA(FPGA_UART_HANDLE, s_frameBuf, FPGA_FRAME_MAX_LEN);

        if (osSemaphoreAcquire(s_rxDoneSem, FPGA_RX_TOTAL_TIMEOUT_MS) == osOK) {
            if (s_frameLen > 0U) {
                /* Forward straight to the ESP32 link -> server. */
                (void)App_Esp32_SendMeasurementData(s_frameBuf, s_frameLen,
                                                     ESP32_SEND_TIMEOUT_MS + 1000U);
            }
        } else {
            /* No data arrived in time after the trigger - abort and resync. */
            HAL_UART_AbortReceive(FPGA_UART_HANDLE);
        }
    }
}
