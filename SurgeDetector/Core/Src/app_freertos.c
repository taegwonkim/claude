/**
 * app_freertos.c — CubeMX가 생성하는 Core/Src/freertos.c에 해당.
 * MX_FREERTOS_Init()에서 defaultTask만 생성하고, 나머지 모듈(WiFi/FPGA/PcComm)의
 * 태스크/큐/세마포어는 각 모듈의 App_xxx_Init()이 스스로 생성하도록 위임한다
 * (README.md 3.8.1 참조).
 */
#include "cmsis_os2.h"
#include "main.h"
#include "app_config.h"
#include "app_wifi.h"
#include "app_fpga.h"
#include "app_pccomm.h"

static osThreadId_t   defaultTaskHandle;
static osTimerId_t    tmrHeartbeat;
static osTimerId_t    tmrWifiWatchdog;

static void StartDefaultTask(void *argument);
static void Heartbeat_Callback(void *argument);
static void WifiWatchdog_Callback(void *argument);

void MX_FREERTOS_Init(void)
{
    const osThreadAttr_t defaultTask_attributes = {
        .name = "defaultTask",
        .priority = (osPriority_t) osPriorityNormal,
        .stack_size = 256 * 4,
    };
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    const osTimerAttr_t hbAttr = { .name = "tmrHeartbeat" };
    tmrHeartbeat = osTimerNew(Heartbeat_Callback, osTimerPeriodic, NULL, &hbAttr);

    const osTimerAttr_t wdAttr = { .name = "tmrWifiWatchdog" };
    tmrWifiWatchdog = osTimerNew(WifiWatchdog_Callback, osTimerPeriodic, NULL, &wdAttr);
}

static void Heartbeat_Callback(void *argument)
{
    (void)argument;
    HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
}

static void WifiWatchdog_Callback(void *argument)
{
    (void)argument;
    uint32_t flags = osEventFlagsGet(egSysStatus);

    HAL_GPIO_WritePin(LED_WIFI_GPIO_Port, LED_WIFI_Pin,
                       (flags & EVT_SERVER_CONNECTED) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    if ((flags & EVT_SERVER_CONNECTED) == 0u)
    {
        WifiCmdMsg_t m = { .id = WIFI_CMD_FORCE_RECONNECT };
        /* wifiTask가 자체 재시도 루프를 돌고 있을 때는 큐가 이미 비어있지 않을 수
         * 있으므로 non-blocking put 실패는 무시(다음 주기에 재시도) */
        osMessageQueuePut(qWifiCmd, &m, 0, 0);
    }
}

/**
 * StartDefaultTask — 시스템 초기화 순서를 보장하는 진입점.
 * 1) 외부 Flash + 설정 로드 (EVT_FLASH_READY, EVT_CONFIG_LOADED 셋)
 * 2) 나머지 모듈 태스크 생성 (모두 EVT_CONFIG_LOADED 대기 후 동작 시작)
 * 3) 하트비트/워치독 타이머 시작
 * 4) 이후 defaultTask는 유휴 루프(추가로 IWDG refresh 등 시스템 감시 용도로 확장 가능)
 */
static void StartDefaultTask(void *argument)
{
    (void)argument;

    App_Config_Init();     /* SPI Flash + mutexFlash + egSysStatus 생성/초기화 */

    App_WiFi_Init();       /* wifiTask, qFpgaToWifi, qWifiCmd, mutexWifiUart, semUart1Event */
    App_FPGA_Init();       /* fpgaTask, semFpgaTrigger, semUart2RxCplt */
    App_PcComm_Init();     /* pcUartTask, pcUsbTask, pcDataPushTask, qFpgaToPc, semUart3Event */

    osTimerStart(tmrHeartbeat, 500);
    osTimerStart(tmrWifiWatchdog, 5000);

    for (;;)
    {
        /* 향후 IWDG_Refresh() 등 시스템 레벨 감시 로직 추가 지점 */
        osDelay(1000);
    }
}
