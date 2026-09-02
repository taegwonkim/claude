# SurgeDetector

STM32L562CET6 + FreeRTOS 기반 서지 피크 검출기 펌웨어.

Cyclone IV FPGA 가 외부 ADC 6채널의 피크값을 취득하면, MCU 가 이를 받아
**RS485(USART3) / USB CDC / WiFi(ESP32-C WROOM → TCP 서버)** 세 경로로 동시에 내보냅니다.
AP·서버 접속 정보는 PC에서 RS485 또는 USB 로 읽고 쓸 수 있으며 외부 SPI 플래시에 저장됩니다.

```
                         ┌──────────────────────────────────────────┐
   외부 ADC 6ch ──▶ FPGA │ trigger(PA1, falling) ─┐                 │
   (Cyclone IV)          │ USART2 115200 ─────────┼─▶  STM32L562CET6 │
                         └────────────────────────┘        │ FreeRTOS│
                                                           │         │
   PC ◀── RS485 (USART3) ──────────────────────────────────┤         │
   PC ◀── USB CDC (PA11/PA12) ─────────────────────────────┤         │
   Server ◀── TCP:50001 ◀── ESP32-C WROOM ◀── USART1 AT ───┤         │
                        W25Q40CLS ◀── SPI2 ─────────────────┘         │
                                                           └──────────┘
```

## 문서

| 문서 | 내용 |
|---|---|
| [docs/01_CubeMX_Config.md](docs/01_CubeMX_Config.md) | CubeMX 설정 전체 — 클럭, 핀맵, USART1/2/3, SPI2, USB, DMA, NVIC 우선순위 |
| [docs/02_FreeRTOS_Design.md](docs/02_FreeRTOS_Design.md) | **FreeRTOS 상세** — Config 파라미터, 태스크 7개, 큐 4개, 세마포어/뮤텍스, 힙 산정 |
| [docs/03_Protocol.md](docs/03_Protocol.md) | FPGA 프레임, 데이터 출력 포맷, PC 설정 명령(read/write), ESP-AT 시퀀스 |
| [docs/04_Build_Integration.md](docs/04_Build_Integration.md) | CubeIDE 통합 절차, 빌드 검증 결과, 트러블슈팅 |
| [Integration/](Integration/) | CubeMX 생성 파일(`main.c`, `freertos.c`, `usbd_cdc_if.c`)에 넣을 스니펫 |

## 코드 구성 (`App/`)

CubeMX 가 건드리지 않는 별도 폴더에 애플리케이션을 두고,
CubeMX 생성 코드에는 `USER CODE` 구간에서 두 줄만 호출합니다
(`App_PreKernelInit()`, `App_Main()`).
따라서 **CubeMX 재생성 시에도 애플리케이션 코드가 보존**됩니다.

| 파일 | 역할 |
|---|---|
| `app_main.c` | 커널 오브젝트/태스크 생성, 하트비트 |
| `uart_link.c` | UART DMA 순환 수신 + 링버퍼 + DMA 송신 (USART1/2/3 공용) |
| `fpga_link.c` | PA1 트리거 → USART2 18바이트 프레임 수신 → CRC8 검증 → 큐 게시 |
| `data_router.c` | 샘플 → `SD,seq,tick,ch0..ch5,st` 라인 → 3개 출력 경로 분배 |
| `esp_at.c` | ESP-AT 명령/응답/URC 처리 (뮤텍스로 세션 원자성 보장) |
| `wifi_task.c` | 연결 상태머신(RESET→INIT→NETCFG→JOIN→TCP→ONLINE) + 지수 백오프 재연결 |
| `cli.c` | PC 설정 프로토콜 파서 (RS485/USB 공용, `SET`/`GET`/`SAVE`/`STATUS` …) |
| `cfg_store.c` | 설정 구조체 + CRC32 + 외부 플래시 이중 슬롯 저장 |
| `w25q40.c` | W25Q40CLS SPI NOR 드라이버 |
| `usb_bridge.c` | USB CDC ↔ 큐 브리지 |
| `util_crc.c` | CRC-8 / CRC-32 |
| `hooks.c` | HAL 콜백(UART RX/TX/Error, EXTI) → 애플리케이션 라우팅 |

## 빠른 시작

1. CubeMX 로 [01 문서](docs/01_CubeMX_Config.md) 대로 프로젝트 생성 (TrustZone **Disabled**)
2. `App/` 폴더 복사 + include/source 경로 등록 ([04 문서 2장](docs/04_Build_Integration.md))
3. [Integration/](Integration/) 스니펫을 `USER CODE` 구간에 반영
4. 빌드 → 다운로드 → USB CDC 터미널에서 `VER`, `FLASHID`, `GET ALL` 확인
5. `SET SSID ...` → `SET PASS ...` → `SET SRVIP ...` → `SET SRVPORT 50001` → `SAVE`

## 주의

- 핀맵은 LQFP48 기준 제안값입니다. 실제 보드에 맞추어 CubeMX 에서 바꾸되,
  **User Label**(`ESP_EN`, `FPGA_TRIG`, `FLASH_CS`, `LED_RUN`, `LED_WIFI`, `LED_ERR`)만
  동일하게 지정하면 애플리케이션 코드는 수정할 필요가 없습니다.
- FPGA 프레임 포맷·PC 명령 규격은 요구사항에 명시되지 않은 부분을
  [03 문서](docs/03_Protocol.md)에서 확정한 것입니다. 상대측 규격이 다르면
  `fpga_link.c` / `app_cfg.h` 만 수정하면 됩니다.
- 이 저장소에는 `.ioc` 파일이 없습니다. 손으로 만든 `.ioc` 는 CubeMX 버전에 따라
  잘못 열리거나 엉뚱한 초기화 코드를 생성할 위험이 있어, 대신 CubeMX에서 그대로
  입력할 수 있는 **설정값 전체를 문서로** 제공합니다.
