# 03. 프로토콜 정의

본 프로젝트에서 정의한 3개의 인터페이스 규격입니다.
요구사항에 명시되지 않은 세부 포맷은 아래와 같이 **본 구현에서 확정**했으며,
FPGA/서버 측과 다르면 `App/Inc/app_cfg.h` 와 `fpga_link.c` 만 수정하면 됩니다.

---

## 1. MCU ↔ FPGA (USART2, 115200 8N1)

### 1.1 시작 명령 (MCU → FPGA)
전원 인가 후 (또는 CLI `START` 명령 시) **한 번만** 전송합니다.

```
"START\r\n"     (7 bytes)
```

정지 명령(선택, FPGA가 지원할 경우):
```
"STOP\r\n"      (6 bytes)
```

### 1.2 트리거 (FPGA → MCU, 하드웨어 신호)
- 핀: `PA1` (EXTI1)
- 극성: **Falling edge**
- 트리거 후 프레임이 도착하기까지 허용 시간: **200 ms** (`SD_FPGA_FRAME_TIMEOUT_MS`)

### 1.3 데이터 프레임 (FPGA → MCU, USART2)
**고정 길이 18 바이트 바이너리**

| 오프셋 | 크기 | 필드 | 설명 |
|---|---|---|---|
| 0 | 1 | `SOF0` | `0xA5` |
| 1 | 1 | `SOF1` | `0x5A` |
| 2 | 2 | `SEQ` | uint16 **little-endian**, FPGA가 증가시키는 시퀀스 |
| 4 | 2 | `CH0` | uint16 LE, 채널 0 피크값 (ADC raw) |
| 6 | 2 | `CH1` | |
| 8 | 2 | `CH2` | |
| 10 | 2 | `CH3` | |
| 12 | 2 | `CH4` | |
| 14 | 2 | `CH5` | |
| 16 | 1 | `STATUS` | bit0: overrange, bit1: adc_err, bit2..7: reserved |
| 17 | 1 | `CRC8` | 오프셋 0~16 의 CRC-8 (poly 0x07, init 0x00) |

> 수신 로직은 링버퍼에서 `0xA5 0x5A` 를 찾아 동기를 맞춘 뒤 나머지 16바이트를
> 읽고 CRC8을 검증합니다. CRC 불일치 시 프레임을 버리고 재동기합니다.
> (`App/Src/fpga_link.c`)

> **CRC를 쓰지 않는 FPGA라면** `app_cfg.h` 의 `SD_FPGA_CHECK_CRC` 를 `0` 으로 두면
> CRC 검사를 건너뜁니다.

---

## 2. MCU → PC / 서버 (데이터 출력)

USART3(RS485), USB CDC, WiFi(TCP) **세 경로 모두 동일한 ASCII 라인**을 사용합니다.

```
SD,<seq>,<tick_ms>,<ch0>,<ch1>,<ch2>,<ch3>,<ch4>,<ch5>,<status>\r\n
```

예:
```
SD,1024,3600250,1023,987,1200,45,60,3300,0
```

| 필드 | 형식 | 설명 |
|---|---|---|
| `seq` | 10진 uint16 | FPGA 시퀀스 |
| `tick_ms` | 10진 uint32 | MCU 부팅 후 경과 ms (`osKernelGetTickCount`) |
| `ch0..ch5` | 10진 uint16 | 6채널 피크값 |
| `status` | 10진 uint8 | FPGA 상태 바이트 |

각 출력 경로는 설정(`OUT_UART`, `OUT_USB`, `OUT_WIFI`)으로 개별 on/off 됩니다.

---

## 3. PC ↔ MCU 설정 프로토콜 (USART3 RS485 / USB CDC 공통)

- **ASCII 라인 기반**, 종단 `\r\n` 또는 `\n`
- 대소문자 구분 없음
- 응답: 성공 `OK`, 실패 `ERR,<사유>`
- 라인 최대 길이: 128 바이트

### 3.1 WRITE 명령 — `SET <KEY> <VALUE>`

| KEY | 값 형식 | 설명 |
|---|---|---|
| `SSID` | 문자열 (최대 32) | AP SSID |
| `PASS` | 문자열 (최대 64) | AP 비밀번호 |
| `SRVIP` | `a.b.c.d` | 서버 IP |
| `SRVPORT` | 1~65535 | 서버 포트 (기본 **50001**) |
| `DHCP` | `0` / `1` | 1 = DHCP 사용, 0 = 고정 IP |
| `IP` | `a.b.c.d` | 모듈 고정 IP (DHCP=0 일 때) |
| `GW` | `a.b.c.d` | 게이트웨이 |
| `MASK` | `a.b.c.d` | 서브넷 마스크 |
| `SAMPLEMS` | 100~60000 | FPGA 샘플 주기(ms). 기록용/참고값 |
| `OUT_UART` | `0`/`1` | RS485로 데이터 출력 |
| `OUT_USB` | `0`/`1` | USB CDC로 데이터 출력 |
| `OUT_WIFI` | `0`/`1` | WiFi(TCP)로 데이터 출력 |
| `AUTOSTART` | `0`/`1` | 부팅 시 FPGA에 자동으로 START 전송 |

예:
```
> SET SSID MyHomeAP
< OK
> SET PASS 12345678
< OK
> SET SRVIP 192.168.0.10
< OK
> SET SRVPORT 50001
< OK
> SET DHCP 0
< OK
> SET IP 192.168.0.50
< OK
> SET GW 192.168.0.1
< OK
> SET MASK 255.255.255.0
< OK
> SAVE
< OK
```

> `SET` 은 RAM 상의 설정만 바꿉니다. **`SAVE` 를 해야 외부 플래시에 기록**됩니다.
> `SSID/PASS/SRVIP/SRVPORT/DHCP/IP/GW/MASK` 중 하나라도 바뀌면
> `SAVE` 시점에 WiFi 재연결(`SD_EVT_WIFI_RECONF`)이 트리거됩니다.

### 3.2 READ 명령 — `GET <KEY>` / `GET ALL`

```
> GET SSID
< SSID=MyHomeAP
< OK

> GET ALL
< SSID=MyHomeAP
< PASS=********
< SRVIP=192.168.0.10
< SRVPORT=50001
< DHCP=0
< IP=192.168.0.50
< GW=192.168.0.1
< MASK=255.255.255.0
< SAMPLEMS=1000
< OUT_UART=1
< OUT_USB=1
< OUT_WIFI=1
< AUTOSTART=1
< OK
```

> `GET PASS` 는 실제 비밀번호를 반환합니다(현장 점검용).
> `GET ALL` 에서는 마스킹(`********`)됩니다. 마스킹 없이 보려면 `GET PASS`.
> 보안상 `GET PASS` 를 막으려면 `app_cfg.h` 의 `SD_ALLOW_PASS_READ` 를 `0` 으로.

### 3.3 제어/진단 명령

| 명령 | 동작 | 응답 |
|---|---|---|
| `SAVE` | RAM 설정 → 외부 플래시 저장 (2개 슬롯 이중화) | `OK` / `ERR,FLASH` |
| `LOAD` | 플래시 → RAM 설정 재적재 | `OK` |
| `DEFAULT` | 공장 초기값으로 RAM 설정 리셋 (SAVE 필요) | `OK` |
| `STATUS` | 시스템 상태 덤프 (아래 예 참조) | 다중 라인 + `OK` |
| `START` | FPGA에 `START\r\n` 전송 | `OK` |
| `STOP` | FPGA에 `STOP\r\n` 전송 | `OK` |
| `WIFI` | WiFi 상태머신 강제 재시작 | `OK` |
| `AT <cmd>` | ESP32에 raw AT 명령 전달 (진단용) | 모듈 응답 그대로 |
| `FLASHID` | W25Q40 JEDEC ID 읽기 | `ID=EF4013` 형태 |
| `VER` | 펌웨어 버전 | `VER=SurgeDetector 1.0.0` |
| `REBOOT` | MCU 소프트 리셋 | `OK` 후 리셋 |

`STATUS` 출력 예:
```
< UPTIME=3600
< WIFI=UP
< TCP=UP
< FPGA=RUN
< RX_FRAME=3598
< RX_CRCERR=2
< TX_WIFI=3598
< DROP_SAMPLE=0
< DROP_WIFI=0
< STACK_FPGA=381
< STACK_ROUTER=470
< STACK_WIFI=812
< HEAP_FREE=11240
< OK
```

### 3.4 RS485 멀티드롭 주소 (선택)
여러 대를 RS485 버스에 물릴 경우, 명령 앞에 `@<addr>:` 접두어를 붙일 수 있습니다.
```
@01:GET ALL
```
`app_cfg.h` 의 `SD_RS485_ADDR` 와 일치할 때만 응답합니다.
`SD_RS485_ADDR = 0` (기본) 이면 접두어 검사를 하지 않고 모든 명령에 응답합니다.

---

## 4. MCU ↔ ESP32-C WROOM (USART1, AT 명령)

ESP-AT 펌웨어 기준 연결 시퀀스입니다. (`App/Src/wifi_task.c`)

| 단계 | 명령 | 기대 응답 | 타임아웃 |
|---|---|---|---|
| 0 | (HW) `ESP_EN` Low 30 ms → High | `ready` | 3000 ms |
| 1 | `AT` | `OK` | 1000 ms |
| 2 | `ATE0` | `OK` | 1000 ms |
| 3 | `AT+CWMODE=1` | `OK` | 1000 ms |
| 4a | `AT+CWDHCP=1,1` (DHCP=1) | `OK` | 2000 ms |
| 4b | `AT+CWDHCP=1,0` → `AT+CIPSTA="ip","gw","mask"` (DHCP=0) | `OK` | 2000 ms |
| 5 | `AT+CWJAP="<ssid>","<pass>"` | `OK` (`WIFI CONNECTED`, `WIFI GOT IP` URC 동반) | **20000 ms** |
| 6 | `AT+CIPMUX=0` | `OK` | 1000 ms |
| 7 | `AT+CIPSTART="TCP","<srvip>",<port>` | `CONNECT` + `OK` | **10000 ms** |
| 8 | (데이터) `AT+CIPSEND=<len>` | `>` | 2000 ms |
| 9 | (payload 그대로 전송) | `SEND OK` | 3000 ms |

### 4.1 URC 처리
| URC | 동작 |
|---|---|
| `WIFI DISCONNECT` | `SD_EVT_WIFI_UP` clear → 상태머신 JOIN 단계로 |
| `WIFI CONNECTED` / `WIFI GOT IP` | `SD_EVT_WIFI_UP` set |
| `CLOSED` | `SD_EVT_TCP_UP` clear → 상태머신 TCP 단계로 |
| `ERROR` / `busy p...` | 해당 명령 실패 처리 후 재시도 |
| `ready` | 모듈 리부팅 감지 → 상태머신 처음부터 |

### 4.2 재연결 백오프
연속 실패 시 대기 시간: **1s → 2s → 4s → 8s → 16s → 30s (최대)**
성공하면 백오프 초기화. 5회 연속 실패하면 ESP32를 **하드웨어 리셋**합니다.

### 4.3 대체 방식 (참고)
`AT+CIPMODE=1` (passthrough) + `AT+CIPSEND` 으로 투명 전송 모드를 쓰면 스루풋이
올라가지만, 연결 끊김 감지가 어렵고 `+++` 로 빠져나와야 하므로 본 구현은
**일반 모드(CIPMODE=0) + 길이 지정 CIPSEND** 를 사용합니다. 1초 주기 소량 데이터에는
이 방식이 안정적입니다.
