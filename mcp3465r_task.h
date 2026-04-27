/**
 * FreeRTOS calibration task for MCP3465R.
 *
 * Task life-cycle:
 *   1. On startup: run MCP3465R_AutoCalibrate() once.
 *   2. Enter measurement loop: read + publish samples at SAMPLE_PERIOD_MS.
 *   3. Accept re-calibration requests via task notifications:
 *        MCP3465R_TASK_NOTIF_RECAL  → repeat full auto-calibration
 *
 * Measurement results are placed in a FreeRTOS queue that the
 * application reads from.
 */

#ifndef MCP3465R_TASK_H
#define MCP3465R_TASK_H

#include <stdint.h>
#include "mcp3465r_calibration.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Task configuration                                                  */
/* ------------------------------------------------------------------ */

#define MCP3465R_TASK_STACK_WORDS   256u
#define MCP3465R_TASK_PRIORITY      (tskIDLE_PRIORITY + 2u)
#define MCP3465R_SAMPLE_PERIOD_MS   10u     /* sampling interval         */
#define MCP3465R_QUEUE_LEN          16u     /* measurement queue depth   */

/* Task notification bits */
#define MCP3465R_TASK_NOTIF_RECAL   (1u << 0)   /* request re-calibration */

/* ------------------------------------------------------------------ */
/*  Measurement data type placed in the queue                           */
/* ------------------------------------------------------------------ */

typedef struct {
    int32_t  adc_code;      /* calibrated 24-bit signed ADC code         */
    float    voltage_v;     /* computed input voltage [V] (0–5 V range)  */
    uint32_t timestamp_ms;  /* xTaskGetTickCount() → ms at sample time   */
    bool     cal_active;    /* true while calibration was running         */
} MCP3465R_Sample_t;

/* ------------------------------------------------------------------ */
/*  Task parameters (fill before calling MCP3465R_TaskCreate)          */
/* ------------------------------------------------------------------ */

typedef struct {
    MCP3465R_Handle_t  *adc;           /* initialised ADC handle          */
    QueueHandle_t       sample_queue;  /* output queue for measurements   */
    float               vref_v;        /* external VREF voltage [V]       */
    float               divider_ratio; /* R2/(R1+R2) of input divider     */
} MCP3465R_TaskParams_t;

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

/**
 * Create the calibration + measurement FreeRTOS task.
 *
 * @param params  Pointer that must remain valid for the task lifetime.
 * @param handle  Receives the created task handle (may be NULL).
 * @return pdPASS on success.
 */
BaseType_t MCP3465R_TaskCreate(const MCP3465R_TaskParams_t *params,
                                TaskHandle_t                *handle);

/**
 * Request a re-calibration from any task / ISR context.
 * Non-blocking.  Calibration runs on the next loop iteration.
 */
void MCP3465R_RequestRecalibration(TaskHandle_t task_handle);

#ifdef __cplusplus
}
#endif
#endif /* MCP3465R_TASK_H */
