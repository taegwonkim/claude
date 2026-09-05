# STM32L562C WiFi 계측 브릿지 — Firmware (RTOS 미사용 / Super-loop 버전)

[`../firmware/`](../firmware/)와 기능/프로토콜/핀맵은 완전히 동일하지만, **FreeRTOS를 쓰지 않고
단일 `while(1)` super-loop에서 협력형(cooperative)으로 폴링**하도록 다시 작성한 버전입니다.
어떤 버전을 CubeIDE 프로젝트에 넣을지는 하나만 고르면 됩니다(둘을 동시에 쓰지 않음).

두 버전 사이의 통신 프로토콜/역할 분담은 동일하므로 [`../docs/프로토콜_명세.md`](../docs/프로토콜_명세.md)를
그대로 따릅니다. PC용 C# 도구([`../pc-app/`](../pc-app/))는 어느 펌웨어 버전과 연결해도 동작에
차이가 없습니다(시리얼 프레임 포맷이 같기 때문).

## 왜 두 버전이 있나

`firmware/`(FreeRTOS)는 FPGA 트리거 처리, ESP32 AT 통신, PC 커맨드 처리를 각각 별도
우선순위의 태스크로 분리해 **진짜 동시성**을 확보합니다(예: ESP32가 WiFi 연결 시도로
수 초~수십 초 블로킹되어도 FPGA 트리거는 그 사이에도 최우선 태스크로 즉시 처리됨).
이 `firmware-no-rtos/`는 RTOS 없이 같은 기능을 구현한 버전으로, 코드/의존성이 더 단순하지만
**아래 "RTOS 미사용의 결과"에 정리한 실시간성 트레이드오프**가 있습니다. 리소스가 아주
제한적인 보드로 옮기거나, RTOS 없이 더 단순하게 유지보수하고 싶을 때 사용하세요.

## 아키텍처: 협력형 super-loop

FreeRTOS의 태스크 4개(`FPGA_Task`/`ESP32_Task`/`PCComm_Task`/`Config_Task`)를 각 모듈의
논블로킹(또는 짧게 블로킹하는) `*_Poll()` 함수로 바꾸고, `main()`의 `while(1)`에서 매 반복
순서대로 호출합니다:

```c
App_Init();          // main()의 페리퍼럴(HAL) 초기화 이후, 1회
while (1) {
    App_Run();        // 매 반복 — 아래 5개를 순서대로 폴링
}
```

`App_Run()`(`Core/Src/app_main.c`) 내부:

```c
void App_Run(void)
{
    FpgaLink_Poll();   // 트리거 대기 -> 최대 200ms ADC 라인 수신 -> PC 미러 + 큐 적재
    Esp32_Poll();       // WiFi(재)연결 요청 처리, 큐에서 측정값 팝 -> TCP 전송, URC 폴링,
                        // 재접속 타이머, STATUS 주기 브로드캐스트
    PcComm_Poll();      // USART3+USB 라인 처리 (SET/SAVE/GET CONFIG/STATUS/HELP)
    Config_Poll();      // SAVE 요청 있으면 플래시 쓰기 -> WiFi 재연결 요청
    StatusLed_HeartbeatTick(); // LED_RUN 1Hz 토글
}
```

RTOS의 큐/세마포어/뮤텍스는 모두 `app_main.c`가 소유한 단순 구조로 대체했습니다(전부 같은
super-loop 스레드에서만 접근하므로 락이 필요 없습니다):

| RTOS 버전 | 이 버전 |
|---|---|
| `g_measQueueId`(osMessageQueue, 길이 8) | `app_main.c`의 고정 크기 원형 버퍼 + `App_PushMeasurement()` |
| `g_cfgEventQueueId`(osMessageQueue) | 단일 슬롯 pending 플래그 + `App_RequestConfigSave()` |
| `g_wifiEventQueueId`(osMessageQueue) | 단일 슬롯 pending 플래그(`app_main.c` 내부) |
| FPGA 트리거 `osSemaphoreId_t` | `volatile bool` 플래그(`fpga_link.c`) |
| `Esp32_GetCachedNetInfo()`의 `osMutexId_t` | 락 없음(단일 스레드라 불필요) |
| `osDelay(N)` | 제거하거나 `HAL_Delay(N)` (인터럽트는 그대로 동작) |

## RTOS 미사용의 결과 (반드시 확인)

super-loop는 **선점형이 아니므로**, 한 `*_Poll()` 호출이 오래 걸리면 그동안 나머지 폴링은
전혀 진행되지 않습니다. 원래 RTOS 버전에서 이미 블로킹 방식으로 짜여 있던 부분들이라 코드
구조 자체는 거의 그대로지만, RTOS에서는 "그 태스크만" 블로킹되고 다른 태스크는 계속 돌았던
반면, 여기서는 **전체 시스템이 함께 멈춥니다**. 실제 영향:

- **WiFi/TCP (재)연결 중(`Esp32_ConnectWifi`/`Esp32_TcpConnect`, 최대 수 초~`APP_AT_CWJAP_TIMEOUT_MS`
  =20초)**: 이 동안 FPGA 트리거가 와도 `FpgaLink_Poll()`이 실행되지 않아 측정값을 놓칠 수
  있고, PC 커맨드 응답도 그만큼 늦어집니다. ISR(EXTI/UART DMA)은 계속 동작하므로 데이터
  자체가 유실되진 않지만 처리가 지연됩니다(단, 200ms 타임아웃 안에 못 읽은 FPGA 라인은
  RTOS 버전과 마찬가지로 드롭됩니다).
- **부팅 시 ESP32 프로브 대기(`App_Init()`)**: ESP32가 응답할 때까지 `App_Run()` 자체가
  아직 한 번도 돌지 않으므로, 이 구간에는 PC 커맨드/FPGA 트리거 처리가 전혀 없습니다.
- **`App_PopMeasurement()`가 "최대 100ms 대기 후 없으면 넘어가기"가 아니라 즉시 넘어가는
  논블로킹으로 바뀜**: 측정값 처리가 아주 살짝(다음 루프 반복까지) 늦어질 수 있지만 큐가
  흡수하므로 유실되지는 않습니다.
- 반대로 이런 트레이드오프가 부담스럽다면 `../firmware/`(FreeRTOS 버전)를 사용하세요.

## CubeMX 설정

핀맵/클럭/USART/SPI/USB 설정은 하드웨어가 동일하므로
[`../firmware/docs/CubeMX_설정가이드.md`](../firmware/docs/CubeMX_설정가이드.md)를 그대로
따르면 됩니다. 차이는 딱 하나:

- **§9 "FreeRTOS (Middleware and Software Packs → FREERTOS)" 절은 건너뛰세요 — Enable하지
  않습니다.** "Tasks and Queues" 태스크 표, `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`
  관련 NVIC 제약(FreeRTOS 전용)도 이 버전에는 해당 없습니다. NVIC 우선순위는 일반적인
  ARM Cortex-M33 규칙만 따르면 되며(다만 TrustZone Security Extension으로 인해 Priority
  Group이 3비트(0~7)까지만 제공되는 것은 RTOS 여부와 무관한 이 실리콘의 특성이라 여전히
  동일합니다), 서로 겹치지만 않으면 원하는 값으로 설정 가능합니다.
- Project Manager → Toolchain/IDE: STM32CubeIDE, MCU: STM32L562CETx — 동일.

## 콜백/훅 연결 (USER CODE 영역)

`firmware/docs/CubeMX_설정가이드.md` §11과 거의 같지만, RTOS 진입점이 없으므로 `main.c`에
직접 연결합니다:

1. **`Core/Src/main.c`**
   - `/* USER CODE BEGIN 2 */` (모든 `MX_..._Init()` 호출 뒤, `while(1)` 진입 전)에서
     `App_Init();` 1회 호출 (`#include "app_main.h"` 추가).
   - `while (1) { ... }`의 `/* USER CODE BEGIN 3 */` 영역에서 `App_Run();` 호출.
2. **`Core/Src/stm32l5xx_it.c`**: `firmware/`와 동일 — CubeMX가 생성한 그대로 두면 됩니다.
   `HAL_GPIO_EXTI_Callback()`/`HAL_UARTEx_RxEventCallback()`/
   `HAL_RTCEx_WakeUpTimerEventCallback()`은 `Core/Src/app_it_callbacks.c`
   (본 디렉터리에도 동일하게 포함) 한 곳에만 정의되어 있습니다. RTC 설정 자체는
   `firmware/docs/CubeMX_설정가이드.md` §8-1을 그대로 따르면 됩니다(RTOS 유무와 무관한 설정).
3. **`Core/Src/usbd_cdc_if.c`**: `firmware/`와 동일 — `CDC_Receive_FS()`의
   `USER CODE BEGIN 6` 영역에서 `PC_Comm_FeedUSB(Buf, *Len);` 호출.

## 폴더 구조

```
Core/Inc/
  app_config.h        - 프로토콜 상수, 버퍼/큐 크기, 핀 매핑 정의 (RTOS 우선순위/스택 없음)
  app_main.h           - App_Init()/App_Run() 진입점 (app_freertos.h 대응)
  ring_buffer.h         - SPSC 바이트 링버퍼 (firmware/와 동일)
  uart_line_rx.h        - idle-line + Circular DMA UART 수신 공용 헬퍼 (firmware/와 동일)
  measurement_msg.h     - FPGA 측정값 메시지 구조체 + DATA 라인 빌더 (firmware/와 동일)
  w25q40.h              - SPI2 NOR 플래시 드라이버 (firmware/와 동일, .c만 osDelay->HAL_Delay)
  net_config_store.h    - 설정 구조체 + CRC32 + 플래시 로드/세이브 (firmware/와 동일)
  esp32_at.h            - ESP32(USART1) AT 커맨드 드라이버 (뮤텍스 제거, HAL_Delay 사용)
  pc_frame.h            - PC↔MCU STX+CSV+CRLF 프레임 빌드/파싱 (firmware/와 동일)
  pc_comm.h             - PC 커맨드 파서 (PcComm_Task -> PcComm_Poll)
  fpga_link.h           - FPGA START 송신 + 트리거/ADC 수신 (세마포어 -> 플래그, Task -> Poll)
  status_led.h          - LED_RUN/LED_WIFI 제어 (firmware/와 동일)
  reset_config.h         - RTC 리셋 주기(초) 설정 구조체 + CRC32 + 플래시 로드/세이브 (firmware/와 동일, 섹터1)
  rtc_wakeup.h           - RTC Wakeup Timer 무장 + 백업 레지스터 리셋 카운터 (firmware/와 동일,
                          RTOS API 미사용 코드라 두 변형 간 차이 없음)
Core/Src/
  (위 헤더들의 구현)
  app_main.c            - App_Init()/App_Run(), 측정값 큐/설정저장 요청/WiFi연결요청 구현
  app_it_callbacks.c    - HAL_UARTEx_RxEventCallback/HAL_GPIO_EXTI_Callback/
                          HAL_RTCEx_WakeUpTimerEventCallback 단일 정의 + dispatch
                          (firmware/와 동일 — 호출 대상 함수 시그니처가 그대로라 변경 없음)
```

## 빌드 방법 (요약)

1. STM32CubeMX에서 위 "CubeMX 설정" 절대로 새 프로젝트 생성(FreeRTOS는 Enable하지 않음),
   Toolchain/IDE: STM32CubeIDE 선택 후 Generate Code.
2. 생성된 프로젝트의 `Core/Inc`, `Core/Src`에 이 디렉터리의 동일 파일들을 복사(병합).
3. 위 "콜백/훅 연결" 절대로 `main.c` 등에 `App_Init()`/`App_Run()` 호출을 연결.
4. STM32CubeIDE에서 빌드 후 ST-LINK로 플래싱.

## 테스트 방법

`firmware/`와 동일합니다 — PC에서 터미널로 USART3 또는 USB CDC 포트(115200 8N1)에 접속해
`HELP\r\n`으로 커맨드 목록 확인, `SAVE\r\n`으로 플래시 저장 및 자동 WiFi 재접속 확인.
