# STM32L562CET6 + ESP32-C3-WROOM(ESP-AT) WiFi 연결 & 주기적 데이터 전송

STM32L562CET6이 UART로 연결된 ESP32-C3-WROOM(ESP-AT 펌웨어)을 AT 명령으로
제어하여 AP 접속, TCP 서버 접속, DHCP on/off(정적 IP) 설정, 그리고 AP/서버
접속이 끊겼을 때 자동 재접속까지 수행하는 논블로킹 상태 머신입니다.

여기에 더해, 외부 ADC 장비를 USART2로 수신해 1초 주기로 서버(WiFi)와
PC(USART3)에 동시에 보고하는 기능이 포함되어 있습니다. **PC로의 보고는
WiFi 연결 상태와 무관하게 항상 수행**되고, 서버로의 보고는 WiFi가 연결되어
있을 때만 best-effort로 시도됩니다.

## 파일 구성

- `Inc/esp32_at.h`, `Src/esp32_at.c`
  UART 기반 저수준 AT 명령 송수신 드라이버. 1바이트 인터럽트 수신을 라인
  단위로 조립해 큐에 저장하고, 명령 전송 후 기대 응답("OK" 등)이 올 때까지
  타임아웃 기반으로 대기합니다.

- `Inc/wifi_manager.h`, `Src/wifi_manager.c`
  실제 연결 절차(AP 접속 → DHCP/정적 IP 설정 → 서버 접속)와 장애 감지·재접속
  로직을 담은 상태 머신. `WiFi_Manager_Process()`를 메인 루프에서 주기적으로
  호출하면 됩니다. `AT+CWJAP`(최대 20초), `AT+CIPSTART`(최대 10초) 같은
  명령은 응답이 올 때까지 해당 호출을 블로킹합니다.

- `Inc/adc_uart.h`, `Src/adc_uart.c`
  USART2로 들어오는 외부 ADC 데이터를 인터럽트로 수신해 가장 최근 값을
  보관. WiFi 상태와 완전히 독립적으로 동작합니다.

- `Inc/data_reporter.h`, `Src/data_reporter.c`
  1초마다(TIM 인터럽트) 최신 ADC 값을 USART3로 PC에 즉시 전송하고, 서버
  전송용 페이로드를 준비해 두는 모듈. 서버 전송 자체는 메인 루프가
  `WiFi_Manager_IsConnected()`를 확인한 뒤 시도합니다.

- `Src/main_usercode_reference.c`, `Src/usart_usercode_reference.c`,
  `Src/tim_usercode_reference.c`
  STM32CubeIDE/CubeMX가 생성한 `Core/Src/main.c`, `Core/Src/usart.c`,
  `Core/Src/tim.c`의 어느 "USER CODE" 구역에 무엇을 넣어야 하는지 보여주는
  참고용 파일. 전체가 `#if 0`으로 감싸져 있어 프로젝트에 그대로 추가해도
  컴파일 대상에서 빠지며, 내용만 복사해서 실제 파일에 옮겨 넣으면 된다.

## 왜 타이머 인터럽트로 PC 전송을 분리했는가

`WiFi_Manager_Process()`는 AP 재접속 시도 중(`AT+CWJAP`) 한 번 호출로 최대
20초까지 블로킹될 수 있습니다. 만약 "1초마다 PC로 전송"하는 로직을 메인
루프 안에서 단순히 `HAL_GetTick()` 폴링으로 구현했다면, WiFi가 끊겨 재접속을
시도하는 20초 동안 PC 전송도 함께 멈춰버립니다 — "WiFi가 끊겨도 이 과정은
계속되어야 한다"는 요구사항과 정면으로 충돌합니다.

그래서 이 구조에서는:

1. **ADC 수신**(USART2)은 처음부터 인터럽트 기반이라 메인 루프 상태와
   무관하게 항상 최신 값이 갱신됩니다.
2. **PC 전송**(USART3)은 1Hz 하드웨어 타이머(TIM) 인터럽트
   `DataReporter_TimerTick()` 안에서 직접 수행합니다. 메인 루프가 무엇을
   하고 있든(WiFi 재접속 블로킹 포함) 정확히 1초마다 실행되어 PC 전송이
   끊기지 않습니다. 짧은(수 ms) 블로킹 전송을 인터럽트 안에서 쓰는 것은
   AT 명령 응답 대기(초 단위)와 달리 시간이 정해져 있어(`PC_TX_TIMEOUT_MS`)
   허용 가능한 절충입니다.
3. **서버 전송**(WiFi, USART1)은 응답 대기에 몇 초씩 걸릴 수 있어 인터럽트
   안에서 수행하면 안 됩니다. 그래서 타이머 인터럽트는 "보낼 데이터가
   준비됐다"는 플래그만 세우고, 실제 전송은 메인 루프의
   `DataReporter_Process()`가 `WiFi_Manager_IsConnected()`를 확인한 뒤
   시도합니다. 연결이 끊겨 있으면 그 틱의 서버 전송은 조용히 건너뛰고
   다음 틱을 기다립니다 — ADC 수신/PC 전송 경로에는 전혀 영향이 없습니다.

## 하드웨어 연결

이 문서는 다음 배선을 전제로 합니다.

| STM32L562CET6 | 상대 장치 | 용도 |
|---|---|---|
| USART1_TX / USART1_RX | ESP32-C3-WROOM RX / TX | WiFi AT 명령 |
| USART2_RX (필요시 TX도) | 외부 ADC 장비 TX (/ RX) | ADC 데이터 수신 |
| USART3_TX (필요시 RX도) | PC (USB-UART 브리지 등) RX (/ TX) | PC로 데이터 송신 |
| GND | 공통 | 공통 그라운드 필수 |
| 3.3V (충분한 전류 공급, ESP32는 순간 전류가 큼) | ESP32-C3 3V3 | 전원 |

### STM32CubeMX 설정 (.ioc)

1. **Connectivity → USART1** (ESP32)
   - Mode: `Asynchronous`
   - Baud Rate `115200`(ESP-AT 펌웨어 기본값), Word Length `8 Bits`,
     Parity `None`, Stop Bits `1`
   - NVIC 탭에서 `USART1 global interrupt` 체크
2. **Connectivity → USART2** (외부 ADC)
   - Mode: `Asynchronous` (ADC 쪽에서 응답을 받을 필요가 없다면 RX만
     써도 되지만, CubeMX 상에서는 보통 Asynchronous로 두고 TX는 그냥
     사용하지 않으면 됩니다)
   - Baud Rate: ADC 장비 사양에 맞춤
   - NVIC 탭에서 `USART2 global interrupt` 체크 (필수 — 인터럽트 수신 방식)
3. **Connectivity → USART3** (PC)
   - Mode: `Asynchronous`
   - Baud Rate: PC측 터미널/수신 프로그램과 동일하게
   - (수신은 쓰지 않으므로 NVIC 인터럽트는 필수는 아님)
4. **Timers → TIM6** (1초 주기 트리거, 여유 있는 다른 타이머로 대체 가능)
   - Activate 체크
   - Prescaler / Counter Period(ARR)을 타이머 클럭에 맞춰 정확히 1Hz가
     되도록 설정합니다. 예를 들어 타이머 입력 클럭이 10MHz라면
     Prescaler=9999(÷10000 → 1kHz), Period=999(÷1000 → 1Hz) 식으로
     "Prescaler와 Period의 곱이 (타이머 클럭 ÷ 1Hz)"가 되게 계산하면
     됩니다. 정확한 타이머 클럭은 프로젝트의 Clock Configuration 탭에서
     확인하세요.
   - NVIC 탭에서 `TIM6 global interrupt` 체크

Generate Code를 실행하면 CubeMX가 `Core/Src/usart.c`에
`huart1`/`huart2`/`huart3`와 각각의 `MX_USARTx_UART_Init()`을,
`Core/Src/tim.c`에 `htim6`와 `MX_TIM6_Init()`을 생성하고, 대응하는
헤더(`usart.h`, `tim.h`)에 `extern` 선언을 넣어 줍니다. 이 라이브러리는
그 핸들들을 그대로 사용합니다.

### 이 라이브러리 파일 추가하기

`Inc/*.h`, `Src/esp32_at.c`, `Src/wifi_manager.c`, `Src/adc_uart.c`,
`Src/data_reporter.c`를 프로젝트의 `Core/Inc`, `Core/Src`에 복사하면
STM32CubeIDE가 자동으로 include 경로와 빌드에 포함시킵니다(별도 폴더에
두는 경우 프로젝트 속성의 Include Paths에 해당 폴더를 추가해야 합니다).
`*_usercode_reference.c` 3개는 참고용이므로 복사하지 않아도 됩니다(복사해도
`#if 0`으로 감싸져 있어 빌드에는 영향이 없습니다).

### main.c / usart.c / tim.c에 코드 삽입하기

CubeMX는 "Generate Code"를 다시 실행해도 `/* USER CODE BEGIN ... */`와
`/* USER CODE END ... */` 사이의 내용은 보존합니다. 따라서 이 라이브러리를
호출하는 코드는 반드시 그 구역 안에 넣어야 합니다.

- `Core/Src/usart.c`
  - 상단 `USER CODE BEGIN 0`: `#include "esp32_at.h"`, `#include "adc_uart.h"`
  - 하단 `USER CODE BEGIN 1`: `HAL_UART_RxCpltCallback()`을 오버라이드해
    `huart->Instance`로 분기 — `USART1`이면 `ESP32_AT_UART_RxCpltCallback()`,
    `USART2`이면 `ADC_UART_RxCpltCallback()` 호출
  - 정확한 위치와 코드는 `Src/usart_usercode_reference.c` 참고

- `Core/Src/tim.c`
  - 상단 `USER CODE BEGIN 0`: `#include "data_reporter.h"`
  - 하단 `USER CODE BEGIN 1`: `HAL_TIM_PeriodElapsedCallback()`을
    오버라이드해 `htim->Instance == TIM6`일 때 `DataReporter_TimerTick()` 호출
  - 정확한 위치와 코드는 `Src/tim_usercode_reference.c` 참고

- `Core/Src/main.c`
  - `USER CODE BEGIN Includes`: `wifi_manager.h`, `adc_uart.h`,
    `data_reporter.h` include
  - `USER CODE BEGIN PV`: AP/IP/서버 설정 구조체 선언
  - `USER CODE BEGIN 2` (주변장치 초기화 이후, while(1) 진입 전):
    `WiFi_Manager_Init()`, `ADC_UART_Init(&huart2)`,
    `DataReporter_Init(&huart3)`, 그리고 **`HAL_TIM_Base_Start_IT(&htim6)`**
    호출 (CubeMX는 타이머를 초기화만 하고 시작하지 않으므로 반드시
    직접 호출해야 합니다)
  - `USER CODE BEGIN 3` (while(1) 루프 안): 매 반복마다
    `WiFi_Manager_Process()`와 `DataReporter_Process()` 호출
  - 정확한 위치와 코드는 `Src/main_usercode_reference.c` 참고

## 연결 절차 (WiFi 상태 머신 흐름)

1. `AT` - 모듈 응답 확인
2. `ATE0` - 에코 끄기
3. `AT+CWMODE=1` - Station 모드
4. `AT+CWDHCP=1,<0|1>` - DHCP on/off
5. (DHCP off인 경우) `AT+CIPSTA="ip","gateway","netmask"` - 정적 IP 설정
6. `AT+CWJAP="ssid","password"` - AP 접속 (`WIFI GOT IP`까지 대기)
7. `AT+CIPMUX=0` - 단일 연결 모드
8. `AT+CIPSTART="TCP","server_ip",port` - TCP 서버 접속
9. 이후 `CONNECTED` 상태에서 5초마다 `AT+CIPSTATUS`로 상태를 점검하고,
   비동기 URC(`WIFI DISCONNECT`, `+CIPCLOSED` 등)도 함께 감시합니다.

## 재접속 로직

- **AP 연결이 끊긴 경우**(`WIFI DISCONNECT` URC 수신, 또는 `AT+CIPSTATUS`가
  `STATUS:5` 반환): `AT+CWJAP`부터 다시 시도합니다(정적 IP는 이미 설정되어
  있으므로 재설정하지 않습니다).
- **서버 연결만 끊긴 경우**(`STATUS:4`, 또는 `+CIPCLOSED` URC): AP는 그대로
  둔 채 `AT+CIPSTART`만 재시도합니다.
- 재시도는 2초부터 시작해 실패할 때마다 2배씩 늘어나 최대 60초까지 커지는
  지수 백오프(exponential backoff)를 사용하며, 성공하면 다시 2초로
  초기화됩니다. 재시도 횟수 제한은 없고, 연결이 복구될 때까지 계속
  시도합니다(무제한 재시도가 부담스러우면 `wifi_manager.c`의
  `RETRY_BACKOFF_MAX_MS` 근처에 실패 횟수 카운터를 추가해 상한을 두면
  됩니다).
- 재접속을 시도하는 동안(`AT+CWJAP` 최대 20초)에도 ADC 수신과 PC 전송은
  타이머 인터럽트로 계속 동작합니다. 서버로의 보고만 그 시간 동안
  지연/건너뛰어집니다.

## 주기적 데이터 전송 (ADC → 서버 / PC)

- `ADC_UART_GetLatest()`는 USART2로 받은 가장 최근 한 줄(기본 가정: 외부
  ADC 장비가 `"<value>\r\n"` 형식의 ASCII 라인을 보낸다)을 반환합니다.
  실제 장비 프로토콜(바이너리 프레임 등)이 다르면 `adc_uart.c`의 라인
  조립 로직만 바꾸면 되고, 인터페이스는 그대로 재사용할 수 있습니다.
- `DataReporter_TimerTick()`(1Hz TIM 인터럽트에서 호출)은 매초
  `"ADC,<수신시각>,<값>\r\n"` 형식의 리포트 문자열을 만들어
  - USART3로 즉시 전송하고(PC, 항상 수행),
  - 같은 문자열을 서버 전송용 버퍼에 넣고 `DataReporter_Process()`가
    처리하도록 플래그를 세웁니다.
- `DataReporter_Process()`(메인 루프에서 매 반복 호출)는 플래그가 서 있으면
  버퍼를 잠깐의 임계구역(`__disable_irq`/`__enable_irq`)으로 복사한 뒤,
  `WiFi_Manager_IsConnected()`일 때만 `WiFi_Manager_Send()`로 서버에
  전송합니다.

## 사용 예시

전체 흐름은 `Src/main_usercode_reference.c`, `Src/usart_usercode_reference.c`,
`Src/tim_usercode_reference.c`를 참고하세요. 핵심만 요약하면:

```c
/* USER CODE BEGIN PV */
static wifi_ap_config_t     s_ap_cfg = { .ssid = "MyHomeAP", .password = "MyAPPassword123" };
static wifi_ip_config_t     s_ip_cfg = { .dhcp_enable = false, .ip = "192.168.0.50",
                                          .gateway = "192.168.0.1", .netmask = "255.255.255.0" };
static wifi_server_config_t s_server_cfg = { .ip = "192.168.0.100", .port = 8080 };
/* USER CODE END PV */

/* USER CODE BEGIN 2 */
WiFi_Manager_Init(&huart1, &s_ap_cfg, &s_ip_cfg, &s_server_cfg);
ADC_UART_Init(&huart2);
DataReporter_Init(&huart3);
HAL_TIM_Base_Start_IT(&htim6);
/* USER CODE END 2 */

while (1)
{
  /* USER CODE BEGIN 3 */
  WiFi_Manager_Process();
  DataReporter_Process();
  HAL_Delay(10);
}
/* USER CODE END 3 */
```

DHCP를 사용하려면 `s_ip_cfg.dhcp_enable = true`로 두면 되고, 이때
`ip`/`gateway`/`netmask` 필드는 사용되지 않습니다. 서버 접속이 필요 없다면
`WiFi_Manager_Init()`의 `server_cfg` 인자에 `NULL`을 넘기면 AP 접속까지만
수행합니다(이 경우 `DataReporter_Process()`의 서버 전송은 항상 건너뜁니다).

## 참고 / 제한 사항

- `AT+CWJAP`, `AT+CIPSTART`, `AT+CIPSEND`처럼 응답까지 시간이 걸리는 명령은
  해당 호출 시점에서 최대 타임아웃만큼 메인 루프를 블로킹합니다
  (`AT+CWJAP`은 최대 20초, `AT+CIPSTART`는 최대 10초, `AT+CIPSEND`는 최대
  3초). ADC 수신/PC 전송은 이 블로킹과 무관하게 동작하도록 인터럽트로
  분리했지만, 서버로의 보고 주기는 WiFi 상태에 따라 지연될 수 있습니다.
- `DataReporter_TimerTick()`은 TIM 인터럽트 컨텍스트에서
  `HAL_UART_Transmit()`을 짧게(최대 `PC_TX_TIMEOUT_MS`, 기본 50ms) 블로킹
  호출합니다. 이 동안 같거나 낮은 우선순위의 다른 인터럽트(USART1/USART2
  RX 등)가 지연될 수 있으므로, NVIC 우선순위를 지나치게 높게 주지
  않는 것을 권장합니다. 지터를 완전히 없애고 싶다면
  `HAL_UART_Transmit_IT()` + `HAL_UART_TxCpltCallback()` 기반으로
  바꿀 수 있습니다.
- ESP-AT 펌웨어 버전에 따라 명령 문법이 조금씩 다를 수 있습니다(특히
  `AT+CWDHCP`, `AT+CIPSTA`). 사용 중인 ESP-AT 릴리스의 AT 명령 레퍼런스를
  확인하고 필요시 `wifi_manager.c`의 명령 문자열을 맞춰주세요.
