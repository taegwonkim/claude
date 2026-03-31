/**
 * @file    freertos.c
 * @brief   FreeRTOS Task Definitions for Voltage-Current Monitoring System
 *
 * Tasks:
 *   1. VoltCtrlTask  (AboveNormal) - 10ms  closed-loop voltage control
 *   2. CommRxTask    (Normal)      - UART command reception & processing
 *   3. CommTxTask    (BelowNormal) - 100ms periodic UART + FDCAN reporting
 *   4. FaultMonTask  (High)        - 50ms  short/open circuit detection
 */

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "queue.h"
#include "semphr.h"

#include "main.h"
#include "dac_ad5641.h"
#include "adc_mcp3465r.h"
#include "voltage_control.h"
#include "comm_handler.h"
#include "debug_uart.h"

/* --------------- External HAL Handles (defined in main.c) --------------- */
extern SPI_HandleTypeDef  hspi1;
extern SPI_HandleTypeDef  hspi2;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart4;
extern FDCAN_HandleTypeDef hfdcan1;

/* --------------- Global Driver Handles ---------------------------------- */
AD5641_Handle_t    g_dac;
MCP3465R_Handle_t  g_adc[MCP3465R_NUM_DEVICES];  /* 4 ADCs, one per channel */
VoltCtrl_Handle_t  g_vctrl;
Comm_Handle_t      g_comm;

/* --------------- FreeRTOS Objects --------------------------------------- */

/* Mutexes for SPI bus access */
osMutexId_t spi1MutexHandle;
osMutexId_t spi2MutexHandle;

static const osMutexAttr_t spi1Mutex_attr = {
    .name = "spi1Mutex"
};
static const osMutexAttr_t spi2Mutex_attr = {
    .name = "spi2Mutex"
};

/* Task handles */
osThreadId_t voltCtrlTaskHandle;
osThreadId_t commRxTaskHandle;
osThreadId_t commTxTaskHandle;
osThreadId_t faultMonTaskHandle;

/* Task attributes */
static const osThreadAttr_t voltCtrlTask_attr = {
    .name = "VoltCtrlTask",
    .stack_size = 512 * 4,   /* 2048 bytes */
    .priority = osPriorityAboveNormal,
};

static const osThreadAttr_t commRxTask_attr = {
    .name = "CommRxTask",
    .stack_size = 384 * 4,   /* 1536 bytes */
    .priority = osPriorityNormal,
};

static const osThreadAttr_t commTxTask_attr = {
    .name = "CommTxTask",
    .stack_size = 384 * 4,   /* 1536 bytes */
    .priority = osPriorityBelowNormal,
};

static const osThreadAttr_t faultMonTask_attr = {
    .name = "FaultMonTask",
    .stack_size = 256 * 4,   /* 1024 bytes */
    .priority = osPriorityHigh,
};

/* --------------- Task Function Prototypes ------------------------------- */
void StartVoltCtrlTask(void *argument);
void StartCommRxTask(void *argument);
void StartCommTxTask(void *argument);
void StartFaultMonTask(void *argument);

/* --------------- Previous fault states for edge detection --------------- */
static ChannelState_t prev_fault_state[VCTRL_NUM_CHANNELS];

/* ========================================================================
 * FreeRTOS Initialization (called from main.c after MX_FREERTOS_Init)
 * ======================================================================== */
void MX_FREERTOS_Init(void)
{
    /* ---- Initialize driver handles ---- */

    /* DAC: AD5641 x4 on SPI2 */
    g_dac.hspi = &hspi2;
    g_dac.cs_port[0] = DAC_CS0_GPIO_Port;  g_dac.cs_pin[0] = DAC_CS0_Pin;
    g_dac.cs_port[1] = DAC_CS1_GPIO_Port;  g_dac.cs_pin[1] = DAC_CS1_Pin;
    g_dac.cs_port[2] = DAC_CS2_GPIO_Port;  g_dac.cs_pin[2] = DAC_CS2_Pin;
    g_dac.cs_port[3] = DAC_CS3_GPIO_Port;  g_dac.cs_pin[3] = DAC_CS3_Pin;
    AD5641_Init(&g_dac);

    /* ADC: MCP3465R x4 on SPI1 (one per channel) */
    g_adc[0].hspi = &hspi1;
    g_adc[0].cs_port = ADC_CS0_GPIO_Port;  g_adc[0].cs_pin = ADC_CS0_Pin;

    g_adc[1].hspi = &hspi1;
    g_adc[1].cs_port = ADC_CS1_GPIO_Port;  g_adc[1].cs_pin = ADC_CS1_Pin;

    g_adc[2].hspi = &hspi1;
    g_adc[2].cs_port = ADC_CS2_GPIO_Port;  g_adc[2].cs_pin = ADC_CS2_Pin;

    g_adc[3].hspi = &hspi1;
    g_adc[3].cs_port = ADC_CS3_GPIO_Port;  g_adc[3].cs_pin = ADC_CS3_Pin;

    for (uint8_t i = 0; i < MCP3465R_NUM_DEVICES; i++) {
        MCP3465R_Init(&g_adc[i]);
    }

    /* Voltage controller */
    MCP3465R_Handle_t *adc_ptrs[VCTRL_NUM_CHANNELS] = {
        &g_adc[0], &g_adc[1], &g_adc[2], &g_adc[3]
    };
    VoltCtrl_Init(&g_vctrl, &g_dac, adc_ptrs);

    /* Communication handler */
    Comm_Init(&g_comm, &huart1, &hfdcan1, &g_vctrl);

    /* Init previous fault states */
    for (int i = 0; i < VCTRL_NUM_CHANNELS; i++) {
        prev_fault_state[i] = CH_STATE_DISABLED;
    }

    /* ---- Create RTOS objects ---- */

    /* Mutexes */
    spi1MutexHandle = osMutexNew(&spi1Mutex_attr);
    spi2MutexHandle = osMutexNew(&spi2Mutex_attr);

    /* Tasks */
    voltCtrlTaskHandle = osThreadNew(StartVoltCtrlTask, NULL, &voltCtrlTask_attr);
    commRxTaskHandle   = osThreadNew(StartCommRxTask,   NULL, &commRxTask_attr);
    commTxTaskHandle   = osThreadNew(StartCommTxTask,   NULL, &commTxTask_attr);
    faultMonTaskHandle = osThreadNew(StartFaultMonTask,  NULL, &faultMonTask_attr);

    Debug_Print("SYS", "FreeRTOS init complete, %d tasks created", 4);
}

/* ========================================================================
 * Task 1: Voltage Control Loop (10ms period)
 *
 * - Reads actual voltage from MCP3465R for each enabled channel
 * - Compares with target voltage
 * - Adjusts AD5641 DAC code to minimize error (closed-loop)
 * ======================================================================== */
void StartVoltCtrlTask(void *argument)
{
    (void)argument;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(10);  /* 10ms = 100 Hz */
    uint32_t loop_count = 0;

    Debug_Print("VCTRL", "Task started, period=10ms");

    for (;;)
    {
        /* Acquire SPI mutexes for ADC read + DAC write */
        if (osMutexAcquire(spi1MutexHandle, pdMS_TO_TICKS(5)) == osOK)
        {
            if (osMutexAcquire(spi2MutexHandle, pdMS_TO_TICKS(5)) == osOK)
            {
                /* Execute one iteration of the control loop */
                VoltCtrl_ControlLoop(&g_vctrl);

                osMutexRelease(spi2MutexHandle);
            }
            osMutexRelease(spi1MutexHandle);
        }

        /* Debug output every 100 iterations (1 second) to avoid flooding */
        loop_count++;
        if (loop_count >= 100) {
            loop_count = 0;
            for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
                ChannelData_t d;
                VoltCtrl_GetChannelData(&g_vctrl, ch, &d);
                if (d.enabled) {
                    float err_mv = (d.target_voltage - d.actual_voltage) * 1000.0f;
                    Debug_Print("VCTRL", "CH%d tgt=%.3fV act=%.3fV dac=%u err=%.1fmV",
                                ch, d.target_voltage, d.actual_voltage,
                                d.dac_code, err_mv);
                }
            }
        }

        /* Precise periodic execution */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ========================================================================
 * Task 2: UART Command Reception
 *
 * - Receives characters from UART1 via interrupt
 * - Waits for task notification from UART RX callback
 * - Processes complete command lines
 * ======================================================================== */
void StartCommRxTask(void *argument)
{
    (void)argument;

    Debug_Print("COMM", "RX task started, waiting for UART1 commands");

    /* Start UART interrupt-based reception */
    Comm_StartReception(&g_comm);

    for (;;)
    {
        /* Task notification is sent from UART callback when a complete line is received.
         * Meanwhile, we just yield. The actual command processing happens in the
         * UART RX callback (Comm_UART_RxCallback) which is called from ISR context.
         * The callback directly calls Comm_ProcessCommand.
         *
         * Alternative: Use a queue to decouple ISR from processing.
         * For simplicity, we use direct callback processing.
         */
        osDelay(pdMS_TO_TICKS(1));
    }
}

/* ========================================================================
 * Task 3: Periodic Data Transmission (100ms period)
 *
 * - Sends 4-channel voltage + current data via UART1
 * - Sends same data via FDCAN1
 * ======================================================================== */
void StartCommTxTask(void *argument)
{
    (void)argument;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(100);  /* 100ms = 10 Hz */
    uint32_t tx_count = 0;

    Debug_Print("COMM", "TX task started, period=100ms");

    /* Initial delay to let the system stabilize */
    osDelay(pdMS_TO_TICKS(500));

    for (;;)
    {
        /* Send UART report */
        Comm_SendUartReport(&g_comm);

        /* Send FDCAN report */
        Comm_SendCanReport(&g_comm);

        /* Toggle status LED */
        HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);

        /* Debug: print full channel table every 5 seconds (50 iterations) */
        tx_count++;
        if (tx_count >= 50) {
            tx_count = 0;
            Debug_Print("COMM", "TX report #%lu sent (UART1+FDCAN)", (unsigned long)HAL_GetTick() / 1000);
            Debug_PrintChannelStatus();
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ========================================================================
 * Task 4: Fault Monitoring (50ms period)
 *
 * - Reads current for each enabled channel
 * - Detects short circuit (over-current)
 * - Detects open circuit (no current when voltage is set)
 * - Sends fault alerts via UART and FDCAN
 * ======================================================================== */
void StartFaultMonTask(void *argument)
{
    (void)argument;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(50);  /* 50ms = 20 Hz */

    Debug_Print("FAULT", "Monitor task started, period=50ms");

    /* Initial delay */
    osDelay(pdMS_TO_TICKS(1000));

    for (;;)
    {
        /* Check faults using existing channel data from control loop */
        VoltCtrl_CheckFaults(&g_vctrl);

        /* Check for new fault events and send alerts */
        for (uint8_t ch = 0; ch < VCTRL_NUM_CHANNELS; ch++) {
            ChannelData_t data;
            VoltCtrl_GetChannelData(&g_vctrl, ch, &data);

            /* Detect fault state transitions */
            if (data.state != prev_fault_state[ch]) {
                if (data.state == CH_STATE_SHORT_CIRCUIT ||
                    data.state == CH_STATE_OPEN_CIRCUIT) {
                    /* New fault detected -> send alert */
                    Comm_SendFaultAlert(&g_comm, ch, data.state);
                    Debug_Print("FAULT", "CH%d %s detected! V=%.3fV I=%.2fmA",
                                ch,
                                (data.state == CH_STATE_SHORT_CIRCUIT) ? "SHORT_CIRCUIT" : "OPEN_CIRCUIT",
                                data.actual_voltage, data.current_mA);

                    /* Turn on fault LED */
                    HAL_GPIO_WritePin(LED_FAULT_GPIO_Port, LED_FAULT_Pin, GPIO_PIN_SET);
                }
                else if (prev_fault_state[ch] == CH_STATE_SHORT_CIRCUIT ||
                         prev_fault_state[ch] == CH_STATE_OPEN_CIRCUIT) {
                    /* Fault cleared */
                    Comm_UartSend(&g_comm, "CLEAR %d\r\n", ch);
                    Debug_Print("FAULT", "CH%d fault CLEARED", ch);

                    /* Turn off fault LED if no other channel is faulted */
                    uint8_t any_fault = 0;
                    for (uint8_t j = 0; j < VCTRL_NUM_CHANNELS; j++) {
                        ChannelData_t cd;
                        VoltCtrl_GetChannelData(&g_vctrl, j, &cd);
                        if (cd.state == CH_STATE_SHORT_CIRCUIT ||
                            cd.state == CH_STATE_OPEN_CIRCUIT) {
                            any_fault = 1;
                            break;
                        }
                    }
                    if (!any_fault) {
                        HAL_GPIO_WritePin(LED_FAULT_GPIO_Port, LED_FAULT_Pin, GPIO_PIN_RESET);
                    }
                }

                prev_fault_state[ch] = data.state;
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ========================================================================
 * FreeRTOS Hook Functions
 * ======================================================================== */

/**
 * @brief Called when memory allocation fails
 */
void vApplicationMallocFailedHook(void)
{
    /* Halt in debug: heap exhausted */
    __disable_irq();
    for (;;) {}
}

/**
 * @brief Called when a stack overflow is detected
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /* Halt in debug: stack overflow */
    __disable_irq();
    for (;;) {}
}
