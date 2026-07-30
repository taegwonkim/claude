# STM32L562 + FreeRTOS 무선 브릿지 (PC 설정 → EEPROM → ESP32-C3 Wi-Fi → Cyclone IV 데이터 전송)

STM32L562를 FreeRTOS로 구동하며, 아래 흐름을 구현하는 애플리케이션 계층 소스입니다.

```
[PC] --USART1(설정,텍스트)--> [STM32L562] --SPI1--> [W25Q40CL 플래시]
                                   |
                                   +--USART2(AT명령)--> [ESP32-C3-WROOM] --Wi-Fi--> [서버]
                                   |
[STM32L562] --USART3(시작명령)-->  [Cyclone IV]
[Cyclone IV] --GPIO(트리거,falling edge)--> [STM32L562]
[Cyclone IV] --USART3(측정데이터)--------->  [STM32L562] --(위 ESP32 경로로 즉시 전달)--> [서버]
```

**설정(1회성, 필요할 때만)**
1. PC가 USART1로 AP SSID/PW, DHCP 여부, 정적 IP/GW/Mask, 서버 IP/Port를 텍스트 명령으로 전송
2. STM32L562가 파싱 후 SPI NOR 플래시(Winbond W25Q40CL)에 저장

**부팅 후 자동으로 계속 도는 메인 루프** (`fpgaIfTask`, `app_fpga_if.c`)
1. ESP32-C3-WROOM의 **MAC 주소를 먼저 조회**(`AT+CIPSTAMAC?`)
2. Wi-Fi AP 접속 + 서버 TCP 접속 (`AT+CWJAP`, `AT+CIPSTART`), 접속 직후 MAC 기반 식별 프레임 1회 전송
3. 무선이 **새로 연결된 첫 순간에 딱 한 번만** Cyclone IV에 측정 시작 명령을 USART3로 전송
   (이후 사이클에서는 재전송하지 않음 — Cyclone IV가 이후 자체 주기로 알아서 측정을 반복)
4. Cyclone IV가 (자체 주기로) 측정 완료 시마다 트리거 GPIO를 low edge로 펄스 →
   STM32L562가 USART3로 측정 데이터 수신
5. 수신 즉시 ESP32-C3-WROOM에 `AT+CIPSEND`로 전달 → 서버로 무선 전송
6. 4번으로 돌아가 **무선 연결이 유지되는 한 계속 반복** (시작 명령 재전송 없음)
7. 도중에 Wi-Fi/서버 연결이 끊기면(전송 실패로 감지) **재접속을 완료할 때까지 대기**한 뒤,
   재연결된 시점에 3번(시작 명령 1회 재전송)부터 다시 시작

이 저장소는 STM32CubeIDE에서 바로 열어 빌드할 수 있는 형태로 애플리케이션 계층
(`app_*`)뿐 아니라 `main.c`/`main.h`/`freertos.c`/`freertos.h`/`FreeRTOSConfig.h`
템플릿까지 포함합니다. 다만 **STM32CubeMX가 보드/실리콘 리비전에 맞춰 정확히
계산해야 하는 두 영역**(`SystemClock_Config()`의 PLL/Flash-latency 값, `MX_DMA_Init()`의
정확한 DMA 채널·DMAMUX 요청 매크로)은 참고용 값이니, CubeMX GUI로 동일 페리페럴을
구성해 생성한 결과로 반드시 교체/검증하세요. 나머지(GPIO AF 매핑, UART/SPI 파라미터,
태스크 배선, HAL 콜백)는 표준 STM32 HAL 패턴을 따르므로 그대로 사용할 수 있습니다.

## 디렉터리 구조

```
Core/Inc/
  main.h                       - Cyclone IV 트리거/EEPROM CS 핀 라벨
  FreeRTOSConfig.h             - FreeRTOS 커널 설정 (우선순위/힙 등)
  freertos.h
  app_config.h    - 핀/UART/버퍼/태스크 우선순위 등 전역 설정
  ring_buffer.h   - UART RX용 바이트 링버퍼
  app_eeprom.h    - 설정 구조체(AppConfig_t) + SPI 플래시 저장/로드
  app_pc_uart.h   - PC 설정 프로토콜 (USART1)
  app_esp32.h     - ESP32-C3-WROOM AT 드라이버/태스크 (USART2)
  app_fpga_if.h   - Cyclone IV 트리거/데이터 수신 (GPIO EXTI + USART3)
  app_tasks.h     - 초기화/태스크 생성 + HAL 콜백 디스패처
Core/Src/
  main.c                       - 진입점 (SystemClock_Config, MX_*_Init, osKernel*)
  freertos.c                   - MX_FREERTOS_Init() → App_Init()/App_CreateTasks()
  app_*.c                      - 위 헤더들의 구현
docs/
  FREERTOS_TASK_MEMORY.md      - 태스크 우선순위·스택·힙 크기 설계 근거
```

## 하드웨어 연결 (기본값, `app_config.h`에서 조정 가능)

| 신호                     | 페리페럴 | 핀              | 설정                              |
|--------------------------|----------|-----------------|-----------------------------------|
| PC 설정 링크             | USART1   | PA9(TX)/PA10(RX)| 115200 8N1, 인터럽트 RX           |
| ESP32-C3-WROOM AT 링크   | USART2   | PA2(TX)/PA3(RX) | 115200 8N1, 인터럽트 RX           |
| Cyclone IV 데이터 링크   | USART3   | PB10(TX)/PB11(RX)| 921600 8N1(조정가능), DMA+IDLE   |
| EEPROM: Winbond W25Q40CL | SPI1     | PA5(SCK)/PA6(MISO)/PA7(MOSI), PA4(CS,GPIO) | Mode 0, Soft NSS, 저속(수 MHz)면 충분 |
| Cyclone IV 트리거        | GPIO EXTI| PB0             | Falling edge, Pull-up 입력        |

## STM32CubeMX 설정 체크리스트

1. **RTOS**: Middleware → FREERTOS 활성화, **Interface = CMSIS_V2** (필수)
   - Config parameters / Advanced settings 값은 `docs/FREERTOS_TASK_MEMORY.md` 5절 참고
   - 기본 생성되는 `defaultTask`는 삭제 (이 프로젝트는 `App_CreateTasks()`가 태스크를 직접 생성)
2. **USART1**: Asynchronous, 115200 8N1, NVIC 인터럽트 활성화
3. **USART2**: Asynchronous, 115200 8N1, NVIC 인터럽트 활성화
4. **USART3**: Asynchronous, 921600 8N1(또는 데이터량에 맞게 조정), **DMA RX 요청 추가(Circular 아님, Normal)**,
   NVIC 인터럽트 활성화 → `HAL_UARTEx_ReceiveToIdle_DMA()` 사용을 위해 필요
5. **SPI1**: Full-Duplex Master, Mode 0(CPOL=Low/CPHA=1Edge), **NSS = Software**(CS는 PA4를 일반 GPIO 출력으로 별도 설정),
   Data Size 8bit, MSB First. W25Q40CL 최대 클럭은 104MHz(표준 Read는 50MHz)이지만 신뢰성 위해 보수적인 값(수 MHz~수십 MHz) 권장
6. **GPIO PB0**: GPIO_EXTI0, Falling edge trigger, Pull-up, NVIC에서 EXTI0 인터럽트 활성화
7. **GPIO PA4**: GPIO_Output, Push-Pull, No pull, 초기 레벨 High (W25Q40CL `/CS`)
8. Clock 설정은 각 UART 보레이트가 오차범위 내에 들어오도록 CubeMX가 자동 계산한 값을 사용

## `main.c` / `freertos.c` 통합 방법

이 저장소의 `Core/Src/main.c`, `Core/Src/freertos.c`가 이미 통합을 반영한 템플릿입니다.
핵심 흐름:

```c
/* main.c */
HAL_Init();
SystemClock_Config();     /* CubeMX 생성본으로 교체/검증 */

MX_GPIO_Init();
MX_DMA_Init();             /* CubeMX 생성본으로 교체/검증 (DMA 채널/요청) */
MX_USART1_UART_Init();
MX_USART2_UART_Init();
MX_USART3_UART_Init();
MX_SPI1_Init();

osKernelInitialize();
MX_FREERTOS_Init();        /* freertos.c: 내부에서 App_Init() + App_CreateTasks() 호출 */
osKernelStart();
```

```c
/* freertos.c */
void MX_FREERTOS_Init(void)
{
    App_Init();          /* app_tasks.h: EEPROM/PC-UART/ESP32/FPGA 모듈 초기화 */
    App_CreateTasks();    /* pcUartTask / esp32Task / fpgaIfTask 생성          */
}
```

`App_Init()`이 내부에서 `osMutexNew()`/`osSemaphoreNew()` 등 FreeRTOS 커널 객체를
생성하므로, **반드시 `osKernelInitialize()` 이후**(= `MX_FREERTOS_Init()` 안)에서만
호출해야 합니다. `main.c`의 `USER CODE BEGIN 2`(커널 초기화 이전)에서 호출하면 안 됩니다.

`HAL_UART_RxCpltCallback`, `HAL_UARTEx_RxEventCallback`, `HAL_GPIO_EXTI_Callback`은
`app_tasks.c`에 이미 정의되어 있습니다. CubeMX가 `main.c`의 `USER CODE` 섹션에 동일한
콜백의 빈 스텁을 생성했다면 **중복 정의를 피하기 위해 반드시 제거**하세요.

태스크 우선순위/스택/힙 크기 설정 근거는 `docs/FREERTOS_TASK_MEMORY.md`를 참고하세요.

## PC 설정 프로토콜 (USART1, 라인 단위 텍스트)

| 명령                        | 설명                                   |
|-----------------------------|----------------------------------------|
| `CFG:SSID=<ssid>`           | AP SSID (최대 32자)                    |
| `CFG:PASS=<password>`       | AP 비밀번호 (최대 64자)                |
| `CFG:DHCP=<0|1>`            | 1=DHCP, 0=고정 IP                      |
| `CFG:IP=<a.b.c.d>`          | 고정 IP (DHCP=0일 때)                  |
| `CFG:GW=<a.b.c.d>`          | 고정 게이트웨이                        |
| `CFG:MASK=<a.b.c.d>`        | 고정 넷마스크                          |
| `CFG:SERVERIP=<a.b.c.d>`    | 서버 IP                                |
| `CFG:SERVERPORT=<1-65535>`  | 서버 TCP 포트                          |
| `CFG:SAVE`                  | EEPROM에 저장 + 자동으로 Wi-Fi/서버 접속 시도 |
| `CFG:CONNECT`               | 저장된 설정으로 즉시 재접속 시도       |

각 명령은 `OK\r\n` 또는 `ERR:<사유>\r\n`으로 응답합니다. 예:

```
> CFG:SSID=MyOfficeAP
< OK
> CFG:PASS=SuperSecret
< OK
> CFG:DHCP=1
< OK
> CFG:SERVERIP=192.168.0.10
< OK
> CFG:SERVERPORT=8080
< OK
> CFG:SAVE
< OK
```

## Cyclone IV 인터페이스 및 측정 루프 (`app_fpga_if.c`, `fpgaIfTask`)

`fpgaIfTask`는 부팅 직후부터 다음 루프를 무한 반복합니다 (의사코드) — **시작 명령은
무선이 새로 연결될 때마다 딱 한 번만** 보내고, 그 뒤로는 Cyclone IV가 스스로의 주기로
보내는 트리거만 계속 기다립니다:

```c
bool linkWasUp = false;

for (;;) {
    if (!App_Esp32_IsConnected()) {
        linkWasUp = false;
        if (!App_Esp32_ConnectAndWait(ESP32_CONNECT_WAIT_TIMEOUT_MS)) {
            osDelay(FPGA_RECONNECT_RETRY_DELAY_MS);   /* 재접속 재시도 대기 */
            continue;
        }
    }

    if (!linkWasUp) {
        Fpga_SendStartCommand();   /* 이번에 새로 연결됐을 때만 USART3로 시작 명령 1회 */
        linkWasUp = true;
    }

    if (trigger 대기 실패 (FPGA_TRIGGER_WAIT_TIMEOUT_MS 초과))
        continue;   /* 아직 측정 주기가 안 됐을 뿐 - 시작 명령 재전송 없이 계속 대기 */

    USART3 DMA(IDLE 검출)로 측정 데이터 수신;
    App_Esp32_SendMeasurementData(data, len, ...);        /* 서버로 전달 */
}
```

- **시작 명령(연결당 1회)**: `FPGA_CMD_START_MEASURE`(`app_config.h`, 기본 `0x01` 1바이트)를
  USART3로 전송합니다. `linkWasUp` 플래그로 "방금 (재)연결됐는지"를 추적해, 무선이 끊기지 않고
  유지되는 동안에는 다시 보내지 않습니다. Cyclone IV 쪽에 실제로 정의된 커맨드 코드/프레임
  포맷이 있다면 이 값과 `Fpga_SendStartCommand()`를 그에 맞게 수정하세요.
- **측정 완료 신호(주기적, FPGA가 페이싱)**: 시작 명령 이후 Cyclone IV는 자체 주기로 알아서
  측정을 반복하며, 완료 시마다 트리거 GPIO(PB0)를 **low edge**로 펄스합니다 (EXTI falling
  edge). `FPGA_TRIGGER_WAIT_TIMEOUT_MS`(기본 30000ms)는 "이 시간 안에 반드시 와야 하는 마감"이
  아니라 단순히 대기 중 주기적으로 링크 상태를 재확인하기 위한 값입니다 — 타임아웃돼도
  에러가 아니라 그냥 계속 대기합니다.
- **데이터 수신**: 트리거 직후 USART3 DMA 수신(`HAL_UARTEx_ReceiveToIdle_DMA`, IDLE 라인 검출)을
  준비해 Cyclone IV가 뒤이어 보내는 측정 데이터를 받습니다. 프레임 최대 길이는
  `FPGA_FRAME_MAX_LEN`(기본 1024바이트), 트리거 후 `FPGA_RX_TOTAL_TIMEOUT_MS`(기본 1000ms) 내에
  데이터가 도착하지 않으면 수신을 중단합니다.
- 수신된 프레임은 별도 가공 없이 그대로 `App_Esp32_SendMeasurementData()`로 전달되어
  ESP32-C3-WROOM을 통해 서버로 전송됩니다. Cyclone IV 쪽 프레임 포맷(헤더/길이/체크섬 등)이
  정해져 있다면 수신 완료 지점에서 해당 포맷대로 파싱/검증을 추가하세요.
- **무선 재접속 + 시작 명령 재전송**: 전송이 실패하면(`App_Esp32_SendMeasurementData()`가
  false 반환) ESP32 모듈이 내부적으로 연결 플래그를 내리고, 다음 루프 반복의 맨 앞에서
  `linkWasUp`도 `false`로 리셋됩니다. 이후 `App_Esp32_IsConnected()`가 다시 true가 되는 시점
  (=재접속 완료)에 시작 명령이 자동으로 한 번 더 전송되어 Cyclone IV의 측정 사이클을
  다시 킥오프합니다 — 별도의 사용자 개입이 필요 없습니다.
- **제약**: 트리거 세마포어가 이진(binary)이라, STM32가 이전 측정을 처리 중일 때 Cyclone IV가
  두 번째 트리거를 보내면 그 펄스는 유실됩니다. Cyclone IV의 측정 주기가 한 사이클
  (수신+`AT+CIPSEND` 왕복 시간)보다 충분히 길다는 전제이며, 그렇지 않다면 트리거를 큐잉하는
  카운팅 세마포어나 FPGA 쪽 흐름 제어(예: STM32의 ACK 대기) 추가를 검토하세요.

## ESP32-C3-WROOM AT 시퀀스 (USART2)

접속(`App_Esp32_RequestConnect()` / `App_Esp32_ConnectAndWait()` → 내부 `Esp32_DoConnect()`):

```
AT                                   (모듈 응답 확인, 최대 3회 재시도)
ATE0                                 (에코 끄기)
AT+CWMODE=1                          (Station 모드)
AT+CIPSTAMAC?                        (① MAC 주소 조회 - 가장 먼저 수행)
AT+CWDHCP=1,1  또는  AT+CWDHCP=1,0 + AT+CIPSTA="ip","gw","mask"
AT+CWJAP="<ssid>","<pass>"           (② Wi-Fi AP 접속)
AT+CIPMUX=0
AT+CIPSTART="TCP","<server_ip>",<server_port>   (③ 서버 TCP 접속)
AT+CIPSEND=...  "ID:<mac>"           (④ 접속 직후 식별 프레임 1회 전송, 실패해도 무시)
```

데이터 전송(`App_Esp32_SendMeasurementData()` → 내부 `Esp32_DoSend()`):

```
AT+CIPSEND=<len>       ('>' 프롬프트 대기)
<raw payload bytes>
                        ("SEND OK" 대기)
```

링크가 끊겨 있으면 전송 요청 시 자동으로 재접속을 한 번 시도한 뒤 전송합니다.
읽어온 MAC 주소는 `App_Esp32_GetMacAddress()`로 다른 모듈에서도 조회할 수 있습니다.

## EEPROM(W25Q40CL SPI NOR 플래시) 레이아웃 및 저장 절차

`AppConfig_t`(`app_eeprom.h`, 189바이트)가 플래시 주소 `0x000000`(섹터 0, 페이지 0)에
저장됩니다: magic(4B) + SSID/PASS/DHCP/고정IP 3종/서버IP/포트 + CRC32(4B).

W25Q40CL은 **NOR 플래시**라서 EEPROM과 달리 임의 바이트 덮어쓰기가 안 됩니다:
- Page Program(`0x02`)은 비트를 1→0으로만 바꿀 수 있습니다.
- 다시 0→1로 되돌리려면(=재기록하려면) 먼저 **4KB 단위 Sector Erase(`0x20`)**로 섹터
  전체를 지워(모든 비트 1) 두어야 합니다.

그래서 `App_Eeprom_SaveConfig()`는 매 저장마다 다음 순서로 동작합니다 (`app_eeprom.c`):

1. `AT+`... 아님 — `Flash_SectorErase(0x000000)`: 섹터 0(4KB) 전체 소거, WEL 설정 후
   BUSY 비트가 내려갈 때까지 폴링 (최대 `EEPROM_ERASE_TIMEOUT_MS`=500ms 대기)
2. `Flash_PageProgram(0x000000, cfg, sizeof(cfg))`: `AppConfig_t` 전체(189B, 256B
   페이지 이내)를 한 번의 Page Program으로 기록 (최대 `EEPROM_PROGRAM_TIMEOUT_MS`=100ms 대기)

로드 시(`Flash_ReadData`, opcode `0x03`)는 소거가 필요 없는 단순 읽기이며, magic/CRC가
맞지 않으면(공장 출하 상태 등) 기본값(빈 SSID, DHCP 활성)으로 대체합니다.

`CFG:SAVE` 처리는 `pcUartTask` 컨텍스트에서 동기적으로 이 소거+기록을 수행하므로
최대 수백 ms 블로킹될 수 있습니다 (다른 태스크는 별도 태스크이므로 영향 없음).

## 설계상 가정 / 조정 포인트

- Cyclone IV ↔ STM32L562 데이터 프레임의 상세 포맷(헤더, 길이 필드, 체크섬 유무)은
  명시되지 않아 "트리거 후 들어오는 바이트 스트림을 IDLE 라인까지 그대로 한 프레임으로 간주"
  하는 방식으로 구현했습니다. 실제 FPGA 출력 포맷이 있다면 `app_fpga_if.c`에 파싱 로직을
  추가하세요.
- ESP32 AT 펌웨어 버전에 따라 일부 명령 문법이 다를 수 있습니다 (`AT+CWJAP` 응답이
  `WIFI CONNECTED` / `WIFI GOT IP` / `OK` 등으로 나뉘는 펌웨어도 있음) — 사용 중인 AT
  펌웨어의 정확한 응답 토큰에 맞춰 `app_esp32.c`의 `Esp32_SendCommand()` 호출부 토큰을
  조정하십시오.
- USART3 보레이트(921600)는 측정 데이터량/주기에 맞춰 조정하세요.
- 서버로의 전송 실패/재시도 정책(예: 큐잉, 재전송 횟수)은 현재 "전송 실패한 측정값은
  버리고, 다음 루프 반복에서 재접속 후 새 측정 사이클부터 재개"로 단순화되어 있습니다.
  실패한 측정값 자체를 재전송해야 하는 응용이라면 `app_fpga_if.c`에 로컬 버퍼링/재시도
  로직을 추가해야 합니다.
- `FPGA_CMD_START_MEASURE`(기본 `0x01`)는 Cyclone IV와 아직 확정되지 않은 임의의 1바이트
  플레이스홀더 명령입니다. 실제 시작 명령 프로토콜(옵코드, 파라미터, 응답 유무 등)이 있다면
  `app_fpga_if.c`의 `Fpga_SendStartCommand()`를 그에 맞게 수정하세요.
- ESP32에 보내는 식별 프레임(`"ID:<mac>\r\n"`)은 서버가 이를 요구한다는 명시적 스펙이 없어
  임의로 추가한 것입니다. 서버 프로토콜에 맞는 형식이 따로 있다면 `app_esp32.c`의
  `Esp32_DoConnect()` 마지막 블록을 수정/제거하세요.
- W25Q40CL SPI 클럭은 `main.c`의 `MX_SPI1_Init()`에서 보수적인 프리스케일러
  (`SPI_BAUDRATEPRESCALER_16`)를 기본값으로 사용합니다. 실제 배선 길이/노이즈 환경에
  따라 CubeMX Clock Configuration에서 계산되는 SPI1 클럭 기준으로 필요 시 더 높여도 됩니다
  (칩 최대 104MHz, 표준 Read(`0x03`) 명령은 최대 50MHz 제한이 있으니 유의).
- `main.c`의 `SystemClock_Config()`(PLL/Flash latency)와 `MX_DMA_Init()`(USART3 RX DMA
  채널/DMAMUX 요청 매크로)은 참고값입니다 — 위 "통합 방법"에서 언급했듯 CubeMX GUI로
  생성한 값으로 반드시 교체/검증하세요.
