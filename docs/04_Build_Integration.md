# 04. STM32CubeIDE 프로젝트 통합 절차

---

## 1. 폴더 구조 (CubeMX 코드 생성 후)

```
SurgeDetector/
├── Core/
│   ├── Inc/            (CubeMX 생성 : main.h, stm32l5xx_it.h, FreeRTOSConfig.h ...)
│   └── Src/            (CubeMX 생성 : main.c, freertos.c, stm32l5xx_it.c ...)
├── Drivers/            (CubeMX 생성 : HAL, CMSIS)
├── Middlewares/        (CubeMX 생성 : FreeRTOS, USB Device Library)
├── USB_DEVICE/         (CubeMX 생성 : usbd_cdc_if.c ...)
├── App/                ★ 본 저장소에서 복사
│   ├── Inc/
│   │   ├── app_cfg.h        컴파일 타임 설정 (핀/버퍼/타임아웃/기본값)
│   │   ├── app_types.h      공용 타입 + 전역 커널 오브젝트 선언
│   │   ├── app_main.h       진입점
│   │   ├── uart_link.h      UART DMA 링버퍼 추상화
│   │   ├── w25q40.h         외부 플래시 드라이버
│   │   ├── cfg_store.h      설정 저장/적재
│   │   ├── fpga_link.h      FPGA 트리거/프레임
│   │   ├── esp_at.h         ESP32 AT 계층
│   │   ├── wifi_task.h      WiFi 상태머신
│   │   ├── data_router.h    출력 분배
│   │   ├── usb_bridge.h     USB CDC 브리지
│   │   ├── cli.h            PC 설정 프로토콜 파서
│   │   └── util_crc.h       CRC8 / CRC32
│   └── Src/                 (같은 이름의 .c)
├── Integration/        ★ CubeMX 생성 파일에 붙여넣을 스니펫 (빌드 제외)
└── docs/               ★ 설계 문서
```

---

## 2. STM32CubeIDE 에 App 폴더 추가

1. `App` 폴더를 프로젝트 루트에 복사 (탐색기에서 붙여넣기 후 IDE에서 F5 새로고침)
2. 프로젝트 우클릭 → **Properties → C/C++ General → Paths and Symbols**
   - **Includes** 탭 → *GNU C* 선택 → **Add...** → `App/Inc`
     (Is a workspace path 체크, 또는 상대경로 `../App/Inc`)
   - **Source Location** 탭 → **Add Folder...** → `App`
3. **Apply and Close** → 프로젝트 정리(Clean) 후 빌드

> `App/Src` 만 소스로 잡히면 됩니다. `Integration/`, `docs/` 는 소스 경로에
> 넣지 마십시오 (마크다운/스니펫 문서라 빌드 대상이 아닙니다).

---

## 3. CubeMX 생성 파일 수정 (USER CODE 구간)

[Integration/README.md](../Integration/README.md) 의 3개 문서를 순서대로 반영합니다.

| 파일 | 넣을 것 |
|---|---|
| `Core/Src/main.c` | `#include "app_main.h"`, `HAL_PWREx_EnableVddUSB()`, 초기 핀 상태, `App_PreKernelInit()` |
| `Core/Src/freertos.c` | `App_Main()` 호출, FreeRTOS 훅 2개 |
| `USB_DEVICE/App/usbd_cdc_if.c` | `usbbridge_rx_from_isr(Buf, *Len)` |

> 모두 `USER CODE BEGIN/END` **안쪽**에 넣어야 CubeMX 재생성 시 보존됩니다.

---

## 4. 컴파일러 설정

| 항목 | 값 | 위치 |
|---|---|---|
| Optimization | `-Og` (디버그) / `-O2` (릴리즈) | C/C++ Build → Settings → MCU GCC Compiler → Optimization |
| Warnings | `-Wall -Wextra` 권장 | 〃 Warnings |
| C dialect | `gnu11` | 〃 General |
| Float ABI / FPU | Hard / FPv5-SP-D16 (CubeMX 기본) | MCU Settings |
| **`Use float with printf`** | **체크 불필요** | 본 코드는 정수 포맷만 사용 |

---

## 5. 빌드 검증

본 저장소의 `App/Src/*.c` 는 호스트(gcc)에서 다음이 확인되어 있습니다.

- `-Wall -Wextra` 무경고 컴파일
- 심볼 중복 없음
- CRC-8(poly 0x07) : `"123456789"` → `0xF4` ✔
- CRC-32(reflected 0xEDB88320) : `"123456789"` → `0xCBF43926` ✔
- FPGA 프레임 파서 : 정상 프레임 디코드 / CRC 오류 검출 / 잡음 후 재동기 / 절단 프레임 타임아웃 ✔
- CLI 파서 : `SET`/`GET`/`GET ALL`/에러응답/스트림 피드 ✔
- IP 문자열 검증 : 정상 3종 + 비정상 7종 ✔

> STM32 타깃 빌드는 CubeMX 로 생성한 HAL/FreeRTOS/USB 미들웨어가 있어야 하므로
> 위 검증은 HAL 을 스텁으로 대체한 상태에서 수행한 것입니다.

---

## 6. 동작 확인 순서

1. **USB CDC 연결** — PC에서 가상 COM 포트 인식 확인 → 터미널(115200, 아무 값이나) 접속
2. `VER` → `VER=SurgeDetector 1.0.0` 응답 확인
3. `FLASHID` → `ID=EF4013` 류 응답 확인 (SPI2/W25Q40 배선 검증)
4. `GET ALL` → 기본 설정 확인
5. `SET SSID ...` / `SET PASS ...` / `SET SRVIP ...` / `SET SRVPORT 50001` → `SAVE`
6. `STATUS` → `WIFI=UP`, `TCP=UP` 이 될 때까지 관찰 (LED_WIFI 점등)
7. FPGA 전원 인가 → 트리거 입력 확인 → `STATUS` 의 `TRIG` / `RX_FRAME` 증가 확인
8. RS485 / USB / 서버 세 경로로 `SD,...` 라인 수신 확인
9. AP를 껐다 켜서 자동 재연결 확인 (`WIFI_RECONN` 증가, 다시 `TCP=UP`)

---

## 7. 트러블슈팅

| 증상 | 확인할 것 |
|---|---|
| 부팅 직후 LED_ERR 점등 | `App_Main()` 의 초기화 실패 — `uartlink_init_all()` 또는 `cfgstore_init()` 실패. `FLASHID` 로 SPI 확인 |
| `FLASHID` 가 `ERR,SPI` | SPI2 배선/CS 핀, SPI 모드 0, 프리스케일러 확인 |
| `FLASHID` 가 `FF FF FF` / `00 00 00` | MISO 미연결 또는 CS 상시 High/Low |
| CDC 포트가 안 잡힘 | `HAL_PWREx_EnableVddUSB()` 누락, HSI48+CRS 미설정, PA11/PA12 배선 |
| HardFault (osKernelStart 직후) | SYS Timebase 가 SysTick 인 경우. TIM6 로 변경 |
| `configASSERT` 걸림 | FreeRTOS API를 호출하는 인터럽트 우선순위가 5보다 높음(숫자 작음) |
| `RX_TIMEOUT` 만 증가 | 트리거는 오는데 USART2 프레임이 없음 — 보레이트/배선/프레임 포맷 확인 |
| `RX_CRCERR` 만 증가 | 프레임은 오는데 CRC 불일치 — FPGA CRC 규격 확인, 급하면 `SD_FPGA_CHECK_CRC=0` |
| `DROP_WIFI` 증가 | 서버 전송이 취득 속도를 못 따라감 — 보레이트 상향 또는 `qWifiTx` 확대 |
| WiFi 가 `BACKOFF` 에서 못 나옴 | SSID/PASS 오타, AP 2.4GHz 여부, `AT` 명령으로 직접 진단(`AT AT+CWLAP`) |
| 스택 오버플로 훅 진입 | `STATUS` 의 `STACK_*` 값 확인 후 `app_cfg.h` 의 `SD_STK_*` 증가 |

---

## 8. 성능/여유 (1초 주기 기준)

| 항목 | 값 |
|---|---|
| 1샘플 출력 라인 | 약 44 bytes |
| USART3 115200 기준 전송 시간 | 약 3.8 ms |
| ESP32 `AT+CIPSEND` 1회 왕복 | 약 20~60 ms |
| CPU 부하 (110 MHz) | 1% 미만 |

샘플 주기를 100 ms 로 줄여도(초당 10프레임) 여유가 큽니다.
그 이상(≥100 Hz)으로 올리려면 ASCII 대신 바이너리 패킷 + 다중 샘플 묶음 전송으로
바꾸는 것을 권장합니다 (`data_router.c` 만 수정하면 됩니다).
