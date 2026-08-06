# STM32CubeMX 설정 가이드 (STM32L562CETx + FreeRTOS)

> **대상 버전: STM32CubeMX 6.18.1.** Pinout & Configuration / Clock Configuration / Project
> Manager의 기본 3-탭 구조와 페리퍼럴별 설정 항목은 6.x 계열 전반에서 안정적으로 유지되어
> 왔으므로 이 가이드는 다른 6.x 버전에서도 대체로 그대로 적용됩니다. 다만 좌측 IP 트리에서
> FreeRTOS/USB_DEVICE 같은 항목은 **"Middleware and Software Packs"** 카테고리 아래(6.18.1
> 기준 위치)에 있으니, 옛 버전 자료에서 흔히 보이는 "Middleware" 단독 표기와 다르면 이 경로를
> 따르세요. 버튼/메뉴 문구가 여기 적힌 것과 미세하게 다르면(예: 아이콘 위치, 하위 탭 순서)
> 기능은 동일하니 가장 가까운 항목을 선택하면 됩니다.

## 0. 프로젝트 생성

- MCU 선택: `STM32L562CETx` (LQFP48/UFBGA 등 보유 패키지에 맞게). MCU 선택 후 CubeMX가
  **STM32CubeL5 펌웨어 패키지** 설치/업데이트를 요구하면 CubeMX가 제안하는 최신 호환 버전을
  그대로 설치하세요(6.18.1은 특정 최소 펌웨어 패키지 버전을 요구할 수 있음).
- Project Manager → Toolchain/IDE: **STM32CubeIDE**
- Project Manager → Code Generator:
  - "Generate peripheral initialization as a pair of '.c/.h' files per peripheral" 체크 (관리 편의)
  - "Copy only the necessary library files" 권장 안 함 → Copy all used libraries files (오프라인 빌드 안정성)

STM32L562는 TrustZone(Cortex-M33) MCU입니다. 이 프로젝트 범위에서는 보안 분리 없이
**Non-secure만 사용**하는 것을 권장합니다:
- `System Core → GTZC` 또는 Option Bytes에서 TZEN 비활성화 상태 유지 (기본값), 즉 CubeMX에서
  Secure/Non-secure 프로젝트 분리 없이 단일 프로젝트로 생성.

## 1. Clock Configuration (Pinout & Configuration → RCC / Clock Configuration)

- RCC → High Speed Clock (HSE): 보드에 외부 크리스탈이 있다면 `Crystal/Ceramic Resonator` 선택
  (없으면 HSI 사용).
- USB를 쓰므로 48MHz 클럭 정밀도가 필요합니다. HSE + PLL로 SYSCLK를 올리고, USB용 48MHz는
  PLLSAI1(또는 PLL "Q" 분주)에서 생성하도록 Clock Configuration 탭에서 `48 MHz clocks` 라인이
  정확히 48.000MHz가 되도록 맞춥니다 (CubeMX가 자동 계산).
- SYSCLK: 110MHz (STM32L562 최대) 권장, HCLK/PCLK1/PCLK2 CubeMX 기본 자동계산 사용.

## 2. GPIO / 인터페이스 핀 배정 (실제 보드 핀맵)

| 기능 | 페리퍼럴 | 핀 | User Label | 비고 |
|---|---|---|---|---|
| PC 커맨드/데이터 | USART3 | PB10(TX)/PB11(RX) | - | 115200 8N1 |
| PC 커맨드/데이터(백업) | USB (Device, FS, CDC) | PA11(DM)/PA12(DP) | - | Virtual COM Port |
| ESP32 AT 통신 | USART1 | PA9(TX)/PA10(RX) | - | 115200 8N1, **흐름제어(RTS/CTS) 사용 안 함** |
| ESP32 하드웨어 리셋 | GPIO Output | PA8 | `ESP32_NRST` | Low active reset (ESP32 EN/RST 핀에 연결) |
| FPGA 트리거 입력 | GPIO EXTI | PH1 | `FROM_FPGA` | Falling edge, Pull-up |
| FPGA ADC 데이터 | USART2 | PA2(TX, 미사용)/PA3(RX) | - | FPGA(자체 ADC 리드)→MCU 단방향, 115200 8N1 |
| W25Q40 Flash | SPI2 (Master) | PB13(SCK)/PB14(MISO)/PB15(MOSI) | - | Flash 전용 |
| W25Q40 Flash CS | GPIO Output | PB12 | `EEP_NSS` | SW 제어 |
| 상태 LED(동작 확인) | GPIO Output | PC13 | `LED_RUN` | 1Hz 토글(heartbeat) |
| WiFi 상태 LED | GPIO Output | PC14 | `LED_WIFI` | 서버 TCP 연결 시 ON |

> **GPIO User Label 필수 지정**: Pinout 뷰에서 핀을 우클릭 → "Enter User Label"로 위 표의
> `User Label` 열 이름을 **정확히 그대로** 입력하세요. CubeMX가 `main.h`에
> `<Label>_GPIO_Port` / `<Label>_Pin` 매크로를 자동 생성하며, 본 리포지토리 코드
> (`esp32_at.c`, `w25q40.c`, `fpga_link.c`, `status_led.c`)가 이 매크로 이름을 그대로 참조합니다.
> - PA8(ESP32 리셋) → `ESP32_NRST`
> - PH1(FPGA 트리거) → `FROM_FPGA`
> - PB12(SPI2 CS) → `EEP_NSS`
> - PC13(상태 LED) → `LED_RUN`
> - PC14(WiFi 상태 LED) → `LED_WIFI`

## 3. USART1 (ESP32, AT 커맨드)

- Mode: Asynchronous, Baud Rate 115200, Word Length 8, Parity None, Stop 1,
  **Hardware Flow Control: Disable**(RTS/CTS 미사용 — ESP-AT 모듈도 `AT+UART_CUR`로 흐름제어
  없는 상태인지 확인할 것).
- NVIC: USART1 global interrupt Enable.
- DMA 탭: USART1_RX → DMA1 Channel(임의), Mode **Circular**, Data Width Byte
  (Idle-line 검출 + 순환 DMA 수신 방식으로 가변 길이 AT 응답 처리).
- USART1_TX → DMA1 Channel, Mode Normal (선택사항, IT만으로도 가능).
- NVIC에서 "USART1 global interrupt"의 **UART Idle line detection**을 쓰기 위해
  HAL_UARTEx_ReceiveToIdle_DMA API 사용 예정 (코드에서 처리, CubeMX는 DMA+IT만 설정하면 됨).

## 4. USART3 (PC)

- Mode: Asynchronous, 115200 8N1.
- NVIC Enable, DMA RX Circular (USART1과 동일한 방식), TX는 IT 또는 DMA Normal.

## 5. USB Device (PC 미러 채널)

- Connectivity → **USB**: Device (FS) 모드 Enable. (STM32L562는 F4/F7/L4+ 계열에 있는
  USB_OTG_FS IP가 아니라 별도의 단순 **USB(FS) Device-only** 페리퍼럴을 사용하므로, 좌측
  트리에도 "USB_OTG_FS"가 아닌 "USB" 항목만 나타납니다.)
- Middleware and Software Packs → USB_DEVICE: Class for FS IP = **Communication Device Class (Virtual Port Com)**.
- USB_DEVICE Parameter Settings: Device Descriptor의 VID/PID/문자열은 임의 설정 가능(사내 테스트용 VID 사용 권장).
- NVIC에서 USB 관련 인터럽트 자동 Enable 확인.

## 6. USART2 (FPGA → MCU, ADC 측정값)

- Mode: Asynchronous, **RX만 사용**(TX 핀은 배선하지 않아도 무방, CubeMX에서는 페어로 핀이 잡히지만
  실제 기판에서 TX는 미연결 가능). Baud Rate 115200, 8N1 (FPGA UART 코어 설정과 반드시 일치시킬 것).
- NVIC: USART2 global interrupt Enable.
- DMA 탭: USART2_RX → DMA, Mode **Circular**, Data Width Byte (USART1/3와 동일하게
  idle-line 검출 + 순환 DMA 수신 방식 사용, `HAL_UARTEx_ReceiveToIdle_DMA`).
- NVIC: EXTI line(트리거 핀, PH1 → `EXTI1_IRQn`) interrupt Enable.

> 트리거(EXTI, PH1/`FROM_FPGA`)와 ADC 값(USART2)은 **별개의 신호**입니다. FPGA는 ADC를 직접 읽어(자체 ADC IP
> 또는 외부 ADC 칩을 FPGA가 제어) falling-edge 펄스로 "측정 완료/전송 시작"을 알린 뒤,
> 곧이어 USART2로 측정값 라인을 전송합니다. MCU는 EXTI 인터럽트로 깨어난 뒤 USART2 수신을
> 타임아웃(`APP_AT_RESP_TIMEOUT_MS`류 상수, `fpga_link.c`에서 별도 상수 사용)과 함께 대기합니다.

## 7. SPI2 (W25Q40CLSNIG, Master)

- Mode: **Full-Duplex Master**.
- Hardware NSS: **Disable** (CS는 GPIO Output(PB12, User Label `EEP_NSS`)로 소프트웨어 제어 —
  멀티바이트 커맨드 시퀀스 제어에 유리).
- Data Size: 8 bits, CPOL=Low, CPHA=1 Edge (W25Q4x 표준 SPI Mode 0).
- Prescaler: 플래시 최대 클럭(104MHz 등) 이내로, PCLK 기준 적당히 분주 (예: 10~20MHz로 설정).
- DMA는 선택사항(설정값 저장은 소용량이라 폴링/IT로 충분). 필요시 SPI2 RX/TX DMA Normal 추가.

## 8. EXTI (FPGA 트리거)

- GPIO PH1(`FROM_FPGA`)을 GPIO_EXTI1로 설정, Pull-up, Trigger: **Falling edge**.
- NVIC → EXTI1 interrupt Enable, Preemption Priority는 아래 "인터럽트 우선순위" 절 참고.

## 9. FreeRTOS (Middleware and Software Packs → FREERTOS)

좌측 IP 트리에서 **Middleware and Software Packs → FREERTOS**를 체크하면 우측에 설정 화면이
열리고, 그 위쪽에 `Config parameters` / `Include parameters` / `Advanced settings` /
`Tasks and Queues` / `Timers and Semaphores` 등 서브탭이 나타납니다(6.18.1 기준, 순서는
버전에 따라 약간 다를 수 있음). 각 항목이 **어느 탭에 있는지**가 실수하기 쉬운 부분이라
아래에 탭별로 정리합니다.

- **드롭다운(Interface)**: FREERTOS 화면 최상단의 "Interface" 콤보에서 **CMSIS_V2** 선택
  (CMSIS_V1은 레거시, 이 프로젝트는 osThreadNew 등 V2 API 사용).
- **`Config parameters` 탭** (수치/드롭다운 값들):
  - `TOTAL_HEAP_SIZE`: 최소 **24576 (24KB)** 권장 (USB CDC + LWIP 미사용, AT 파서 버퍼 고려).
    메모리 여유가 있다면 32768(32KB)로 넉넉히.
  - `MINIMAL_STACK_SIZE`: 128 words (기본값) 유지.
- **`Include parameters` 탭** (기능 On/Off 체크박스들 — Config parameters 탭과 헷갈리지 말 것):
  - `USE_MUTEXES`: Enable (SPI2 플래시 접근, UART TX 공유 보호용)
  - `USE_COUNTING_SEMAPHORES`: Enable
  - `USE_TIMERS`: Enable (WiFi 재접속 재시도 타이머 등에 사용 가능)
- **`Advanced settings` 탭**:
  - `USE_NEWLIB_REENTRANT`: Disabled (불필요시 메모리 절약)
  - `configCHECK_FOR_STACK_OVERFLOW`: 2 (Method 2 권장, 개발 중 스택 오버플로우를 가장 확실히 잡음)
  - `configUSE_MALLOC_FAILED_HOOK`: Enable (Enabled)
  - `configASSERT`: Enable(개발 중)

### Tasks and Queues 탭 — 태스크 정의

CubeMX가 자동 생성하는 `defaultTask`는 유지하되(낮은 우선순위 idle 성격), 아래 태스크를
**추가**로 만듭니다 (Task 이름/우선순위/스택은 `Core/Inc/app_config.h`의 정의와 반드시 일치시키세요.
CubeMX GUI에서 만들지 않고 코드(`app_freertos.c`)에서 `osThreadNew()`로 직접 생성해도 무방합니다 —
본 리포지토리 코드는 **`app_freertos.c`에서 코드로 직접 생성하는 방식**을 사용합니다.
CubeMX Tasks and Queues 탭은 건드리지 않고 비워둬도 됩니다).

| Task | 우선순위(CMSIS-RTOS2) | Stack(words) | 역할 |
|---|---|---|---|
| `FPGA_Task` | `osPriorityRealtime` (또는 최소 `osPriorityHigh`) | 512 | EXTI 트리거 대기 → USART2로 도착하는 ADC 라인 수신 → 큐 전달 |
| `ESP32_Task` | `osPriorityAboveNormal` | 1024 | AT 커맨드 송수신, WiFi/TCP 연결, 측정값 서버 전송 |
| `PCComm_Task` | `osPriorityNormal` | 768 | USART3+USB 커맨드 파싱, 설정 변경 큐잉, 측정값 PC 에코 |
| `Config_Task` | `osPriorityBelowNormal` | 512 | W25Q40 플래시 R/W(뮤텍스 보호), 설정 CRC 검증 |
| `defaultTask`(CubeMX 기본) | `osPriorityLow` | 128(기본) | `LED_RUN` 하트비트 토글(`StatusLed_HeartbeatTick()`) 등 저부하 백그라운드 |

우선순위 근거: FPGA 트리거/ADC 라인은 늦게 처리하면 다음 트리거와 뒤섞여 데이터 유실이므로 최우선. ESP32/TCP는
초당 다회 응답을 놓치면 재시도 비용이 크므로 그 다음. PC 커맨드 처리와 플래시 저장은
사람 입력/저빈도 이벤트라 여유 있게 낮은 우선순위로도 충분합니다.

`ESP32_Task`는 시작 시 `ESP32_NRST`(PA8)를 통해 ESP32를 하드웨어 리셋(`Esp32_HardReset()`,
`esp32_at.c`)한 뒤 부팅 대기 → `AT` 프로브 순서로 진행합니다. 이렇게 하면 MCU 리셋/재플래싱 후
ESP32가 이전 세션의 어중간한 상태(예: 이전 TCP 연결이 반쯤 열린 상태)로 남아있지 않고 항상
깨끗한 상태에서 AT 커맨드 시퀀스를 시작합니다.

### 인터럽트(NVIC) 우선순위 — Core → NVIC 탭

**Sub Priority는 항상 0으로 고정되어 바꿀 수 없습니다 — 정상입니다, 오류 아닙니다.** FreeRTOS
미들웨어를 Enable하면 CubeMX가 NVIC Priority Group을 자동으로 **"4 bits for pre-emption
priority, 0 bits for subpriority"**로 강제 설정합니다(System Core → NVIC 탭 최상단의
"Priority Group" 드롭다운에서 확인 가능하며, 여기서 항목이 회색으로 잠겨 있거나 다른 그룹으로
바꿔도 다시 이 값으로 돌아옵니다). Cortex-M용 FreeRTOS 포트는 서브프라이오리티 개념 없이
**Preemption Priority 숫자 하나만으로** "이 ISR이 FreeRTOS `...FromISR` API를 호출해도
안전한가"를 판단하기 때문에, 서브프라이오리티를 쓰면 이 판단이 깨질 수 있어 ST가 의도적으로
막아놓은 것입니다. **Priority Group을 억지로 바꾸지 말고, 아래처럼 인터럽트마다 서로 다른
Preemption Priority 값을 주는 방식으로 우선순위를 구분하세요.**

FreeRTOS와 함께 쓸 때 `configLIBRARY_LOWEST_INTERRUPT_PRIORITY`(보통 15)와
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`(보통 5) 사이 규칙을 반드시 지켜야 합니다:
**FreeRTOS API(`...FromISR`)를 호출하는 ISR의 Preemption Priority 숫자는 5 이상(즉, 우선순위는
5보다 낮거나 같아야, 즉 "숫자가 커야")** 이어야 합니다 (숫자가 작을수록 하드웨어 우선순위가 높음).

| 인터럽트 | Preemption Priority | Sub Priority |
|---|---|---|
| EXTI1 (FPGA 트리거, PH1) | 5 | 0 (고정) |
| USART2 (FPGA ADC 데이터) + DMA | 6 | 0 (고정) |
| USART1 (ESP32) + DMA | 7 | 0 (고정) |
| USART3 (PC) + DMA | 8 | 0 (고정) |
| USB (FS Device) | 9 | 0 (고정) |
| SPI2 (Flash, 폴링 사용시 불필요) | 10 | 0 (고정) |
| SysTick | 15 (CubeMX 기본, FreeRTOS 커널 틱) | - |

> Pinout & Configuration → **System Core → NVIC** 탭(전체 인터럽트를 한 표로 보여주는 곳)에서
> 각 인터럽트를 선택 후 Preemption Priority 값만 위 표대로 지정하세요(Sub Priority 칸은
> 건드릴 수 없는 게 정상). 이 값은 USART1/USART2/USART3/SPI2 등 각 페리퍼럴 설정 화면의
> "NVIC Settings" 서브탭에서도 동일하게 보이고 수정할 수 있습니다(어느 쪽에서 바꿔도 서로
> 동기화됩니다) — Project Manager → Advanced Settings는 NVIC 우선순위가 아니라 HAL/LL
> 드라이버 생성 옵션을 다루는 별개의 화면이므로 혼동하지 마세요.

## 10. Memory 설정 (Linker / 사용량 계획)

STM32L562CET6 기준 Flash 512KB / SRAM 256KB(SRAM1+SRAM2+SRAM3 합산, 모델별 상이 — 데이터시트 확인):

- FreeRTOS Heap: 24~32KB (위 TOTAL_HEAP_SIZE)
- 태스크 스택 합계 대략: (512+1024+768+512+128) words × 4byte ≈ **11.7KB**
- USB CDC 미들웨어 내부 버퍼: 약 2~3KB
- 나머지는 애플리케이션 버퍼(UART 링버퍼, AT 응답 파싱 버퍼 등, `app_config.h`에서 상수로 관리)

SRAM 256KB 대비 위 사용량은 여유가 충분하므로 별도 MPU 세밀 조정 없이 기본 CubeMX 링커 스크립트
그대로 사용해도 됩니다. 다만 스택 오버플로우 감시(`configCHECK_FOR_STACK_OVERFLOW=2`)는 반드시 켜서
개발 중 스택 크기 부족을 조기에 발견하세요.

## 11. 코드 생성 후 콜백 연결 (USER CODE 영역)

1. **`Core/Src/freertos.c`**
   - CubeMX는 이 파일에 `MX_FREERTOS_Init(void)`(태스크/큐 등 RTOS 오브젝트 생성)와
     `StartDefaultTask(void *argument)`(`defaultTask`의 본체, `osThreadNew`로 생성됨) 두 함수를
     생성합니다. `MX_FREERTOS_Init()` 안의 `/* USER CODE BEGIN RTOS_THREADS */` 영역에서
     본 리포지토리의 `App_FreeRTOS_Init();` 1줄을 호출하세요(`#include "app_freertos.h"` 추가).
     이 함수가 `FPGA_Task`/`ESP32_Task`/`PCComm_Task`/`Config_Task`를 `osThreadNew()`로 직접
     생성하므로, CubeMX Tasks and Queues 탭에서 별도로 태스크를 추가할 필요는 없습니다.

2. **`Core/Src/stm32l5xx_it.c`**
   - `EXTI1_IRQHandler`, `USARTx_IRQHandler` 모두 CubeMX가 생성한 그대로 두면 됩니다
     (내부에서 `HAL_GPIO_EXTI_IRQHandler`/`HAL_UART_IRQHandler`를 호출 → 아래 콜백으로 이어짐).
   - 수정 불필요. `HAL_GPIO_EXTI_Callback()`과 `HAL_UARTEx_RxEventCallback()`은
     `Core/Src/app_it_callbacks.c`(본 리포지토리 신규 파일)에 **한 곳에만** 정의되어 있으며,
     여기서 `esp32_at.c`/`fpga_link.c`/`pc_comm.c`로 이벤트를 나눠 전달합니다
     (HAL weak 콜백은 프로젝트 전체에서 한 번만 정의 가능하므로, 각 드라이버가 직접 정의하지 않고
     이 파일이 대신 dispatch합니다).

3. **`Core/Src/usbd_cdc_if.c`** (USB_DEVICE 미들웨어가 생성)
   - `static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)` 함수의
     `USER CODE BEGIN 6` 영역에서 `PC_Comm_FeedUSB(Buf, *Len);` 호출 추가
     (`#include "pc_comm.h"` 필요). 본 리포지토리의 `pc_comm.c`가 이 함수를 제공합니다.

4. **`Core/Src/freertos.c`의 `StartDefaultTask()`**
   - `for(;;) { ... }` 루프의 `USER CODE BEGIN StartDefaultTask` 영역에서
     `StatusLed_HeartbeatTick();` 호출 추가(`#include "status_led.h"` 필요). `LED_RUN`(PC13)을
     내부적으로 약 500ms 간격(1Hz 토글)으로 켜고 끕니다. 루프의 `osDelay(...)` 값은 CubeMX 기본값을
     그대로 둬도 되며(예: `osDelay(1)`), `StatusLed_HeartbeatTick()`이 자체적으로 경과 시간을
     확인하므로 호출 주기가 짧아도 무방합니다.
   - `LED_WIFI`(PC14)는 `ESP32_Task`(`app_freertos.c`)가 링크 상태가 바뀔 때마다
     `StatusLed_SetWifi()`를 호출해 갱신하므로 여기서는 건드릴 필요 없습니다.

위 5개 지점 외에는 CubeMX 재생성(Generate Code) 시에도 `Core/Inc`, `Core/Src`의 본 리포지토리
파일들은 영향받지 않습니다 (USER CODE 마커 밖에 위치한 신규 파일이므로 CubeMX가 덮어쓰지 않음).
