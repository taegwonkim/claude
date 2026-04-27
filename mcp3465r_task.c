/**
 * MCP3465R FreeRTOS Task — calibration + continuous measurement
 *
 * State machine:
 *
 *   ┌──────────┐   success    ┌─────────────┐   RECAL notify
 *   │  INIT    │ ──────────►  │  MEASURE    │ ◄─────────────────┐
 *   │ (cal+cfg)│              │  (sampling) │                   │
 *   └──────────┘              └──────┬──────┘                   │
 *        │ error                     │ RECAL notify      ┌──────┴──────┐
 *        ▼                           └──────────────────► │ CALIBRATE   │
 *   ┌──────────┐                                         │ (re-run cal)│
 *   │  ERROR   │                                         └─────────────┘
 *   │  (halt)  │
 *   └──────────┘
 */

#include "mcp3465r_task.h"
#include "task.h"
#include "timers.h"

/* ------------------------------------------------------------------ */
/*  Task states                                                         */
/* ------------------------------------------------------------------ */

typedef enum {
    STATE_INIT,
    STATE_CALIBRATE,
    STATE_MEASURE,
    STATE_ERROR,
} TaskState_t;

/* ------------------------------------------------------------------ */
/*  Task body                                                           */
/* ------------------------------------------------------------------ */

static void mcp3465r_task_fn(void *pv)
{
    const MCP3465R_TaskParams_t *params = (const MCP3465R_TaskParams_t *)pv;
    MCP3465R_Handle_t           *adc    = params->adc;
    QueueHandle_t                queue  = params->sample_queue;

    TaskState_t          state = STATE_INIT;
    MCP3465R_CalResult_t cal_result;
    MCP3465R_CalStatus_t cal_status;
    TickType_t           last_wake = xTaskGetTickCount();

    for (;;) {
        uint32_t notif = 0;

        switch (state) {

        /* ---------------------------------------------------------- */
        case STATE_INIT:
            /* One-time device initialisation */
            cal_status = MCP3465R_Init(adc);
            if (cal_status != MCP3465R_CAL_OK) {
                state = STATE_ERROR;
                break;
            }
            state = STATE_CALIBRATE;
            break;

        /* ---------------------------------------------------------- */
        case STATE_CALIBRATE: {
            /* Run full auto calibration: offset then gain */
            cal_status = MCP3465R_AutoCalibrate(adc, &cal_result);

            if (cal_status != MCP3465R_CAL_OK) {
                /* Retry after 1 s — transient SPI error is possible */
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            }

            /* Publish a sentinel sample to notify consumers that
             * calibration finished (adc_code = 0, cal_active = true).  */
            MCP3465R_Sample_t sentinel = {
                .adc_code    = 0,
                .voltage_v   = 0.0f,
                .timestamp_ms = (uint32_t)(xTaskGetTickCount() *
                                           portTICK_PERIOD_MS),
                .cal_active  = true,
            };
            xQueueOverwrite(queue, &sentinel);

            /* Reset periodic wake reference so the first sample
             * fires immediately after calibration completes.         */
            last_wake = xTaskGetTickCount();
            state = STATE_MEASURE;
            break;
        }

        /* ---------------------------------------------------------- */
        case STATE_MEASURE: {
            /* Check for re-calibration request (non-blocking) */
            xTaskNotifyWait(0u, MCP3465R_TASK_NOTIF_RECAL,
                            &notif, 0u);
            if (notif & MCP3465R_TASK_NOTIF_RECAL) {
                state = STATE_CALIBRATE;
                break;
            }

            /* Acquire one calibrated sample */
            int32_t code = 0;
            cal_status = MCP3465R_ReadCalibratedSample(adc, &code);

            if (cal_status == MCP3465R_CAL_OK) {
                MCP3465R_Sample_t s;
                s.adc_code    = code;
                s.voltage_v   = MCP3465R_CodeToVoltage(code,
                                                        params->vref_v,
                                                        params->divider_ratio);
                s.timestamp_ms = (uint32_t)(xTaskGetTickCount() *
                                             portTICK_PERIOD_MS);
                s.cal_active   = false;

                /* Drop oldest if queue is full (non-blocking post) */
                if (xQueueSend(queue, &s, 0) != pdTRUE) {
                    MCP3465R_Sample_t discard;
                    xQueueReceive(queue, &discard, 0);
                    xQueueSend(queue, &s, 0);
                }
            }

            /* Sleep until next sample period */
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MCP3465R_SAMPLE_PERIOD_MS));
            break;
        }

        /* ---------------------------------------------------------- */
        case STATE_ERROR:
            /* Fatal — suspend self.  Watchdog or supervisor must recover. */
            vTaskSuspend(NULL);
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

BaseType_t MCP3465R_TaskCreate(const MCP3465R_TaskParams_t *params,
                                TaskHandle_t                *handle)
{
    TaskHandle_t h;
    BaseType_t   ret = xTaskCreate(mcp3465r_task_fn,
                                   "mcp3465r",
                                   MCP3465R_TASK_STACK_WORDS,
                                   (void *)params,
                                   MCP3465R_TASK_PRIORITY,
                                   &h);
    if (handle)
        *handle = h;

    return ret;
}

void MCP3465R_RequestRecalibration(TaskHandle_t task_handle)
{
    xTaskNotify(task_handle, MCP3465R_TASK_NOTIF_RECAL, eSetBits);
}
