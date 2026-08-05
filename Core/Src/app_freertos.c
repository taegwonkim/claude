#include "app_freertos.h"
#include "app_config.h"
#include "measurement_msg.h"
#include "w25q40.h"
#include "esp32_at.h"
#include "pc_comm.h"
#include "fpga_link.h"
#include "status_led.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdbool.h>

osMessageQueueId_t g_cfgEventQueueId;
osMessageQueueId_t g_wifiEventQueueId;
osMessageQueueId_t g_measQueueId;

/* WiFi/TCP 끊김 감지 후 재접속 재시도 최소 간격 */
#define ESP32_RECONNECT_RETRY_MS (5000U)

/* 링크 상태 변화(끊김/재연결)를 PC(USART3+USB)에 이벤트 라인으로 알리고, LED_WIFI(PC14)를 갱신한다. */
static void ReportLinkStateChange(Esp32_LinkState_t prev, Esp32_LinkState_t cur)
{
    if (cur == prev) {
        return;
    }

    if (cur == ESP32_LINK_DOWN) {
        PcComm_BroadcastFrame("EVENT,WIFI_DISCONNECTED");
    } else if (cur == ESP32_WIFI_UP) {
        PcComm_BroadcastFrame((prev == ESP32_TCP_UP) ? "EVENT,TCP_CLOSED" : "EVENT,WIFI_CONNECTED");
    } else if (cur == ESP32_TCP_UP) {
        PcComm_BroadcastFrame("EVENT,TCP_CONNECTED");
    }

    StatusLed_SetWifi(cur == ESP32_TCP_UP);
}

/* cur_state에 맞춰 필요한 단계부터 AT 커맨드로 재접속을 시도한다.
 * LINK_DOWN: AT+CWJAP부터 다시 (WiFi 재접속) -> 성공 시 TCP도 재연결.
 * WIFI_UP(=WiFi는 붙어있는데 TCP만 끊김): AT+CIPSTART만 재시도. */
static bool Esp32_TryReconnect(const NetConfig_t *cfg, Esp32_LinkState_t cur_state)
{
    bool ok = true;

    if (cur_state == ESP32_LINK_DOWN) {
        ok = Esp32_ConnectWifi(cfg);
        if (ok) {
            ok = Esp32_TcpConnect(cfg);
        }
    } else if (cur_state == ESP32_WIFI_UP) {
        ok = Esp32_TcpConnect(cfg);
    }
    return ok;
}

static void Esp32TaskBody(void *argument)
{
    (void)argument;
    NetConfig_t cfg;
    NetConfig_t last_cfg;
    bool have_cfg = false;
    MeasurementMsg_t msg;
    uint32_t last_retry = 0U;
    Esp32_LinkState_t prev_state = ESP32_LINK_DOWN;

    Esp32_Init();
    Esp32_HardReset(); /* ESP32_NRST(PA8) 리셋 펄스 + 부팅 대기, 이전 세션 상태 정리 */
    {
        uint32_t probe_fail_count = 0U;

        while (!Esp32_Probe()) {
            osDelay(1000U);
            probe_fail_count++;
            if (probe_fail_count >= 5U) { /* 5회 연속 무응답이면 하드 리셋 재시도 */
                probe_fail_count = 0U;
                Esp32_HardReset();
            }
        }
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
            last_retry = HAL_GetTick(); /* 방금 시도했으니 재시도 타이머도 리셋 */
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

        Esp32_PollUrc(); /* WIFI DISCONNECT / CLOSED 등 URC를 반영해 링크 상태 갱신 */

        {
            Esp32_LinkState_t cur_state = Esp32_GetLinkState();

            ReportLinkStateChange(prev_state, cur_state);
            prev_state = cur_state;

            /* WiFi 또는 TCP가 끊긴 상태로 감지되면, 저장된 마지막 설정(last_cfg)으로
             * AT 커맨드(AT+CWJAP / AT+CIPSTART)를 통해 재접속을 시도한다. */
            if (have_cfg && cur_state != ESP32_TCP_UP &&
                (HAL_GetTick() - last_retry) >= ESP32_RECONNECT_RETRY_MS) {
                last_retry = HAL_GetTick();
                (void)Esp32_TryReconnect(&last_cfg, cur_state);
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
