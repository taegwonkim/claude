#include "app_main.h"
#include "app_config.h"
#include "measurement_msg.h"
#include "w25q40.h"
#include "esp32_at.h"
#include "pc_comm.h"
#include "fpga_link.h"
#include "status_led.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* ------------------------------------------------------------------------- */
/* 측정값 큐: FpgaLink_Poll() -> Esp32_Poll(). RTOS의 g_measQueueId(osMessageQueue,
 * 길이 APP_MEAS_QUEUE_LEN)를 고정 크기 원형 버퍼로 대체. 두 함수 모두 App_Run()의
 * 같은 super-loop 스레드에서만 호출되므로(ISR은 건드리지 않음) 락이 필요 없다. */
/* ------------------------------------------------------------------------- */
static MeasurementMsg_t s_measQueue[APP_MEAS_QUEUE_LEN];
static uint32_t s_measHead = 0U; /* 다음 push 위치 */
static uint32_t s_measTail = 0U; /* 다음 pop 위치 */
static uint32_t s_measCount = 0U;

bool App_PushMeasurement(const MeasurementMsg_t *msg)
{
    if (s_measCount >= APP_MEAS_QUEUE_LEN) {
        return false; /* 큐 가득 참 - 드롭 (RTOS 버전과 동일 정책) */
    }
    s_measQueue[s_measHead] = *msg;
    s_measHead = (s_measHead + 1U) % APP_MEAS_QUEUE_LEN;
    s_measCount++;
    return true;
}

static bool App_PopMeasurement(MeasurementMsg_t *msg)
{
    if (s_measCount == 0U) {
        return false;
    }
    *msg = s_measQueue[s_measTail];
    s_measTail = (s_measTail + 1U) % APP_MEAS_QUEUE_LEN;
    s_measCount--;
    return true;
}

/* ------------------------------------------------------------------------- */
/* 설정 저장 요청: PcComm_Poll() -> Config_Poll(). 단일 슬롯(동시에 1건만 대기). */
/* ------------------------------------------------------------------------- */
static volatile bool s_cfgSavePending = false;
static NetConfig_t s_cfgSaveData;

bool App_RequestConfigSave(const NetConfig_t *cfg)
{
    if (s_cfgSavePending) {
        return false; /* 이전 저장 요청이 아직 처리 전 (RTOS 버전의 ERR,QUEUE_FULL에 대응) */
    }
    s_cfgSaveData = *cfg;
    s_cfgSavePending = true;
    return true;
}

/* ------------------------------------------------------------------------- */
/* WiFi (재)연결 요청: App_Init()/Config_Poll() -> Esp32_Poll(). 단일 슬롯. */
/* ------------------------------------------------------------------------- */
static volatile bool s_wifiConnectPending = false;
static NetConfig_t s_wifiConnectData;

static void RequestWifiConnect(const NetConfig_t *cfg)
{
    s_wifiConnectData = *cfg;
    s_wifiConnectPending = true; /* 처리 전에 재요청되면 최신 값으로 덮어씀 */
}

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

/* ------------------------------------------------------------------------- */
/* Esp32_Poll(): 원래 Esp32TaskBody의 for(;;) 루프 1회분. RTOS 버전의
 * osMessageQueueGet(g_measQueueId, ..., 100U)처럼 "새 항목을 최대 100ms 대기"하는 블로킹은
 * super-loop 모델에서는 다른 폴링을 멈추게 되므로, 있으면 처리하고 없으면 즉시 넘어가는
 * 논블로킹 방식으로 바꿨다(측정값 처리가 살짝 늦어질 뿐 유실되지는 않음 - 큐가 흡수). */
/* ------------------------------------------------------------------------- */
static NetConfig_t s_lastCfg;
static bool s_haveCfg = false;
static uint32_t s_lastRetry = 0U;
static uint32_t s_lastStatusBroadcast = 0U;
static Esp32_LinkState_t s_prevLinkState = ESP32_LINK_DOWN;

static void Esp32_Poll(void)
{
    if (s_wifiConnectPending) {
        s_wifiConnectPending = false;
        s_lastCfg = s_wifiConnectData;
        s_haveCfg = true;

        if (Esp32_GetLinkState() == ESP32_TCP_UP) {
            Esp32_TcpClose();
        }
        if (s_lastCfg.ssid[0] != '\0') {
            if (Esp32_ConnectWifi(&s_lastCfg)) {
                Esp32_TcpConnect(&s_lastCfg);
            }
        }
        s_lastRetry = HAL_GetTick(); /* 방금 시도했으니 재시도 타이머도 리셋 */
    }

    {
        MeasurementMsg_t msg;
        if (App_PopMeasurement(&msg)) {
            if (Esp32_GetLinkState() == ESP32_TCP_UP) {
                char payload[APP_PC_LINE_MAX];
                int len = MeasurementMsg_BuildDataLine(payload, sizeof(payload), &msg);
                if (len > 0) {
                    Esp32_TcpSend((uint8_t *)payload, (uint16_t)len);
                }
            }
            /* TCP 미연결 시 서버 전송은 스킵됨(PC 미러는 FpgaLink_Poll이 별도로 이미 처리) */
        }
    }

    Esp32_PollUrc(); /* WIFI DISCONNECT / CLOSED 등 URC를 반영해 링크 상태 갱신 */

    {
        Esp32_LinkState_t cur_state = Esp32_GetLinkState();

        ReportLinkStateChange(s_prevLinkState, cur_state);
        s_prevLinkState = cur_state;

        /* WiFi 또는 TCP가 끊긴 상태로 감지되면, 저장된 마지막 설정(s_lastCfg)으로
         * AT 커맨드(AT+CWJAP / AT+CIPSTART)를 통해 재접속을 시도한다. */
        if (s_haveCfg && cur_state != ESP32_TCP_UP &&
            (HAL_GetTick() - s_lastRetry) >= APP_ESP32_RECONNECT_RETRY_MS) {
            s_lastRetry = HAL_GetTick();
            (void)Esp32_TryReconnect(&s_lastCfg, cur_state);
        }

        /* PC로 ESP32 상태(STATUS,<num>)를 주기적으로 브로드캐스트한다 (docs/프로토콜_명세.md §1). */
        if ((HAL_GetTick() - s_lastStatusBroadcast) >= APP_ESP32_STATUS_BROADCAST_MS) {
            char status_buf[32];
            s_lastStatusBroadcast = HAL_GetTick();
            snprintf(status_buf, sizeof(status_buf), "STATUS,%d", (int)cur_state);
            PcComm_BroadcastFrame(status_buf);
        }
    }
}

/* Config_Poll(): 원래 ConfigTaskBody의 for(;;) 루프 1회분. */
static void Config_Poll(void)
{
    if (!s_cfgSavePending) {
        return;
    }
    s_cfgSavePending = false;

    /* NOTE: pc_comm.c는 요청 접수 시점에 이미 "SAVED"를 응답한다. 플래시 쓰기 실패까지
     * PC에 알리려면 별도 결과 콜백으로 확장 필요(본 구현 범위 밖, RTOS 버전과 동일). */
    if (NetConfig_Save(&s_cfgSaveData)) {
        RequestWifiConnect(&s_cfgSaveData);
    }
}

void App_Init(void)
{
    NetConfig_t cfg;

    /* SPI2/USART1/USART2/USART3/USB는 이 시점 이전에 main()에서 이미 초기화되어 있음 */
    (void)W25Q40_Init();
    if (!NetConfig_Load(&cfg)) {
        NetConfig_SetDefaults(&cfg);
    }

    PcComm_Init(&cfg);
    FpgaLink_Init();
    Esp32_Init();

    Esp32_HardReset(); /* ESP32_NRST(PA8) 리셋 펄스 + 부팅 대기, 이전 세션 상태 정리 */
    {
        uint32_t probe_fail_count = 0U;

        /* RTOS가 없으므로 이 루프가 성공할 때까지 App_Run()은 아직 한 번도 돌지 않는다 —
         * 즉 ESP32가 응답할 때까지 PC 커맨드/FPGA 트리거는 전혀 처리되지 않는다
         * (firmware-no-rtos/README.md "RTOS 미사용의 결과" 참고). */
        while (!Esp32_Probe()) {
            HAL_Delay(1000U);
            probe_fail_count++;
            if (probe_fail_count >= 5U) { /* 5회 연속 무응답이면 하드 리셋 재시도 */
                probe_fail_count = 0U;
                Esp32_HardReset();
            }
        }
    }

    /* 부팅 시 로드한 설정으로 최초 WiFi 연결을 요청 (App_Run()의 Esp32_Poll()이 소비) */
    RequestWifiConnect(&cfg);
}

void App_Run(void)
{
    FpgaLink_Poll();
    Esp32_Poll();
    PcComm_Poll();
    Config_Poll();
    StatusLed_HeartbeatTick();
}
