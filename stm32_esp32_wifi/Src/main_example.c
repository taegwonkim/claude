/*
 * main_example.c
 *
 * STM32CubeIDE/CubeMX로 생성된 프로젝트에 이 라이브러리를 통합하는 예시.
 * USART1이 ESP32-C3-WROOM과 연결되어 있다고 가정한다(CubeMX에서
 * USART1을 Asynchronous, 인터럽트(NVIC) 활성화 상태로 미리 생성해 둘 것).
 *
 * 실제 프로젝트에서는 이 파일의 내용을 main.c의 해당 위치(USER CODE 영역)에
 * 옮겨 넣으면 된다.
 */

#include <string.h>
#include "main.h"
#include "wifi_manager.h"
#include "esp32_at.h"

extern UART_HandleTypeDef huart1; /* CubeMX가 생성한 핸들 */

/* HAL_UART_RxCpltCallback은 프로젝트 전체에서 하나만 존재해야 하므로,
 * 다른 UART를 함께 쓴다면 huart->Instance로 분기해서 각 모듈에 전달한다. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    ESP32_AT_UART_RxCpltCallback(huart);
}

static void wifi_config_load(wifi_ap_config_t *ap,
                              wifi_ip_config_t *ip,
                              wifi_server_config_t *server)
{
    /* AP(공유기) 설정 */
    strcpy(ap->ssid, "MyHomeAP");
    strcpy(ap->password, "MyAPPassword123");

    /* DHCP 설정: false면 아래 정적 IP를 사용 */
    ip->dhcp_enable = false;
    strcpy(ip->ip, "192.168.0.50");
    strcpy(ip->gateway, "192.168.0.1");
    strcpy(ip->netmask, "255.255.255.0");

    /* 접속할 TCP 서버 */
    strcpy(server->ip, "192.168.0.100");
    server->port = 8080;
}

void App_Main(void)
{
    wifi_ap_config_t ap_cfg;
    wifi_ip_config_t ip_cfg;
    wifi_server_config_t server_cfg;

    wifi_config_load(&ap_cfg, &ip_cfg, &server_cfg);

    WiFi_Manager_Init(&huart1, &ap_cfg, &ip_cfg, &server_cfg);

    wifi_state_t last_state = (wifi_state_t)-1;

    for (;;) {
        WiFi_Manager_Process();

        wifi_state_t state = WiFi_Manager_GetState();
        if (state != last_state) {
            last_state = state;
            /* 필요하다면 여기서 LED 표시나 로그 출력으로 상태 전이를 확인한다 */
        }

        if (WiFi_Manager_IsConnected()) {
            static uint32_t last_send_tick = 0;
            if ((HAL_GetTick() - last_send_tick) >= 1000) {
                last_send_tick = HAL_GetTick();
                const uint8_t payload[] = "hello\r\n";
                WiFi_Manager_Send(payload, sizeof(payload) - 1);
            }
        }

        HAL_Delay(10); /* 상태 머신 폴링 주기 */
    }
}
