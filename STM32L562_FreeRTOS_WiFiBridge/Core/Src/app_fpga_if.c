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

/** STM32 -> Cyclone IV: "start a measurement" command (see FPGA_CMD_START_MEASURE). */
static void Fpga_SendStartCommand(void)
{
    uint8_t cmd = FPGA_CMD_START_MEASURE;
    HAL_UART_Transmit(FPGA_UART_HANDLE, &cmd, 1U, 100U);
}

/**
 * Continuous measurement loop:
 *   1) make sure the ESP32 Wi-Fi + server link is up (reconnect + wait if not)
 *   2) tell Cyclone IV to start a measurement
 *   3) wait for its "measurement complete" trigger (falling edge) + data (USART3)
 *   4) forward the data to the server via the ESP32 link
 *   5) repeat immediately - runs continuously as long as the link stays up;
 *      if it drops, step 1 blocks here until reconnected before resuming.
 */
void App_FpgaIf_Task(void *argument)
{
    (void)argument;

    for (;;) {
        while (!App_Esp32_IsConnected()) {
            if (!App_Esp32_ConnectAndWait(ESP32_CONNECT_WAIT_TIMEOUT_MS)) {
                osDelay(FPGA_RECONNECT_RETRY_DELAY_MS);
            }
        }

        /* drain any stale trigger/rx-done signal left over from a previous,
         * aborted cycle before starting a fresh one */
        osSemaphoreAcquire(s_triggerSem, 0);
        osSemaphoreAcquire(s_rxDoneSem, 0);

        Fpga_SendStartCommand();

        if (osSemaphoreAcquire(s_triggerSem, FPGA_MEASURE_TIMEOUT_MS) != osOK) {
            /* Cyclone IV didn't respond to this cycle's START - try again;
             * the top-of-loop connection check also re-runs. */
            continue;
        }

        s_frameLen = 0;
        HAL_UARTEx_ReceiveToIdle_DMA(FPGA_UART_HANDLE, s_frameBuf, FPGA_FRAME_MAX_LEN);

        if (osSemaphoreAcquire(s_rxDoneSem, FPGA_RX_TOTAL_TIMEOUT_MS) == osOK) {
            if (s_frameLen > 0U) {
                /* Forward straight to the ESP32 link -> server. A failed
                 * send means the link dropped mid-cycle; the next loop
                 * iteration's connection check will reconnect before the
                 * following measurement is started. */
                (void)App_Esp32_SendMeasurementData(s_frameBuf, s_frameLen,
                                                     ESP32_SEND_TIMEOUT_MS + 1000U);
            }
        } else {
            /* No data arrived in time after the trigger - abort and resync. */
            HAL_UART_AbortReceive(FPGA_UART_HANDLE);
        }
    }
}
