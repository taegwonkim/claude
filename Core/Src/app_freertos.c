#include "app_freertos.h"
#include "app_config.h"
#include "measurement_msg.h"
#include "w25q40.h"
#include "esp32_at.h"
#include "pc_comm.h"
#include "fpga_link.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdbool.h>

osMessageQueueId_t g_cfgEventQueueId;
osMessageQueueId_t g_wifiEventQueueId;
osMessageQueueId_t g_measQueueId;

/* WiFi 재접속 재시도 최소 간격 (LINK_DOWN/WIFI_UP 상태에서 자동 재시도) */
#define ESP32_RECONNECT_RETRY_MS (5000U)

static void Esp32TaskBody(void *argument)
{
    (void)argument;
    NetConfig_t cfg;
    NetConfig_t last_cfg;
    bool have_cfg = false;
    MeasurementMsg_t msg;
    uint32_t last_retry = 0U;

    Esp32_Init();
    osDelay(2000U); /* ESP32 모듈 자체 부팅 시간 대기 */
    while (!Esp32_Probe()) {
        osDelay(1000U);
    }

    for (;;) {
        if (osMessageQueueGet(g_wifiEventQueueId, &cfg, NULL, 0U) == osOK) {
            last_cfg = cfg;
            have_cfg = true;

            if (Esp32_GetLinkState() == ESP32_TCP_UP) {
                Esp32_TcpClose();
            }
            if (last_cfg.ssid[0] != '\0') {
                if (Esp32_ConnectWifi(&last_cfg)) {
                    Esp32_TcpConnect(&last_cfg);
                }
            }
        }

        if (osMessageQueueGet(g_measQueueId, &msg, NULL, 100U) == osOK) {
            if (Esp32_GetLinkState() == ESP32_TCP_UP) {
                char payload[APP_PC_LINE_MAX];
                int len = MeasurementMsg_BuildDataLine(payload, sizeof(payload), &msg);
                if (len > 0) {
                    Esp32_TcpSend((uint8_t *)payload, (uint16_t)len);
                }
            }
            /* TCP 미연결 시 서버 전송은 스킵됨(PC 미러는 FpgaLink_Task가 별도로 이미 처리) */
        }

        Esp32_PollUrc();

        if (have_cfg && (HAL_GetTick() - last_retry) > ESP32_RECONNECT_RETRY_MS) {
            Esp32_LinkState_t st = Esp32_GetLinkState();
            if (st == ESP32_LINK_DOWN) {
                last_retry = HAL_GetTick();
                if (Esp32_ConnectWifi(&last_cfg)) {
                    Esp32_TcpConnect(&last_cfg);
                }
            } else if (st == ESP32_WIFI_UP) {
                last_retry = HAL_GetTick();
                Esp32_TcpConnect(&last_cfg);
            }
        }
    }
}

static void ConfigTaskBody(void *argument)
{
    (void)argument;
    NetConfig_t cfg;

    for (;;) {
        if (osMessageQueueGet(g_cfgEventQueueId, &cfg, NULL, osWaitForever) == osOK) {
            /* NOTE: pc_comm.c는 큐 적재 성공 시점에 이미 "SAVED"를 응답한다.
             * 플래시 쓰기 실패까지 PC에 알리려면 별도 결과 큐/콜백으로 확장 필요(본 구현 범위 밖). */
            if (NetConfig_Save(&cfg)) {
                osMessageQueuePut(g_wifiEventQueueId, &cfg, 0U, 0U);
            }
        }
    }
}

static osThreadId_t NewThread(osThreadFunc_t func, const char *name,
                               osPriority_t priority, uint32_t stack_words)
{
    osThreadAttr_t attr;

    memset(&attr, 0, sizeof(attr));
    attr.name = name;
    attr.priority = priority;
    attr.stack_size = stack_words * 4U; /* word -> byte */

    return osThreadNew(func, NULL, &attr);
}

void App_FreeRTOS_Init(void)
{
    NetConfig_t cfg;

    /* SPI2/USART1/USART2/USART3/USB는 이 시점(osKernelStart 이전)에 main()에서 이미 초기화되어 있음 */
    (void)W25Q40_Init();
    if (!NetConfig_Load(&cfg)) {
        NetConfig_SetDefaults(&cfg);
    }

    g_cfgEventQueueId  = osMessageQueueNew(APP_CFG_EVENT_QUEUE_LEN, sizeof(NetConfig_t), NULL);
    g_wifiEventQueueId = osMessageQueueNew(APP_WIFI_EVENT_QUEUE_LEN, sizeof(NetConfig_t), NULL);
    g_measQueueId      = osMessageQueueNew(APP_MEAS_QUEUE_LEN, sizeof(MeasurementMsg_t), NULL);

    PcComm_Init(&cfg);
    FpgaLink_Init();

    /* 부팅 시 로드한 설정으로 ESP32_Task가 최초 WiFi 연결을 시도하도록 요청 */
    osMessageQueuePut(g_wifiEventQueueId, &cfg, 0U, 0U);

    NewThread(FpgaLink_Task, "FPGA_Task", APP_PRIO_FPGA_TASK, APP_STACK_FPGA_TASK);
    NewThread(Esp32TaskBody, "ESP32_Task", APP_PRIO_ESP32_TASK, APP_STACK_ESP32_TASK);
    NewThread(PcComm_Task, "PCComm_Task", APP_PRIO_PCCOMM_TASK, APP_STACK_PCCOMM_TASK);
    NewThread(ConfigTaskBody, "Config_Task", APP_PRIO_CONFIG_TASK, APP_STACK_CONFIG_TASK);
}
