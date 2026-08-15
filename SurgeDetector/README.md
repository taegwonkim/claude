# SurgeDetector — STM32L562CET6 + FreeRTOS 펌웨어

FPGA(Cyclone IV) 기반 6채널 서지(surge) 피크 검출 시스템의 MCU 펌웨어.
STM32CubeMX 설정과 STM32CubeIDE 애플리케이션 코드로 구성됩니다.

- MCU: **STM32L562CET6** (Cortex‑M33, 110MHz, TrustZone **비활성화**로 사용)
- RTOS: **FreeRTOS** (CMSIS‑RTOS2 API, CubeMX 자동생성 스타일 유지)
- Toolchain: STM32CubeIDE (STM32CubeMX 통합)

> 이 저장소의 `.ioc`, `Core/`, `App/` 은 STM32CubeIDE에서 "Existing STM32CubeMX
> Configuration File(.ioc) 가져오기"로 새 프로젝트를 만든 뒤, `App/` 이하 코드를
> 프로젝트에 추가(Add Existing Files)하는 방식으로 사용합니다. `.ioc`를 코드 생성
> (Generate Code)하면 `Core/Src/main.c`, `stm32l5xx_it.c`, `usart.c`, `spi.c`,
> `usb_device.c` 등이 CubeMX 버전에 맞춰 재생성되므로, 본 저장소의 `Core/*`
> 파일들은 **CubeMX가 만들지 않는 애플리케이션 훅(hook) 코드만** 담고 있습니다.

---

## 1. 시스템 개요

```
 PC ── USART3(RS485) ──┐                      ┌── USART1(TTL,115200) ── ESP32-C3-WROOM ── WiFi ── Server(TCP:50001)
 PC ── USB(CDC) ───────┼── STM32L562CET6 ─────┤
                        │   (FreeRTOS)         └── USART2(TTL) + EXTI(trigger) ── Cyclone IV FPGA ── 외부 ADC x6ch
                        │
                        └── SPI2 ── W25Q40CLS (설정값 저장용 외부 NOR Flash)
```

### 1.1 데이터 흐름
1. 부팅 후 `defaultTask` 가 SPI Flash에서 설정(WiFi SSID/PW, 서버 IP/Port, DHCP 모드)을 로드.
2. `fpgaTask` 가 USART2로 `"START\r\n"` 을 **최초 1회** 전송 → FPGA가 주기적(예: 1초)으로
   ADC 6채널을 취득 시작.
3. FPGA는 데이터가 준비될 때마다 **트리거 신호(GPIO, falling edge)** 를 먼저 MCU에 보내고,
   곧이어 USART2로 6채널 데이터 프레임을 전송.
4. MCU는 EXTI(falling edge)로 깨어나 USART2 DMA로 데이터 프레임을 수신 → 파싱.
5. 파싱된 데이터는 두 갈래로 전달:
   - `wifiTask` → ESP32 AT 명령(`AT+CIPSEND`)으로 서버 전송
   - `pcUartTask`/`pcUsbTask` → USART3(RS485)/USB(CDC)로 PC에 전송
6. PC는 언제든 USART3 또는 USB로 **설정 write/read 커맨드**를 보낼 수 있음 → 파싱 후
   SPI Flash에 반영, WiFi 모듈 재설정.
7. WiFi 연결이 끊기면 `wifiTask`(및 감시용 소프트웨어 타이머)가 자동 재연결 수행.

---

## 2. 하드웨어 인터페이스 / 핀 매핑

| 기능 | 페리페럴 | 핀(예시, 보드에 맞게 조정) | 비고 |
|---|---|---|---|
| ESP32-C3 AT 통신 | USART1 | PA9(TX) / PA10(RX) | TTL 3.3V, 115200-8N1, DMA RX(IDLE) |
| ESP32 EN(리셋) | GPIO Output | PB0 | Low pulse ≥100ms 로 하드리셋 |
| ESP32 부팅 상태 감지(옵션) | GPIO Input | PB1 | 필요 시 |
| FPGA 통신 | USART2 | PA2(TX) / PA3(RX) | TTL, 115200(또는 FPGA 사양에 맞춤)-8N1, DMA RX |
| FPGA 데이터 준비 트리거 | GPIO EXTI | PB4 (EXTI4) | Falling edge 인터럽트 |
| PC RS485 | USART3 | PB10(TX)/PB11(RX) | RS485 트랜시버, DE/RE 제어핀 PB12, 115200-8N1 |
| PC USB CDC | USB FS | PA11(DM)/PA12(DP) | Virtual COM Port |
| 외부 Flash W25Q40CLS | SPI2 | PB13(SCK)/PB14(MISO)/PB15(MOSI), CS=PB12 대신 다른 핀(예 PC6) | 40MHz 이하 권장(예 20MHz), Mode0 |
| 상태 LED | GPIO Output | PC13 | 하트비트, 500ms 토글 |
| WiFi 상태 LED(옵션) | GPIO Output | PC14 | 서버 연결시 On |
| 디버그 SWD | SYS | PA13/PA14 | |

> RS485와 SPI가 동일 포트(B)의 핀을 겹쳐 예시했으므로 **실제 보드 핀아웃에 맞춰
> CubeMX Pinout view에서 재배치**하십시오. 위 표는 CubeMX에서 그대로 입력할 수 있는
> 대표값입니다.

---

## 3. STM32CubeMX 설정

### 3.1 RCC / Clock Tree
- HSE: 미실장 보드 가정 시 **MSI 발진기**를 시스템 클록 소스로 사용 (보드에 8/16MHz 크리스탈이
  있다면 HSE 사용 권장, 정확한 USART/USB 보레이트를 위해 HSE 권장)
- USB Device 사용 시 48MHz 클록이 정확해야 하므로:
  - HSE 있는 보드: `PLL` → SYSCLK 110MHz, `PLLQ`(또는 USB용 PLL)로 48MHz 생성
  - HSE 없는 보드: MSI를 48MHz로 트리밍(HSI48 사용 가능하면 HSI48 → USB 클록소스로 선택,
    STM32L5는 **HSI48 내장** 보유 → USB 클록 소스로 HSI48 선택이 가장 간단)
- SYSCLK = 110MHz, HCLK = 110MHz, APB1 = 110MHz, APB2 = 110MHz (필요 시 저전력 요구사항에
  맞춰 하향 조정)
- Clock Configuration 탭에서 USART1/2/3 클록소스는 PCLK 또는 별도 `SYSCLK`로 선택 가능(정확도
  중요 시 HSI16 고정 소스 권장)

### 3.2 System Core
- **RCC**: 위와 같이 설정
- **NVIC**: Priority Group = **4 bits for pre-emption priority, 0 bits for subpriority**
  (`NVIC_PRIORITYGROUP_4`, CubeMX 기본값)
- **GPIO**: 2.1절 핀 맵대로 설정
  - EXTI4(PB4): GPIO_MODE_IT_FALLING, Pull-up, NVIC priority **5**(아래 3.5 참조)
  - RS485 DE/RE(PB12): Output Push-Pull
  - ESP32 EN(PB0): Output Push-Pull, 초기값 High
- **SYS**: Debug = Serial Wire, Timebase Source = **TIM6**(FreeRTOS가 SysTick을 점유하므로
  HAL 타임베이스는 반드시 SysTick 이외의 타이머로 변경! CubeMX가 FreeRTOS 추가 시 자동 권고)

### 3.3 USART1 — ESP32-C3 AT 통신
- Mode: Asynchronous, 115200-8-N-1
- DMA: **USART1_RX** → DMA1 Channel, Circular 모드 비권장(가변 길이 응답) → **Normal + IDLE Line
  Detection** 방식 사용 (`HAL_UARTEx_ReceiveToIdle_DMA`)
- NVIC: USART1 global interrupt Enable, DMA1 channel interrupt Enable, 우선순위 5
- Parameter Settings: Over sampling 16, 기본

### 3.4 USART2 — FPGA 통신
- Mode: Asynchronous, 115200-8-N-1 (FPGA UART IP 사양에 맞춰 조정, 예 921600 등 고속도 가능)
- DMA: USART2_RX → **Normal 모드**, 고정 프레임 길이 수신(`HAL_UART_Receive_DMA`) 또는
  IDLE 라인 검출(가변 길이일 경우)
- TX: 폴링 또는 인터럽트(짧은 `"START\r\n"` 1회 전송이라 폴링으로 충분)
- NVIC 우선순위 4 (FPGA 데이터는 상대적으로 시간에 민감하므로 WiFi보다 약간 높게)

### 3.5 USART3 — PC RS485
- Mode: Asynchronous, 115200-8-N-1, **Driver Enable(DE) 기능 활성화 가능**
  (L5 USART는 `DEM`(Driver Enable Mode) 하드웨어 지원 → GPIO 토글 없이 자동 half‑duplex 제어 가능.
  Configuration → Advanced Parameters → Driver Enable = Enable, DE Polarity = High,
  DE Assertion/De-assertion Time 설정)
- DMA RX: IDLE Line Detection 방식 (`HAL_UARTEx_ReceiveToIdle_DMA`), 프레임 경계 검출용
- NVIC 우선순위 5

### 3.6 SPI2 — W25Q40CLS 외부 Flash
- Mode: Full-Duplex Master
- Prescaler: SPI 클록 ≤ 40MHz 이하로 설정(예 PCLK/4 → 20MHz대)
- CPOL = Low, CPHA = 1 Edge (Mode 0)
- Data Size = 8bit, MSB First
- NSS: **Software 관리**(GPIO 수동 제어) 권장 — CubeMX에서 CS 핀은 일반 GPIO Output으로 별도 설정
- Hardware CRC 미사용(SW CRC로 무결성 체크, 4장 프로토콜 참조)

### 3.7 USB Device — Virtual COM Port(CDC)
- Middleware → USB_DEVICE → Class = **Communication Device Class (Virtual Port Com)**
- USB_DEVICE Parameter: Device Descriptor에 VID/PID/문자열 지정
- `usbd_cdc_if.c` 의 `CDC_Receive_FS()` 콜백에서 수신 바이트를 `app_pccomm` 모듈의
  스트림 버퍼로 전달하도록 훅 추가 (`USB_DEVICE/App/usbd_cdc_if_hooks.c` 참조)

### 3.8 Middleware → FreeRTOS (핵심)

| 항목 | 설정값 | 비고 |
|---|---|---|
| Interface | **CMSIS_V2** | osThreadNew/osMessageQueueNew 등 CMSIS-RTOS2 API 사용 |
| USE_NEWLIB_REENTRANT | Enabled | printf 등 재진입 안전 |
| Memory Management scheme | **heap_4** | 단편화 방지용 병합 지원, TX/RX 버퍼 동적할당에 적합 |
| TOTAL_HEAP_SIZE | **20480 (0x5000) bytes** | 태스크 스택 합 + 큐/버퍼 여유분 고려, 필요시 조정 |
| Tick Rate (Hz) | **1000** | 1ms tick |
| CHECK_FOR_STACK_OVERFLOW | Method 2 | 개발 단계 필수, 양산시 유지 권장 |
| USE_TIMERS | Enabled | 워치독/하트비트 소프트웨어 타이머용 |
| TIMER_TASK_PRIORITY | osPriorityHigh(=configTIMER_TASK_PRIORITY) | |
| USE_MUTEXES | Enabled | Flash/UART 보호용 |
| USE_COUNTING_SEMAPHORES | Enabled | |
| USE_RECURSIVE_MUTEXES | Enabled | |
| configMAX_SYSCALL_INTERRUPT_PRIORITY | 5 | ISR에서 FreeRTOS API 호출 허용 최저 우선순위(숫자 클수록 낮은 실제 우선순위) |
| Config USE_PORT_OPTIMISED_TASK_SELECTION | Enabled | |
| USE_APPLICATION_TASK_TAG | Disabled | |

> NVIC 우선순위 규칙: FreeRTOS API(`xQueueSendFromISR` 등)를 호출하는 모든 ISR
> (EXTI4, USART1/2/3 IRQ, DMA IRQ)의 선점 우선순위는 반드시 **5 이상(숫자)**,
> 즉 `configMAX_SYSCALL_INTERRUPT_PRIORITY(=5)` 보다 낮은 실제 우선순위(=큰 숫자)로
> 설정해야 합니다. SysTick/PendSV/SVC는 CubeMX/FreeRTOS 포트가 자동 관리합니다.

#### 3.8.1 Tasks (Config tab → Tasks and Queues)

| Task 이름 | 함수 | 우선순위(osPriority) | Stack(words) | 생성 방식 | 역할 |
|---|---|---|---|---|---|
| defaultTask | `StartDefaultTask` | osPriorityNormal | 256 | CubeMX 기본 + App Init 호출 | Flash 설정 로드, 각 모듈 Init, 하트비트 LED 감시 |
| fpgaTask | `App_FPGA_Task` | **osPriorityHigh** | 512 | `app_fpga_init()`에서 생성 | FPGA 트리거 대기 → 데이터 수신/파싱/배포 |
| wifiTask | `App_WiFi_Task` | osPriorityAboveNormal | 768 | `app_wifi_init()`에서 생성 | ESP32 AT 시퀀스, 서버 접속/재접속, 데이터 송신 |
| pcUartTask | `App_PcUart_Task` | osPriorityNormal | 512 | `app_pccomm_init()`에서 생성 | USART3(RS485) 커맨드 파싱/응답 |
| pcUsbTask | `App_PcUsb_Task` | osPriorityNormal | 512 | `app_pccomm_init()`에서 생성 | USB CDC 커맨드 파싱/응답 |
| pcDataPushTask | `App_PcDataPush_Task` | osPriorityNormal | 384 | `app_pccomm_init()`에서 생성 | `qFpgaToPc` 소비 → RS485+USB로 서지데이터 동시 통지(단일 소비자로 큐를 비우고 양쪽 인터페이스에 순차 전송) |

(CubeMX Tasks and Queues 탭에서 `defaultTask`만 GUI로 만들고, 나머지 4개 태스크는
`osThreadNew()`를 **애플리케이션 코드(App/Src/*.c의 Init 함수)** 에서 생성합니다. 이렇게
분리하면 각 모듈이 자신의 RTOS 오브젝트를 캡슐화하여 관리하기 쉬워집니다. 물론 CubeMX
GUI의 Tasks and Queues 탭에 전부 등록해 `MX_FREERTOS_Init()` 한 곳에서 생성해도 무방합니다 —
`Core/Src/app_freertos.c` 에 두 방식 모두 동작하도록 구성해 두었습니다.)

#### 3.8.2 Queues

| Queue 이름 | 항목 타입 | 길이 | 생성자/소비자 | 용도 |
|---|---|---|---|---|
| `qFpgaToWifi` | `SurgeData_t` | 8 | fpgaTask → wifiTask | 서버 전송용 서지 데이터 |
| `qFpgaToPc` | `SurgeData_t` | 8 | fpgaTask → pcUartTask/pcUsbTask(브로드캐스트 배포) | PC 표시/저장용 서지 데이터 |
| `qWifiCmd` | `WifiCmdMsg_t` | 4 | pcUartTask/pcUsbTask → wifiTask | 설정변경 시 WiFi 재접속 지시 |
| `qPcUartTx` | `PcFrame_t*` | 8 | 내부 응답 큐 | RS485 응답 프레임 전송 큐 |
| `qPcUsbTx` | `PcFrame_t*` | 8 | 내부 응답 큐 | USB 응답 프레임 전송 큐 |

큐가 가득 찬 상태(WiFi 단절 등)에서 `fpgaTask`는 **0-timeout으로 osMessageQueuePut** 시도 후
실패하면 가장 오래된 데이터를 버리고 최신 데이터를 우선하는 정책(`overwrite-oldest`)을
적용합니다 (`app_fpga.c`의 `FPGA_PushOrDropOldest()` 참조).

#### 3.8.3 Semaphore / Mutex

| 이름 | 타입 | 초기값 | Give(발생) | Take(대기) | 용도 |
|---|---|---|---|---|---|
| `semFpgaTrigger` | Binary | 0 | EXTI4 ISR | fpgaTask | FPGA 트리거(falling edge) 통지 |
| `semUart2RxCplt` | Binary | 0 | USART2 DMA RX Complete/IDLE 콜백 | fpgaTask | 데이터 프레임 수신 완료 통지 |
| `semUart1Event` | Binary | 0 | USART1 IDLE 콜백 | wifiTask(AT 응답 대기) | ESP32 응답 수신 통지 |
| `semUart3Event` | Binary | 0 | USART3 IDLE 콜백 | pcUartTask | RS485 프레임 수신 완료 통지 |
| `mutexFlash` | Mutex | - | - | app_config, w25qxx 전 함수 | SPI Flash 동시접근 보호 |
| `mutexWifiUart` | Mutex | - | - | AT 명령 송신 시 | USART1 송신 중 재진입 방지 |

#### 3.8.4 Software Timers

| 이름 | 주기 | 타입 | 콜백 | 역할 |
|---|---|---|---|---|
| `tmrHeartbeat` | 500ms | Periodic | `Heartbeat_Callback` | 상태 LED 토글 |
| `tmrWifiWatchdog` | 5000ms | Periodic | `WifiWatchdog_Callback` | 서버 연결 상태 확인, 끊김 시 `qWifiCmd`에 재접속 명령 투입 |

#### 3.8.5 Event Group

`egSysStatus` 비트 정의 (`App/Inc/app_config.h`):
```
EVT_FLASH_READY      (1<<0)
EVT_CONFIG_LOADED    (1<<1)
EVT_WIFI_JOINED      (1<<2)
EVT_SERVER_CONNECTED (1<<3)
EVT_USB_ENUMERATED   (1<<4)
```
`defaultTask`는 `EVT_FLASH_READY | EVT_CONFIG_LOADED` 를 기다린 뒤 나머지 태스크에
동작 허가를 주는 방식으로 초기화 순서를 보장합니다.

---

## 4. 통신 프로토콜

### 4.1 PC ↔ MCU 설정 프로토콜 (USART3/USB 공용, `App/Inc/protocol.h`)

```
+-----+-----+--------+----------------+--------+-----+
| STX | LEN | CMD ID |     PAYLOAD    | CRC16  | ETX |
| 1B  | 1B  |  1B    |   0~250 Bytes  |  2B    | 1B  |
+-----+-----+--------+----------------+--------+-----+
STX=0x02, ETX=0x03, LEN=CMD+PAYLOAD 길이, CRC16-CCITT(0xFFFF init) over (CMD+PAYLOAD)
```

| CMD ID | 이름 | 방향 | Payload |
|---|---|---|---|
| 0x10 | CMD_WRITE_WIFI_CFG | PC→MCU | ssid[32], password[64] |
| 0x11 | CMD_READ_WIFI_CFG | PC→MCU (요청) / MCU→PC(응답) | ssid[32], password[64] |
| 0x12 | CMD_WRITE_SERVER_CFG | PC→MCU | server_ip[4], server_port(u16) |
| 0x13 | CMD_READ_SERVER_CFG | 요청/응답 | server_ip[4], server_port(u16) |
| 0x14 | CMD_WRITE_NET_CFG | PC→MCU | dhcp_enable(u8), ip[4], gw[4], mask[4] |
| 0x15 | CMD_READ_NET_CFG | 요청/응답 | dhcp_enable(u8), ip[4], gw[4], mask[4] |
| 0x20 | CMD_READ_STATUS | 요청/응답 | wifi_joined, server_connected, fpga_running 등 |
| 0x30 | CMD_DATA_PUSH | MCU→PC | `SurgeData_t` (서지 측정값 실시간 통지) |
| 0x7E | CMD_ACK | MCU→PC | 없음 |
| 0x7F | CMD_NACK | MCU→PC | error_code(u8) |

### 4.2 FPGA ↔ MCU 데이터 프레임 (USART2, `App/Inc/protocol.h`의 `FPGA_FRAME_*`)

```
+--------+---------+------------------------+--------+--------+
| 0xAA55 | SEQ(u16)| CH0..CH5 raw(u16 x 6)  | CRC16  | 0x0D0A |
|  2B    |  2B     |        12B             |  2B    |  2B    |
+--------+---------+------------------------+--------+--------+
```
- FPGA는 GPIO 트리거(falling edge)를 먼저 내리고, 즉시 위 20바이트 프레임을 USART2로 송신.
- MCU는 `"START\r\n"` 전송 이후 이 시퀀스를 무한 반복 수신.

### 4.3 서버 전송 (ESP32 AT, `App/Src/app_wifi.c`)
1. `AT` (에코/응답 확인)
2. `ATE0` (에코 off)
3. `AT+CWMODE=1` (Station)
4. `AT+CWJAP="<ssid>","<password>"`
5. `AT+CIPSTART="TCP","<server_ip>",<server_port>`
6. 데이터 발생 시: `AT+CIPSEND=<len>` → `SurgeData_t`를 서버 프로토콜(JSON 또는 바이너리, 필요시
   협의)로 직렬화하여 전송
7. `AT+CIPCLOSE` / `WIFI DISCONNECT` 비동기 URC 수신 시 재접속 루틴 진입

---

## 5. 디렉터리 구조

```
SurgeDetector/
├── SurgeDetector.ioc              # STM32CubeMX 설정 파일
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── FreeRTOSConfig.h
│   │   └── stm32l5xx_it.h
│   └── Src/
│       ├── main.c                 # CubeMX 표준 구조 + App Init 훅
│       ├── app_freertos.c         # MX_FREERTOS_Init (태스크/큐/세마포어/타이머 생성)
│       └── stm32l5xx_it.c         # EXTI/UART/DMA ISR → 세마포어 Give
├── App/
│   ├── Inc/
│   │   ├── protocol.h             # 공용 프레임/커맨드 정의
│   │   ├── app_config.h           # 설정 구조체 + Flash 저장 API + EventGroup
│   │   ├── app_wifi.h
│   │   ├── app_fpga.h
│   │   └── app_pccomm.h
│   └── Src/
│       ├── app_config.c
│       ├── app_wifi.c
│       ├── app_fpga.c
│       └── app_pccomm.c
├── Drivers/BSP/W25Qxx/
│   ├── w25qxx.h
│   └── w25qxx.c
└── USB_DEVICE/App/
    └── usbd_cdc_if_hooks.c        # CDC_Receive_FS 훅 추가 코드 스니펫
```

## 6. 빌드/통합 절차
1. STM32CubeIDE → File → New → STM32 Project → MCU `STM32L562CETx` 선택 (또는
   `SurgeDetector.ioc` 더블클릭으로 신규 프로젝트 생성).
2. CubeMX Pinout/Clock/Middleware 탭에서 본 문서 3장 내용대로 설정 후 **Generate Code**.
3. 생성된 프로젝트에 `App/`, `Drivers/BSP/W25Qxx/` 폴더를 복사하고, IDE에서
   Project → Properties → C/C++ Build → Include Path에 `App/Inc`, `Drivers/BSP/W25Qxx` 추가.
4. `Core/Src/main.c` 의 `USER CODE BEGIN` 영역에 본 저장소 `main.c`의 훅 코드를 병합.
5. `stm32l5xx_it.c`의 해당 ISR `USER CODE` 영역에 본 저장소 코드를 병합.
6. `usbd_cdc_if.c`의 `CDC_Receive_FS()`에 `usbd_cdc_if_hooks.c` 내용을 병합.
7. Build → Flash.

## 7. 주의사항
- STM32L562의 TrustZone은 본 설계에서는 **비활성화**(Secure/Non-secure 분리 불필요) 가정입니다.
  보안 요구가 있다면 별도 TZ 파티셔닝 설계가 필요합니다.
- HAL 타임베이스를 반드시 SysTick이 아닌 TIM6 등으로 변경해야 FreeRTOS와 충돌하지 않습니다
  (CubeMX가 FreeRTOS 추가 시 자동으로 권고/변경).
- 외부 Flash 설정 저장은 **Sector 단위 소거 후 재기록** 방식이며, `app_config.c`에서 더블 버퍼
  (Sector0/Sector1 핑퐁 + 버전 카운터)로 전원 순단 시에도 마지막 정상 설정을 보존합니다.
- 워치독(IWDG) 추가를 권장하며, `defaultTask`에서 각 태스크의 마지막 동작 타임스탬프를
  점검해 하나라도 응답없음(hang) 상태면 `HAL_IWDG_Refresh()`를 멈춰 리셋을 유도하는
  방식(Task-level watchdog)을 권장합니다. (본 저장소 스켈레톤에는 후크만 준비되어 있으며,
  실제 IWDG 설정은 CubeMX IWDG 탭에서 활성화 후 `defaultTask`에 갱신 코드를 추가하십시오.)
