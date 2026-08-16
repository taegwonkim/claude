/*
 * main_usercode_reference.c
 *
 * STM32CubeIDE + STM32CubeMX로 생성된 프로젝트에서, USART1을 ESP32-C3와
 * 통신하는 용도로 이미 설정해 둔 상태(Connectivity > USART1, Mode =
 * Asynchronous, NVIC에서 USART1 global interrupt 활성화)라고 가정한다.
 *
 * 이 파일은 컴파일 대상이 아니다(전체가 #if 0으로 감싸져 있어 실수로
 * 빌드에 포함되어도 main()이 중복 정의되지 않는다). CubeMX가 자동
 * 생성한 실제 Core/Src/main.c를 열어, 아래와 동일한 이름의
 * "USER CODE BEGIN ..." / "USER CODE END ..." 사이에 내용만 옮겨
 * 넣으면 된다. Project > Generate Code를 다시 눌러도 이 마커 안의
 * 내용은 보존된다.
 *
 * huart1 핸들은 CubeMX가 Core/Src/usart.c / Core/Inc/usart.h에 이미
 * 선언해 두었고 main.c는 usart.h를 include하고 있으므로, 별도의
 * extern 선언이 필요 없다.
 */

#if 0

/* USER CODE BEGIN Includes */
#include "wifi_manager.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
static wifi_ap_config_t     s_ap_cfg = {
    .ssid     = "MyHomeAP",
    .password = "MyAPPassword123",
};

/* DHCP를 쓰려면 dhcp_enable = true로 바꾸면 되고,
 * 이 경우 ip/gateway/netmask 필드는 무시된다. */
static wifi_ip_config_t     s_ip_cfg = {
    .dhcp_enable = false,
    .ip          = "192.168.0.50",
    .gateway     = "192.168.0.1",
    .netmask     = "255.255.255.0",
};

static wifi_server_config_t s_server_cfg = {
    .ip   = "192.168.0.100",
    .port = 8080,
};
/* USER CODE END PV */

int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  WiFi_Manager_Init(&huart1, &s_ap_cfg, &s_ip_cfg, &s_server_cfg);
  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    WiFi_Manager_Process();

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
  /* USER CODE END 3 */
}

#endif /* #if 0 */
