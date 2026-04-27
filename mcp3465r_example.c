/**
 * Example: MCP3465R calibration + measurement on STM32L552 / FreeRTOS
 *
 * Hardware wiring:
 *   PA4  ─── SPI1_NSS  ──► MCP3465R  CS̄
 *   PA5  ─── SPI1_SCK  ──► MCP3465R  SCK
 *   PA6  ─── SPI1_MISO ◄── MCP3465R  SDO
 *   PA7  ─── SPI1_MOSI ──► MCP3465R  SDI
 *   3.0V ─────────────────► MCP3465R  REFIN+
 *   GND  ─────────────────► MCP3465R  REFIN−
 *
 * Input voltage (0–5 V) → resistor divider → AIN0:
 *   VIN (0–5 V) ──[2 kΩ]──┬──[3 kΩ]── GND
 *                           └──► AIN0 pin  (0–3.0 V)
 *
 * divider_ratio = 3000 / (2000 + 3000) = 0.60
 */

#include "mcp3465r_task.h"
#include "stm32l5xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* --- Peripheral handles (configured by CubeMX / STM32CubeIDE) ---- */
extern SPI_HandleTypeDef hspi1;

/* ------------------------------------------------------------------ */
/*  Application-level objects                                           */
/* ------------------------------------------------------------------ */

static MCP3465R_Handle_t      s_adc;
static MCP3465R_TaskParams_t  s_task_params;
static StaticQueue_t          s_queue_buf;
static MCP3465R_Sample_t      s_queue_storage[MCP3465R_QUEUE_LEN];
static QueueHandle_t          s_sample_queue;
static StaticSemaphore_t      s_spi_mutex_buf;
static TaskHandle_t           s_adc_task;

/* ------------------------------------------------------------------ */
/*  Consumer task: reads from queue and processes results              */
/* ------------------------------------------------------------------ */

static void consumer_task(void *pv)
{
    (void)pv;
    MCP3465R_Sample_t s;

    for (;;) {
        /* Block indefinitely until a sample arrives */
        if (xQueueReceive(s_sample_queue, &s, portMAX_DELAY) != pdTRUE)
            continue;

        if (s.cal_active) {
            /* Calibration just finished — optionally log to UART */
            /* printf("CAL done\r\n"); */
            continue;
        }

        /* Use s.voltage_v (0.0–5.0 V) or s.adc_code for further processing */
        float  v   = s.voltage_v;
        (void)v;    /* suppress unused-variable warning in this stub */

        /* Example: send over UART, store in buffer, drive a DAC, etc. */
    }
}

/* ------------------------------------------------------------------ */
/*  Application entry point (called from main() after HAL/clock init) */
/* ------------------------------------------------------------------ */

void App_Init(void)
{
    /* Static queue (no heap allocation) */
    s_sample_queue = xQueueCreateStatic(MCP3465R_QUEUE_LEN,
                                        sizeof(MCP3465R_Sample_t),
                                        (uint8_t *)s_queue_storage,
                                        &s_queue_buf);

    /* SPI bus mutex */
    s_adc.spi_mutex = xSemaphoreCreateMutexStatic(&s_spi_mutex_buf);

    /* Populate ADC handle */
    s_adc.hspi     = &hspi1;
    s_adc.cs_port  = GPIOA;
    s_adc.cs_pin   = GPIO_PIN_4;
    s_adc.dev_addr = MCP3465R_DEVICE_ADDR;   /* 0x01 */

    /* Task parameters */
    s_task_params.adc           = &s_adc;
    s_task_params.sample_queue  = s_sample_queue;
    s_task_params.vref_v        = 3.000f;   /* external VREF */
    s_task_params.divider_ratio = 0.60f;    /* 3k/(2k+3k) */

    /* Create the MCP3465R driver task (calibrates on startup) */
    MCP3465R_TaskCreate(&s_task_params, &s_adc_task);

    /* Create application consumer task */
    xTaskCreate(consumer_task, "consumer", 128, NULL,
                tskIDLE_PRIORITY + 1u, NULL);
}

/* ------------------------------------------------------------------ */
/*  Trigger re-calibration from anywhere (e.g. button ISR)            */
/* ------------------------------------------------------------------ */

void Button_IRQHandler(void)
{
    /* Safe to call from ISR context */
    MCP3465R_RequestRecalibration(s_adc_task);
}
