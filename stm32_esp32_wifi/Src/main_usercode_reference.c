/*
 * main_usercode_reference.c
 *
 * STM32CubeIDE + STM32CubeMX로 생성된 프로젝트에서 아래 페리페럴이 이미
 * 설정되어 있다고 가정한다(설정 방법은 README 참고):
 *   - USART1: ESP32-C3와 AT 명령 통신 (Asynchronous, NVIC 인터럽트 활성화)
 *   - USART2: 외부 ADC 장비로부터 데이터 수신 (Asynchronous, NVIC 인터럽트 활성화)
 *   - USART3: PC로 데이터 송신 (Asynchronous)
 *   - TIM6: 1Hz 주기 인터럽트 (NVIC 인터럽트 활성화, 카운터는 아래
 *     USER CODE BEGIN 2에서 직접 시작)
 *
 * 이 파일은 컴파일 대상이 아니다(전체가 #if 0으로 감싸져 있어 실수로
 * 빌드에 포함되어도 main()이 중복 정의되지 않는다). CubeMX가 자동
 * 생성한 실제 Core/Src/main.c를 열어, 아래와 동일한 이름의
 * "USER CODE BEGIN ..." / "USER CODE END ..." 사이에 내용만 옮겨
 * 넣으면 된다. Project > Generate Code를 다시 눌러도 이 마커 안의
 * 내용은 보존된다.
 *
 * huart1/huart2/huart3, htim6 핸들은 CubeMX가 usart.h/tim.h에 이미
 * extern 선언해 두었고 main.c는 그 헤더들을 include하고 있으므로,
 * 별도의 extern 선언이 필요 없다.
 *
 * 동작 개요:
 *   - ADC_UART_Init()이 USART2 인터럽트 수신을 시작한다. 이후 ADC 값은
 *     WiFi 상태와 무관하게 계속 최신으로 갱신된다.
 *   - TIM6이 1초마다 인터럽트를 일으켜 DataReporter_TimerTick()을 호출한다
 *     (tim_usercode_reference.c 참고). 여기서 최신 ADC 값을 USART3로 PC에
 *     즉시 전송하고, 서버로 보낼 페이로드를 준비해 둔다. 이 경로는 메인
 *     루프(WiFi 재접속으로 최대 20초까지 블로킹될 수 있음)와 완전히
 *     독립적이므로, WiFi가 끊겨 있어도 PC로의 1초 주기 전송은 계속된다.
 *   - 메인 루프의 DataReporter_Process()는 위에서 준비된 페이로드를
 *     WiFi가 연결되어 있을 때만 서버로 전송한다(best-effort). 연결이
 *     끊겨 있으면 그 틱은 건너뛰고 다음 틱을 기다릴 뿐, ADC 수신/PC
 *     전송에는 영향을 주지 않는다.
 */

#if 0

/* USER CODE BEGIN Includes */
#include "wifi_manager.h"
#include "adc_uart.h"
#include "data_reporter.h"
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
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_TIM6_Init();

  /* USER CODE BEGIN 2 */
  WiFi_Manager_Init(&huart1, &s_ap_cfg, &s_ip_cfg, &s_server_cfg);
  ADC_UART_Init(&huart2);
  DataReporter_Init(&huart3);
  HAL_TIM_Base_Start_IT(&htim6); /* CubeMX는 초기화만 하고 시작은 안 하므로 직접 호출 */
  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    WiFi_Manager_Process();   /* AP/서버 접속 및 재접속 상태 머신 */
    DataReporter_Process();   /* 준비된 데이터가 있으면 서버로 best-effort 전송 */

    HAL_Delay(10); /* 상태 머신 폴링 주기. PC 전송(1Hz)은 TIM6 인터럽트가
                     * 별도로 처리하므로 이 지연과 무관하게 항상 나간다. */
  }
  /* USER CODE END 3 */
}

#endif /* #if 0 */
