/*
 * main.c
 *
 * STM32L562RCT6 + ESP32-C3(AT 펌웨어) 예제.
 *   - USART1(PA9/PA10, 115200bps) : ESP32-C3 와 통신
 *   - USART2(PA2/PA3,  115200bps) : 디버그 로그 출력용 (보드에 맞게 UART/핀 조정)
 *
 * CubeMX 로 프로젝트를 생성한 경우, 아래에서
 *   /* USER CODE BEGIN xxx */  ~  /* USER CODE END xxx */
 * 로 표시된 부분만 각자의 프로젝트에 옮겨 넣으면 된다. 그 외(SystemClock_Config,
 * MX_xxx_Init 등)는 CubeMX가 .ioc 설정에 맞춰 자동 생성해준다.
 *
 * 이 예제는 STM32L5의 TrustZone(TZEN)을 비활성화한 일반(Non-TrustZone)
 * 프로젝트를 기준으로 한다. TrustZone을 켠 경우 Secure/Non-secure 프로젝트
 * 분리, GTZC 설정 등이 추가로 필요하며 이 예제 범위를 벗어난다.
 */

#include "main.h"
#include "esp32_at.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
static void OnEsp32Data(const uint8_t *data, uint16_t len);
static void OnEsp32State(ESP32_State_t state);
static void DebugLog(const char *msg);
/* USER CODE END PFP */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN 2 */
    DebugLog("ESP32-C3 AT 데모 시작\r\n");

    ESP32_Init(&huart1);
    ESP32_SetWiFiCredentials("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD");
    ESP32_SetServer("192.168.0.10", 8000); /* 서버 PC의 IP, 포트로 교체 */
    ESP32_SetDataCallback(OnEsp32Data);
    ESP32_SetStateCallback(OnEsp32State);

    uint32_t last_send_tick = 0;
    uint32_t seq = 0;
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN WHILE */
        ESP32_Process(); /* 논블로킹 상태 머신: 연결/재접속을 계속 진행시켜줌 */

        /* 연결된 상태에서만 1초마다 예시 데이터를 서버로 전송 */
        if (ESP32_GetState() == ESP32_STATE_TCP_CONNECTED &&
            (HAL_GetTick() - last_send_tick) >= 1000) {

            char payload[48];
            int n = snprintf(payload, sizeof(payload), "seq=%lu,tick=%lu\n",
                              (unsigned long)seq++, (unsigned long)HAL_GetTick());

            HAL_StatusTypeDef st = ESP32_Send((uint8_t *)payload, (uint16_t)n);
            if (st != HAL_OK) {
                DebugLog("전송 실패, 재접속 로직이 처리할 예정\r\n");
            }
            last_send_tick = HAL_GetTick();
        }
        /* USER CODE END WHILE */
    }
}

/* USER CODE BEGIN 4 */

/* 서버로부터 데이터를 받을 때마다 ESP32_Process() 내부(파서)에서 호출됨 */
static void OnEsp32Data(const uint8_t *data, uint16_t len)
{
    /* 데모: 받은 내용을 그대로 디버그 UART(USART2)로 에코 */
    HAL_UART_Transmit(&huart2, (uint8_t *)"[RX] ", 5, 50);
    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 100);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 50);
}

/* 상태가 바뀔 때마다 호출됨: LED 표시, 로그 출력 등에 활용 */
static void OnEsp32State(ESP32_State_t state)
{
    switch (state) {
    case ESP32_STATE_WIFI_CONNECTING: {
        char msg[48];
        snprintf(msg, sizeof(msg), "[STATE] MAC=%s, Wi-Fi 접속 시도\r\n", ESP32_GetMacAddress());
        DebugLog(msg);
        break;
    }
    case ESP32_STATE_WIFI_CONNECTED:   DebugLog("[STATE] Wi-Fi 접속 완료\r\n"); break;
    case ESP32_STATE_TCP_CONNECTING:   DebugLog("[STATE] 서버 접속 시도\r\n"); break;
    case ESP32_STATE_TCP_CONNECTED:    DebugLog("[STATE] 서버 접속 완료, 통신 가능\r\n"); break;
    case ESP32_STATE_LINK_DOWN:        DebugLog("[STATE] 연결 끊김 감지\r\n"); break;
    case ESP32_STATE_RECONNECT_WAIT:   DebugLog("[STATE] 재접속 대기 중(백오프)\r\n"); break;
    case ESP32_STATE_FATAL_ERROR:      DebugLog("[STATE] 모듈 응답 없음 - 배선/전원 확인 필요\r\n"); break;
    default: break;
    }
}

static void DebugLog(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)strlen(msg), 100);
}

/* CubeMX가 UART 인터럽트 수신 완료 시 자동으로 호출하는 콜백.
 * ESP32 쪽 UART(huart1)에 대한 처리를 드라이버로 위임한다. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    ESP32_UART_RxCpltCallback(huart);
}

/* USER CODE END 4 */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* MSI 4MHz -> PLL(x55/2) -> 110MHz SYSCLK (STM32L5 최대 클럭) */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6; /* 4MHz */
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 55;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                 | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    /* ESP32-C3 AT 펌웨어 기본 통신 속도 */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK) {
        Error_Handler();
    }

    /* 수신 인터럽트(HAL_UART_Receive_IT)를 쓰므로 NVIC에 등록 */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    /* 디버그 로그용 UART. 실제 사용 보드의 VCP/디버그 UART 번호·핀에 맞게 조정 */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* STM32L5는 F1과 달리 AFIO 리매핑이 없고, 핀마다 GPIO_InitTypeDef.Alternate
     * 로 직접 AF(Alternate Function) 번호를 지정한다. USART1/2 모두 AF7. */

    /* PA9 = USART1_TX, PA10 = USART1_RX */
    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA2 = USART2_TX, PA3 = USART2_RX (보드에 맞게 조정) */
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
