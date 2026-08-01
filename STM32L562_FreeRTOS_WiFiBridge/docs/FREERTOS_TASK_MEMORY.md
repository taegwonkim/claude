# FreeRTOS 태스크 우선순위 / 메모리 설계

STM32CubeMX에서 FreeRTOS(CMSIS_V2) 미들웨어를 켜고 이 프로젝트를 구성할 때 사용할
우선순위·스택·힙 설정값과 그 근거입니다. 대상 MCU는 STM32L562 (Cortex-M33,
SRAM 256KB급: SRAM1 192KB + SRAM2 64KB) 기준입니다. 사용 중인 정확한 부품번호의
SRAM 용량은 데이터시트로 재확인하세요.

## 1. 태스크 우선순위

CMSIS-RTOS2(FreeRTOS 어댑터)는 `osPriority_t` 값을 그대로 FreeRTOS 우선순위로
매핑합니다. 이 프로젝트는 4개의 애플리케이션 태스크를 사용합니다.

| 태스크        | 우선순위 (`osPriority_t`)   | 근거                                                                 |
|----------------|------------------------------|-----------------------------------------------------------------------|
| `fpgaIfTask`   | `osPriorityAboveNormal1`     | Cyclone IV 트리거(EXTI)에 즉시 반응해 USART3 DMA 수신을 준비해야 함. 네 태스크 중 가장 실시간성이 높음. 지연되면 다음 트리거 전까지 데이터 유실 가능 |
| `esp32Task`    | `osPriorityAboveNormal`      | AT 명령 송수신은 블로킹으로 수백ms~수초 걸릴 수 있어 fpgaIfTask보다는 낮지만, PC 설정 처리보다는 응답성이 중요 (측정 데이터 전송 경로) |
| `pcUartTask`   | `osPriorityNormal`           | USART1로 들어오는 설정 명령 처리. 지연에 가장 관대함 |
| `usbCdcTask`   | `osPriorityNormal`           | USB CDC로 들어오는 설정 명령 처리 - `pcUartTask`와 동급(둘 다 사람이 입력/스크립트로 보내는 CFG: 명령을 다룰 뿐이라 우선순위를 나눌 이유가 없음) |
| (Idle Task)    | `osPriorityIdle` (고정)      | FreeRTOS 커널이 자동 생성                                             |
| (Timer Service)| `configTIMER_TASK_PRIORITY`  | CubeMX 기본값(`configMAX_PRIORITIES - 2`) 유지. 이 앱은 software timer를 직접 쓰지 않음 |

**규칙**: `fpgaIfTask > esp32Task > {pcUartTask, usbCdcTask}`. `configUSE_PREEMPTION`은
반드시 `1`이어야 트리거 발생 시 낮은 우선순위 태스크를 즉시 선점할 수 있습니다.

> `esp32Task`가 AT 명령 대기 중(`osDelay`/세마포어 대기)일 때는 CPU를 반납하므로,
> 실제로는 `fpgaIfTask`가 항상 즉시 실행됩니다 — 우선순위 역전 걱정 없이 안전합니다.

## 2. 스택 크기 (`app_config.h`의 `TASK_STACK_*`, 단위: word = 4byte)

| 태스크        | 스택(word) | 스택(byte) | 근거 |
|----------------|-----------:|-----------:|------|
| `pcUartTask`   | 384        | 1536 B     | 링버퍼 pull + 줄 조립만 담당(파싱은 `CfgProtocol_HandleLine()`으로 이동) |
| `usbCdcTask`   | 384        | 1536 B     | `pcUartTask`와 동일 구조(USB CDC 링버퍼 pull + 줄 조립) |
| `esp32Task`    | 512        | 2048 B     | `snprintf`로 AT 명령 조립(최대 192B 로컬 버퍼) + 함수 호출 깊이가 가장 큼 |
| `fpgaIfTask`   | 384        | 1536 B     | 로직이 단순(세마포어 대기 → DMA 시작/대기 → 전달 호출)하지만 프레임 버퍼(1KB)는 스택이 아닌 정적 배열이라 스택 자체는 작게 유지 |

`CfgProtocol_HandleLine()`(CFG: 파싱 본체, `snprintf` 응답 버퍼 64B 등)은
`app_cfg_protocol.c`로 옮겨졌지만, 이 함수는 `pcUartTask`/`usbCdcTask` 중
호출한 쪽의 스택을 그대로 씁니다(별도 태스크가 아님) — 그래서 두 태스크 모두
기존 `pcUartTask`와 같은 384word로 유지했습니다.

개발 중에는 `configCHECK_FOR_STACK_OVERFLOW = 2`를 켜서 실측 후, 필요 시 여유를
두고 조정하세요 (FreeRTOS `uxTaskGetStackHighWaterMark()`로 실사용량 확인 가능).

## 3. `configTOTAL_HEAP_SIZE` 산정

CMSIS-RTOS2 API(`osThreadNew`, `osMessageQueueNew`, `osMutexNew`,
`osSemaphoreNew`, `osEventFlagsNew`)는 전부 FreeRTOS 힙(`heap_4.c`, CubeMX 기본값)에서
동적 할당됩니다. 이 프로젝트가 실제로 필요로 하는 양을 합산하면:

| 항목                                   | 대략 크기 |
|----------------------------------------|----------:|
| 태스크 스택 4개 합                     | 5,632 B   |
| Idle 태스크 스택 (`configMINIMAL_STACK_SIZE`=128w) | 512 B |
| Timer 서비스 태스크 스택               | 512 B     |
| TCB(태스크 제어 블록) 6개              | ~900 B    |
| 큐 1개(`esp32ReqQueue`, capacity 4)    | ~150 B    |
| 뮤텍스/세마포어/이벤트그룹 (6~7개)     | ~600 B    |
| heap_4 단편화/정렬 오버헤드            | ~500 B    |
| **합계**                               | **~8,800 B** |

여기에 여유(향후 태스크 추가, JSON/TLS 라이브러리 등)를 위해 약 2~3배 마진을 두고

```
#define configTOTAL_HEAP_SIZE   ((size_t)24 * 1024)   /* 24 KB */
```

를 권장합니다. STM32L562 SRAM(약 256KB) 대비 약 9%에 불과해 여유가 충분합니다.

## 4. 정적(비힙) 메모리 사용량 참고

FreeRTOS 힙과 별도로 `.data`/`.bss`에 정적으로 배치되는 애플리케이션 버퍼:

| 버퍼                              | 크기     |
|------------------------------------|---------:|
| `pcUart` RX 링버퍼                 | 256 B    |
| `usbCdc` RX 링버퍼                 | 256 B    |
| `esp32` RX 링버퍼                  | 512 B    |
| `esp32` AT 응답 스캔 버퍼          | 257 B    |
| Cyclone IV 프레임 버퍼             | 1024 B   |
| `AppConfig_t` (EEPROM 캐시용 등)   | ~190 B   |

합계 약 2.5KB — 무시해도 될 수준입니다. 측정 데이터량이 늘어나면
`app_config.h`의 `FPGA_FRAME_MAX_LEN`만 조정하면 됩니다 (SPI 플래시 페이지가
아니라 UART DMA 버퍼이므로 256B 제약 없음).

## 5. CubeMX FreeRTOS 설정 패널 체크리스트

CubeMX의 `Middleware and Software Packs → FREERTOS`에서:

- **Interface**: `CMSIS_V2` (필수 — 앱 코드가 `osThreadNew` 등 v2 API 사용)
- **Config parameters** 탭
  - `TOTAL_HEAP_SIZE` = `24576` (24 * 1024)
  - `TICK_RATE_HZ` = `1000`
  - `MAX_PRIORITIES` = `56` (**CMSIS-RTOS2 요구사항이므로 절대 변경 금지** — 변경 시 빌드 타임 assert 실패)
  - `MINIMAL_STACK_SIZE` = `128`
  - `USE_MUTEXES` = `1` (EEPROM 접근 뮤텍스에 필요)
  - `USE_COUNTING_SEMAPHORES` = `1`
- **Advanced settings** 탭
  - `USE_MALLOC_FAILED_HOOK` = `1` (힙 부족을 즉시 감지)
  - `CHECK_FOR_STACK_OVERFLOW` = `Stack overflow detection 2` (개발 중 권장, 양산 시 유지해도 무방)
  - `USE_TIMERS` = `1` (기본값 유지; 이 앱은 직접 사용하지 않지만 유지해도 비용 미미)
- **Tasks and Queues** 탭
  - CubeMX가 기본 생성하는 `defaultTask`는 **삭제**하세요. 이 프로젝트는
    `App_CreateTasks()`(`app_tasks.c`)가 `osThreadNew()`로 4개 태스크
    (`pcUartTask`/`usbCdcTask`/`esp32Task`/`fpgaIfTask`)를 직접 생성하므로
    `MX_FREERTOS_Init()`에는 커스텀 태스크를 추가하지 않습니다.

동일한 값을 `Core/Inc/FreeRTOSConfig.h`에도 정리해 두었습니다
(`FreeRTOSConfig.h`는 CubeMX가 위 GUI 값을 바탕으로 재생성하는 파일이므로,
CubeMX에서 "Generate Code"를 실행하면 이 값들로 덮어써집니다 — 즉 GUI 값을
맞추는 것이 원본입니다).
