# ESP32-C3 AT 명령으로 Wi-Fi 연결 + 서버 PC와 TCP 통신 (STM32CubeMX / STM32CubeIDE)

STM32 MCU가 UART로 ESP32-C3(AT 펌웨어)를 제어해서 ① Wi-Fi에 접속하고,
② 서버 PC와 TCP 소켓을 맺고, ③ 데이터를 주고받고, ④ 연결이 끊기면 자동으로
재접속하는 예제입니다. MCU는 **STM32L562RCT6**(STM32L5 시리즈, Cortex-M33)
기준으로 작성했으며, UART 핸들 이름만 바꾸면 다른 STM32 보드에도 그대로
적용됩니다.

> **TrustZone 관련 주의**: STM32L562RCT6은 TrustZone(TZEN)을 지원합니다.
> 이 예제는 CubeMX에서 TrustZone을 **비활성화**한 단일(Non-secure only)
> 프로젝트를 기준으로 합니다. TrustZone을 활성화하면 Secure/Non-secure
> 프로젝트가 분리되고 GTZC로 주변장치(UART 포함)의 보안 속성을 지정해야
> 하므로 이 예제 범위를 벗어납니다.

## 1. 전체 구조

```
[서버 PC] <--TCP/IP(Wi-Fi 공유기)--> [ESP32-C3, AT 펌웨어] <--UART(115200)--> [STM32]
                                                                                 |
                                                                        esp32_at.c/.h
                                                                        (AT 명령 상태머신)
```

- STM32는 ESP32-C3에게 **AT 명령 문자열**을 UART로 보내고, ESP32-C3는 그 결과를
  텍스트 응답(`OK`, `ERROR`, `WIFI GOT IP` 등)으로 돌려줍니다.
- 실제 Wi-Fi/TCP 스택은 전부 ESP32-C3 안에서 처리되고, STM32는 "명령을 보내고
  응답을 해석하는" 역할만 합니다.
- `Core/Inc/esp32_at.h`, `Core/Src/esp32_at.c` 가 이 전체 과정을 **논블로킹
  상태 머신**으로 구현한 드라이버입니다. `ESP32_Process()` 를 `main()`의
  `while(1)` 루프에서 계속 호출해주기만 하면 접속, 통신, 재접속을 알아서
  진행합니다.

## 2. 하드웨어 연결

| ESP32-C3 | STM32L562RCT6 |
|---|---|
| TX | PA10 (USART1_RX) |
| RX | PA9  (USART1_TX) |
| GND | GND (공통) |
| 3V3 | 3.3V (ESP32-C3, STM32L5 모두 3.3V 전용, 5V 금지) |

디버그 로그 출력용 UART는 PA2(USART2_TX)/PA3(USART2_RX)를 예시로 사용했습니다.
실제 사용 중인 보드(자체 제작 보드 등)에 ST-Link VCP나 별도 USB-UART가
다른 핀/USART에 물려 있다면 `main.c`의 `MX_USART2_UART_Init()`,
`MX_GPIO_Init()`을 해당 핀 번호로 수정하세요.

> ESP32-C3 모듈에 이미 AT 펌웨어가 플래시되어 있어야 합니다(대부분의
> ESP32-C3-DevKitM 류는 공장 출고 시 AT 펌웨어가 들어있거나, Espressif의
> `esp-at` 리포지토리에서 미리 빌드된 바이너리를 플래싱해서 준비합니다).

## 3. STM32CubeMX 설정

1. **New Project** → MCU Selector에서 `STM32L562RCT6` 검색 후 선택
   (보드가 아닌 개별 MCU로 시작 — 자체 제작 보드 기준).
2. `Pinout & Configuration` → `System Core` → **RCC**에서 아래 사항 확인:
   - TrustZone은 **비활성화** 상태로 둡니다(기본값).
3. `Connectivity` → **USART1** 선택 → Mode: `Asynchronous`.
   - Parameter Settings: Baud Rate `115200`, Word Length `8 Bits`,
     Parity `None`, Stop Bits `1` (ESP-AT 기본값과 동일하게 맞춤).
   - NVIC Settings 탭에서 **USART1 global interrupt** 체크 (인터럽트 수신용).
4. **USART2** (디버그 로그용) 도 동일하게 `Asynchronous`, 115200bps로 설정.
   PA2/PA3 대신 다른 핀을 쓰려면 Pinout view에서 원하는 핀을 클릭해
   `USART2_TX`/`USART2_RX`로 지정합니다.
5. `Clock Configuration` 탭에서 SYSCLK를 원하는 값으로 설정합니다
   (이 예제는 MSI 4MHz → PLL → 110MHz 기준, STM32L5 최대 클럭). MSI 대신
   보드에 HSE 크리스탈이 있다면 그에 맞게 Oscillator를 바꿔도 됩니다.
6. `Project Manager` → Toolchain/IDE를 **STM32CubeIDE**로 선택 → `GENERATE CODE`.
7. 생성된 프로젝트에 이 저장소의 다음 파일들을 복사합니다.
   - `Core/Inc/esp32_at.h` → 프로젝트의 `Core/Inc/`
   - `Core/Src/esp32_at.c` → 프로젝트의 `Core/Src/`
   - `main.c`의 `USER CODE BEGIN/END` 블록 안 내용(초기화, while 루프,
     콜백 함수들)을 CubeMX가 생성한 `main.c`의 동일한 `USER CODE` 구간에
     옮겨 넣습니다. (이 저장소의 `Core/Src/main.c`는 통째로 붙여넣어도 되도록
     완성된 예시이지만, 실제로는 CubeMX가 자동 생성하는 `SystemClock_Config`,
     `MX_GPIO_Init` 등과 병합해서 써야 합니다.)
   - `HAL_UART_RxCpltCallback()` 함수 안에서 `ESP32_UART_RxCpltCallback(huart);`
     를 호출하도록 추가합니다(이미 있다면 그 안에 한 줄만 추가).

> STM32L5는 F1 계열과 달리 AFIO 리매핑이 없고, `GPIO_InitTypeDef.Alternate`
> 필드로 핀마다 AF 번호(USART1/2 모두 `AF7`)를 직접 지정합니다. CubeMX가
> 이 부분을 자동 생성해주므로 직접 신경 쓸 필요는 없습니다.

## 4. AT 명령 시퀀스 (드라이버가 자동으로 순서대로 실행)

| 단계 | 명령 | 의미 |
|---|---|---|
| 1 | `AT` | 모듈이 살아있는지 확인 |
| 2 | `ATE0` | 에코 끄기(응답 파싱을 단순하게) |
| 3 | `AT+CWMODE=1` | Station 모드로 설정 |
| 4 | `AT+CWJAP="SSID","PASSWORD"` | 공유기(AP)에 접속 → 성공 시 `WIFI CONNECTED`, `WIFI GOT IP`, `OK` 순서로 응답 |
| 5 | `AT+CIPMUX=0` | 단일 TCP 연결 모드 |
| 6 | `AT+CIPMODE=0` | 일반(비-투명) 모드: `AT+CIPSEND=<len>` 으로 프레임 단위 송신 |
| 7 | `AT+CIPSTART="TCP","<서버IP>",<포트>` | 서버 PC로 TCP 연결 |
| 8 | (통신 중) `AT+CIPSEND=<len>` + 데이터 | 데이터 전송, 성공 시 `SEND OK` |
| - | `+IPD,<len>:<data>` | 서버가 보낸 데이터가 비동기로 도착(모듈이 자체적으로 push) |

## 5. 재접속(재시도) 로직

`esp32_at.c`의 상태 머신은 다음 상황을 **비동기 이벤트**로 감지합니다.

- `WIFI DISCONNECT` 수신 → AP 연결 자체가 끊김
- `CLOSED` 수신 → TCP 연결이 서버/네트워크 쪽에서 종료됨
- `AT+CIPSEND` 이후 `SEND FAIL`/`ERROR` 수신 → 링크 이상으로 간주

이 중 하나라도 감지되면 `TCP_CONNECTED` → `LINK_DOWN` → `RECONNECT_WAIT`
상태로 전이하고, 지정된 시간(`ESP32_RECONNECT_BASE_MS = 2초`)만큼 대기한 뒤
**처음(AT 응답 확인)부터** 재접속을 시도합니다. 실패가 반복되면 대기 시간을
2배씩 늘려(지수 백오프) 최대 `ESP32_RECONNECT_MAX_MS`(기본 60초)까지 늘어나며,
공유기/서버에 과도한 재접속 요청이 몰리지 않도록 합니다. 접속에 성공하면
백오프 값은 다시 초기값으로 리셋됩니다.

모듈 자체가 `AT` 명령에도 응답하지 않는 상태(배선/전원 문제 등)가 5회 연속
반복되면 `ESP32_STATE_FATAL_ERROR`로 전이해 무한 재시도를 멈춥니다(필요하면
`esp32_at.c`의 `RetryOrFatal()`에서 하드웨어 리셋 GPIO 토글 등으로 확장 가능).

## 6. 사용 방법 (애플리케이션 코드 관점)

```c
ESP32_Init(&huart1);
ESP32_SetWiFiCredentials("MyWiFi", "MyPassword");
ESP32_SetServer("192.168.0.10", 8000);
ESP32_SetDataCallback(OnEsp32Data);   // 서버가 보낸 데이터 수신 시 호출
ESP32_SetStateCallback(OnEsp32State); // 연결 상태가 바뀔 때마다 호출

while (1) {
    ESP32_Process(); // 반드시 계속 호출: 연결/재접속을 진행시킴

    if (ESP32_GetState() == ESP32_STATE_TCP_CONNECTED) {
        ESP32_Send((uint8_t *)"hello\n", 6);
    }
}
```

자세한 전체 예시는 `Core/Src/main.c` 참고.

## 7. 서버 PC 쪽 테스트

`tools/server_test.py` 는 파이썬 표준 라이브러리만으로 동작하는 테스트용
TCP 서버입니다.

```bash
python3 tools/server_test.py 8000
```

- 접속되면 1초마다 `ping <n>`을 STM32로 보내고, STM32가 보낸 데이터를
  콘솔에 출력합니다.
- 서버를 강제로 종료(Ctrl+C 후 재실행 등)해서 STM32 쪽 재접속 로직이
  정상 동작하는지 확인할 수 있습니다. `main.c` 예제의 `OnEsp32State()`가
  `[STATE] 연결 끊김 감지` → `[STATE] 재접속 대기 중` → `[STATE] 서버 접속 완료`
  순서로 USART2(디버그 콘솔)에 로그를 출력하는 것을 시리얼 터미널
  (예: PuTTY, TeraTerm)로 확인하면 됩니다.

## 8. 자주 발생하는 문제

- **`AT` 명령에 응답이 없음**: 배선(TX/RX 교차 연결 여부), ESP32-C3 전원(3.3V),
  UART 보레이트(모듈에 따라 기본값이 115200이 아닐 수 있음, `AT+UART_DEF` 로
  확인/변경 가능)를 점검하세요.
- **`AT+CWJAP` 이 계속 실패**: SSID/비밀번호 오타, 2.4GHz 대역인지 확인(ESP32는
  5GHz Wi-Fi 미지원), 공유기의 MAC 필터링 여부 확인.
- **`AT+CIPSTART` 실패**: 서버 PC 방화벽에서 해당 포트가 막혀있지 않은지,
  서버 PC와 ESP32가 같은 네트워크(같은 공유기)에 있는지 확인.
- **데이터가 깨져서 옴**: `esp32_at.c`의 `ESP32_LINE_BUF_SIZE`, `+IPD` 청크
  버퍼(`s_ipd_chunk`) 크기를 실제 페이로드 크기에 맞게 늘려야 할 수 있습니다.
