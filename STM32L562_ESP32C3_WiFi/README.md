# STM32L562CET6 + ESP32-C3-WROOM (ESP-AT) — Wi-Fi AP / TCP 서버 접속 및 자동 재접속

STM32CubeMX / STM32CubeIDE 기반. STM32L562CET6 가 **USART1 로 ESP32-C3 에
AT 명령을 보내고 응답을 폴링 방식으로 받아** Wi-Fi AP 와 TCP 서버에 접속합니다.
AP 나 서버가 끊기면 스스로 다시 붙습니다.

- 대상 MCU : **STM32L562CET6 (UFQFPN48, Flash 512KB)**, TrustZone Disabled
- Wi-Fi 모듈 : **ESP32-C3-WROOM-02** + Espressif **ESP-AT 펌웨어 (v2.x 이상)**
- 통신 : USART1 115200-8-N-1, **인터럽트/DMA 없이 폴링** (SysTick 1ms 틱만 사용)
- 순서 : 모듈 하드웨어 리셋 → AT 초기화 → **MAC 주소 읽기** → **AP 접속(SSID/PW)**
  (**DHCP ON** 또는 **DHCP OFF + 고정 IP/게이트웨이/넷마스크**) → **TCP 서버 접속(IP/PORT)**
- 재접속 : `WIFI DISCONNECT` / `CLOSED` URC + 주기적 `AT+CIPSTATUS` 확인 → 자동 재시도,
  연속 실패 시 모듈 하드웨어 리셋
- 로그 : USART2 (PC 터미널) 로 AT 송수신 내용과 상태 전이를 모두 출력

---

## 1. 배선

```
 STM32L562CET6                         ESP32-C3-WROOM-02 (ESP-AT)
 ─────────────                         ─────────────────────────
 PA9   USART1_TX  ───────────────────▶ GPIO6   (AT UART RX)
 PA10  USART1_RX  ◀─────────────────── GPIO7   (AT UART TX)
 PB0   ESP_EN     ───────────────────▶ EN      (칩 인에이블 = 하드웨어 리셋)
                                        GPIO5   (CTS) ──▶ GND   ※ 흐름제어 미사용
                                        GPIO4   (RTS)   open
                                        GPIO8   ──▶ 10k 풀업 (부팅 모드)
                                        GPIO9   ──▶ 10k 풀업 (부팅 모드, 부팅 시 H)
 3V3 ──────────────────────────────────▶ 3V3     ※ 500mA 이상 공급 가능해야 함
 GND ──────────────────────────────────▶ GND

 PA2   USART2_TX  ───▶ USB-Serial RX   (디버그 로그, 115200)
 PA3   USART2_RX  ◀─── USB-Serial TX
 PA5   STATUS_LED ───▶ LED ─ 330Ω ─ GND
```

| 주의 | 내용 |
|------|------|
| **AT 포트 핀** | ESP32-C3 AT 펌웨어의 **명령 포트는 UART1 = GPIO6(RX)/GPIO7(TX)** 입니다. GPIO20/21 은 UART0(부팅 로그·펌웨어 다운로드) 이라 여기에 물리면 `ready` 만 보이고 AT 에 응답하지 않습니다. |
| **CTS 를 GND 로** | ESP-AT 는 CTS(GPIO5) 가 H 이면 송신을 멈춥니다. 흐름제어를 쓰지 않으므로 반드시 GND 에 묶어 주세요. (또는 한 번 `AT+UART_DEF=115200,8,1,0,0` 을 보내 흐름제어를 영구 해제) |
| **전원** | Wi-Fi 송신 순간 300~400mA 피크. LDO 용량이 작으면 AP 접속 중 모듈이 리부팅(`ready` 재수신) 됩니다. |
| **로직 레벨** | 둘 다 3.3V 라 레벨 변환 불필요. |
| **EN 핀** | 모듈에 EN 풀업(10k)+RC 가 있으면 PB0 은 오픈드레인처럼 L 로만 당겨도 됩니다. 이 코드는 푸시풀로 H/L 을 직접 구동합니다. |

## 2. CubeMX 설정 (`STM32L562_ESP32C3_WiFi.ioc`)

| 항목 | 값 |
|------|----|
| MCU | STM32L562CETx, UFQFPN48 |
| SYSCLK | MSI 48 MHz (크리스탈 불필요), Flash latency 2 |
| USART1 | PA9/PA10, Asynchronous, 115200-8-N-1, **FIFO mode Enable**, 클럭 = HSI16 |
| USART2 | PA2/PA3, Asynchronous, 115200-8-N-1, 클럭 = HSI16 |
| GPIO | PA5 `STATUS_LED` 출력, PB0 `ESP_EN` 출력 (초기 High) |
| ICACHE | 1-way |
| NVIC | SysTick 만 (UART 인터럽트 없음) |

USART 클럭을 SYSCLK(MSI) 대신 HSI16 으로 두는 이유: 16 MHz / 115200 = 138.9 → 보레이트 오차 −0.08%.
USART1 의 8바이트 RX FIFO 를 켜 두면 메인 루프가 약 0.7 ms 늦어져도 오버런이 나지 않습니다.
오버런 발생 횟수는 `ESP_OverrunCount()` 로 확인할 수 있습니다 (정상이면 0).

## 3. 파일 구성

```
Core/Inc/wifi_config.h   ← ★ SSID / 비밀번호 / 서버 IP·PORT / DHCP ON·OFF / 고정 IP / 재시도 정책
Core/Inc/main.h          ← 핀, 보레이트, 로그 on/off, 버퍼 크기
Core/Src/esp32_at.c      ← AT 드라이버: UART 폴링, 줄 파서, URC 이벤트, +IPD 수신, CIPSEND
Core/Src/wifi_mgr.c      ← 상태 머신: 리셋→초기화→MAC→AP→서버→ONLINE, 재접속 정책
Core/Src/debug_log.c     ← USART2 printf 로그
Core/Src/main.c          ← 클럭/GPIO 초기화, 메인 루프, 데모 송수신, LED
Core/Src/stm32l5xx_hal_msp.c / stm32l5xx_it.c
```

## 4. 설정 — `wifi_config.h`

```c
#define WIFI_SSID                 "MyAccessPoint"
#define WIFI_PASSWORD             "MyPassword123"

#define SERVER_IP                 "192.168.0.10"     /* 도메인 이름도 가능 */
#define SERVER_PORT               5000U
#define SERVER_TCP_KEEPALIVE_S    10U                /* 0 = keep-alive 안 씀 */

#define WIFI_USE_DHCP             1U                 /* 1 = DHCP ON, 0 = 고정 IP */
#define WIFI_STATIC_IP            "192.168.0.50"     /* DHCP OFF 일 때만 사용 */
#define WIFI_STATIC_GATEWAY       "192.168.0.1"
#define WIFI_STATIC_NETMASK       "255.255.255.0"

#define WIFI_AP_RETRY_MS          5000U   /* AP 접속 실패 → 재시도 간격 */
#define WIFI_AP_FAIL_LIMIT        5U      /* 연속 실패 n회 → 모듈 리셋 */
#define WIFI_SERVER_RETRY_MS      3000U   /* 서버 접속 실패 → 재시도 간격 */
#define WIFI_SERVER_FAIL_LIMIT    5U      /* 연속 실패 n회마다 AP 상태 재확인 */
#define WIFI_SERVER_FAIL_RESET    15U     /* 연속 실패 n회 → 모듈 리셋 */
#define WIFI_STATUS_CHECK_MS      10000U  /* ONLINE 중 링크 상태 확인 주기 */
#define APP_TX_PERIOD_MS          5000U   /* 데모: 서버로 주기 송신 (0 = 안 함) */
```

실행 중에 바꾸려면 `WIFI_Config_t` 를 채워 `WIFI_Reconfigure(&cfg)` 를 부르면 됩니다.

## 5. 동작 흐름

```
전원 ON
  └─ WIFI_Init()  : USART1 초기화, 설정 로드
       │
       ▼
  [RESET]   PB0 L(50ms) → H  →  "ready" 대기 (5s)
       │
       ▼
  [INIT]    AT (동기화 5회)  → ATE0 → AT+GMR → AT+SYSSTORE=0 → AT+CWMODE=1
            → AT+CWAUTOCONN=0 → AT+CWRECONNCFG=0,0        (모듈 자체 재접속 OFF)
            → AT+CIPMUX=0 → AT+CIPMODE=0 → AT+CIPRECVMODE=0 → AT+CIPDINFO=0
            → AT+CIPSTAMAC?   ───────────────────────────▶  MAC 주소 (로그 + WIFI_GetMac())
       │
       ▼
  [AP_CONNECT]
            DHCP ON  : AT+CWDHCP=1,1
            DHCP OFF : AT+CWDHCP=0,1 → AT+CIPSTA="ip","gateway","netmask"
            → AT+CWJAP="ssid","pw"  (최대 20s)
            → AT+CIPSTA?           ───────────────────────▶  할당된 IP (WIFI_GetIp())
       │  실패: +CWJAP:<1 timeout|2 wrong password|3 AP not found|4 fail>
       │        → 5s 후 재시도, 연속 5회 → [RESET]
       ▼
  [SERVER_CONNECT]
            AT+CIPSTART="TCP","ip",port,keepalive
       │  실패: 3s 후 재시도, 5회마다 AT+CIPSTATUS 로 AP 확인, 15회 → [RESET]
       ▼
  [ONLINE]  ─ 5초마다 "STM32L562 #n mac=.. ip=.." 송신 (AT+CIPSEND)
            ─ 서버 데이터 수신 (+IPD) → USART2 로 출력
            ─ 10초마다 AT+CIPSTATUS 로 링크 확인
            │
            ├─ "WIFI DISCONNECT" 또는 STATUS:5   → [AP_CONNECT]      (5s 후)
            ├─ "CLOSED" / SEND FAIL / STATUS:4   → [SERVER_CONNECT]  (3s 후)
            ├─ AT 응답 없음 (timeout)             → [RESET]
            └─ "ready" (모듈이 스스로 재부팅)      → [INIT]
```

- 재시도 대기(`WAIT`) 는 `HAL_GetTick()` 기반 논블로킹이라 대기 중에도 메인 루프·LED 가 계속 돕니다.
- 대기 중 AP 가 끊기면 목적지를 서버 재접속 → AP 재접속으로 자동 변경합니다.
- 모듈의 자동 재접속(`CWAUTOCONN`, `CWRECONNCFG`) 은 꺼서 STM32 가 상태를 전적으로 관리합니다.
  구형 펌웨어에서 이 명령이 `ERROR` 를 내도 무시하고 진행합니다.
- `AT+SYSSTORE=0` 으로 설정을 모듈 플래시에 저장하지 않게 해 매 부팅마다 쓰는 `CWJAP`/`CIPSTA` 로
  플래시가 마모되는 것을 막습니다.

### LED 패턴

| 상태 | LED |
|------|-----|
| RESET / INIT | 100 ms 빠른 점멸 |
| AP / 서버 접속 중 | 250 ms 점멸 |
| 재시도 대기 | 1 s 점멸 |
| ONLINE | 켜짐, 2초마다 짧게 깜빡 |

## 6. 폴링 방식 AT 드라이버 (`esp32_at.c`)

```
ESP_Poll()   메인 루프 및 모든 대기 루프에서 호출
  UART RDR ─┬─ +IPD 데이터 수신 중 → 데이터 링버퍼(1KB)          → ESP_DataRead()
            └─ 한 줄 버퍼에 누적
                 ├─ "\r\n"      → ProcessLine()
                 │     ├─ OK / ERROR / FAIL / SEND OK / SEND FAIL → 명령 결과
                 │     ├─ busy p...                                → 200ms 후 재전송
                 │     ├─ ready / WIFI CONNECTED / WIFI GOT IP /
                 │     │  WIFI DISCONNECT / CONNECT / CLOSED       → 이벤트 비트
                 │     └─ 그 외 (+CIPSTAMAC:, STATUS: ...)        → 응답 버퍼
                 ├─ ">" 한 글자 → CIPSEND 프롬프트
                 └─ "+IPD,<len>:" → len 바이트 데이터 모드 진입
```

| 함수 | 설명 |
|------|------|
| `ESP_Cmd(cmd, timeout)` | `cmd\r\n` 전송 후 OK/ERROR 까지 폴링 대기. `ESP_OK / ESP_ERROR / ESP_TIMEOUT` |
| `ESP_CmdFmt(timeout, fmt, ...)` | printf 형식 |
| `ESP_GetQuoted("+CIPSTAMAC:", buf, n)` | 응답에서 `"..."` 값 추출 |
| `ESP_GetInt("STATUS:", &v)` | 응답에서 정수 추출 |
| `ESP_WaitFor("ready", timeout)` | 특정 줄 대기 |
| `ESP_TakeEvents()` | URC 이벤트 비트 가져오기 (+클리어) |
| `ESP_SendData(buf, len)` | `AT+CIPSEND=len` → `>` → 데이터 → `SEND OK` |
| `ESP_DataRead(buf, max)` | 서버에서 받은 바이트 읽기 |

## 7. 애플리케이션에서 쓰는 API (`wifi_mgr.h`)

```c
WIFI_Init(NULL);                 /* wifi_config.h 기본값으로 시작 */
while (1) {
  WIFI_Process();                /* 반드시 계속 호출 */
  if (WIFI_IsOnline()) {
    WIFI_Send((uint8_t *)"hi\r\n", 4);
  }
  n = WIFI_Receive(buf, sizeof buf);
}
WIFI_GetMac();  WIFI_GetIp();  WIFI_GetState();  WIFI_GetStateName();  WIFI_GetFwVersion();
```

## 8. 테스트 방법

1. PC 에서 TCP 서버를 띄웁니다 (PC 의 IP 를 `SERVER_IP` 에 적었는지 확인).
   ```
   # Linux/macOS
   nc -l 5000
   ```
   또는 아무 OS 에서나 (python 3):
   ```python
   # tcp_server.py  —  python tcp_server.py
   import socket
   s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
   s.bind(("0.0.0.0", 5000)); s.listen(1)
   while True:
       c, a = s.accept(); print("connected", a)
       while True:
           d = c.recv(256)
           if not d: break
           print(d); c.send(b"ack\r\n")
       print("closed"); c.close()
   ```
2. USART2 를 115200 으로 터미널에 연결하면 다음과 같은 로그가 보입니다.
   ```
   [      12] === STM32L562CET6 + ESP32-C3 AT Wi-Fi client ===
   [      13] WIFI: ssid='MyAccessPoint' server=192.168.0.10:5000 dhcp=on
   [      13] WIFI: module reset #1
   [      13] ESP32: hardware reset (EN low 50ms)
     <- ready
   [     412] ESP32: ready
   [     412] WIFI: RESET -> INIT
     -> AT
     <- OK
     -> ATE0
     <- OK
     -> AT+GMR
     <- AT version:2.4.0.0(...)
     ...
     -> AT+CIPSTAMAC?
     <- +CIPSTAMAC:"7c:df:a1:12:34:56"
     <- OK
   [     720] WIFI: MAC address = 7c:df:a1:12:34:56
   [     720] WIFI: INIT -> AP_CONNECT
   [     721] WIFI: DHCP on
     -> AT+CWJAP="MyAccessPoint","MyPassword123"
     <- WIFI CONNECTED
     <- WIFI GOT IP
     <- OK
     -> AT+CIPSTA?
     <- +CIPSTA:ip:"192.168.0.37"
   [    3910] WIFI: AP connected, ip=192.168.0.37
   [    3910] WIFI: AP_CONNECT -> SERVER_CONNECT
     -> AT+CIPSTART="TCP","192.168.0.10",5000,10
     <- CONNECT
     <- OK
   [    4120] WIFI: server connected
   [    4120] WIFI: SERVER_CONNECT -> ONLINE
     -> AT+CIPSEND=48
     <- OK
     -> <48 bytes>
     <- SEND OK
   [    9135] APP: tx #1 ok
   ```
3. 서버를 종료하면 `CLOSED` → 3초 후 재접속, AP 를 끄면 `WIFI DISCONNECT` → 5초 간격으로 AP 재접속을
   시도하는 것을 볼 수 있습니다. 로그가 너무 많으면 `main.h` 의 `ESP_AT_DEBUG` 를 0 으로.

## 9. 문제 해결

| 증상 | 확인할 것 |
|------|-----------|
| `no 'ready'` + `module not responding to AT` | TX/RX 교차 배선, GPIO6/7 (UART0 인 GPIO20/21 아님), 3.3V 전원 용량, 모듈에 ESP-AT 펌웨어가 들어 있는지 |
| `ready` 는 보이는데 AT 에 응답 없음 | CTS(GPIO5) 를 GND 로. 모듈이 흐름제어 대기 상태 |
| 응답이 깨져 보임 | 보레이트(ESP-AT 기본 115200). 모듈 설정이 바뀌었으면 `AT+UART_DEF=115200,8,1,0,0` |
| `AP connect failed: AP not found (3)` | SSID 오타, 2.4 GHz 인지 (ESP32-C3 는 5 GHz 불가) |
| `AP connect failed: wrong password (2)` | 비밀번호, WPA3 전용 AP 인지 (ESP-AT 최신 버전 필요) |
| `IP config failed` (DHCP OFF) | IP/게이트웨이/넷마스크 문자열 형식, 게이트웨이가 같은 서브넷인지 |
| 서버 접속 반복 실패 | PC 방화벽에서 포트 허용, 서버가 `0.0.0.0` 에 바인드됐는지, 같은 서브넷인지 |
| `ESP_OverrunCount()` 가 늘어남 | 메인 루프에 긴 블로킹 코드가 있음 → 그 안에서 `ESP_Poll()` 을 호출하거나 나눠서 실행 |
| 접속 중 갑자기 `module rebooted` | 전원 부족(브라운아웃). 모듈 3V3 에 100µF 이상 벌크 커패시터 |

## 10. CubeMX 로 재생성할 때

- `.ioc` 를 열어 Generate Code 하면 `main.c` 의 `MX_USART1_UART_Init()` / `MX_USART2_UART_Init()` 본문이
  채워집니다. `esp32_at.c` 의 `ESP_Init()`, `debug_log.c` 의 `DBG_Init()` 이 같은 설정으로 다시 초기화하므로
  그대로 두어도 되고, CubeMX 생성 본문을 지우고 이 저장소 버전을 유지해도 됩니다.
- USER CODE 구간 안의 코드는 보존됩니다.
- Project Manager → Code Generator 에서 "Generate peripheral initialization as a pair of .c/.h files" 는
  끄는 것이 이 구조와 맞습니다.
