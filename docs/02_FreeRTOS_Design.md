# 02. FreeRTOS 설계 상세 (CubeMX Middleware → FREERTOS)

> Interface: **CMSIS_V2** (osThreadNew / osMessageQueueNew / osSemaphoreNew / osMutexNew)

---

## 1. Config parameters 탭

| 파라미터 | 값 | 이유 |
|---|---|---|
| `USE_PREEMPTION` | **Enabled** | 우선순위 기반 선점 |
| `CPU_CLOCK_HZ` | `SystemCoreClock` (110000000) | 자동 |
| `TICK_RATE_HZ` | **1000** | 1 ms tick — 1초 샘플 주기/타임아웃 계산이 쉬움 |
| `MAX_PRIORITIES` | **56** | CMSIS-RTOS v2 기본 |
| `MINIMAL_STACK_SIZE` | **128** Words (512 B) | |
| `MAX_TASK_NAME_LEN` | **16** | |
| `IDLE_SHOULD_YIELD` | Enabled | |
| `USE_MUTEXES` | **Enabled** | SPI/설정/AT 보호에 필수 |
| `USE_RECURSIVE_MUTEXES` | Enabled | |
| `USE_COUNTING_SEMAPHORES` | **Enabled** | UART RX 신호용 |
| `QUEUE_REGISTRY_SIZE` | **12** | 디버거(RTOS viewer)에서 큐 확인 |
| `USE_APPLICATION_TASK_TAG` | Disabled | |
| `ENABLE_BACKWARD_COMPATIBILITY` | Disabled | |
| `USE_PORT_OPTIMISED_TASK_SELECTION` | **Enabled** | Cortex-M33 CLZ 사용, 스케줄러 가속 |
| `USE_TICKLESS_IDLE` | Disabled | (저전력이 필요하면 별도 검토) |
| `USE_TASK_NOTIFICATIONS` | **Enabled** | |
| **Memory Allocation** | | |
| `Memory Management scheme` | **heap_4** | 병합 가능한 동적 할당 |
| `TOTAL_HEAP_SIZE` | **40960** (40 KB) | 아래 6장 산정표 참조 |
| `SUPPORT_DYNAMIC_ALLOCATION` | Enabled | |
| `SUPPORT_STATIC_ALLOCATION` | **Enabled** | Idle/Timer 태스크 정적 할당(경고 제거) |
| **Hook function related definitions** | | |
| `USE_IDLE_HOOK` | Disabled | |
| `USE_TICK_HOOK` | Disabled | |
| `USE_MALLOC_FAILED_HOOK` | **Enabled** | 힙 고갈 즉시 검출 |
| `USE_DAEMON_TASK_STARTUP_HOOK` | Disabled | |
| `CHECK_FOR_STACK_OVERFLOW` | **Option2** | 스택 오버플로 검출 |
| **Run time and task stats** | | |
| `GENERATE_RUN_TIME_STATS` | Disabled (디버깅 시 Enable) | |
| `USE_TRACE_FACILITY` | **Enabled** | |
| `USE_STATS_FORMATTING_FUNCTIONS` | Enabled | |
| **Software timer definitions** | | |
| `USE_TIMERS` | **Enabled** | |
| `TIMER_TASK_PRIORITY` | **2** | |
| `TIMER_QUEUE_LENGTH` | 10 | |
| `TIMER_TASK_STACK_DEPTH` | **256** Words | |
| **Interrupt nesting behaviour** | | |
| `LIBRARY_LOWEST_INTERRUPT_PRIORITY` | **15** | |
| `LIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` | **5** | ISR에서 FreeRTOS API 사용 가능 하한 |

### Include parameters 탭
| 항목 | 값 |
|---|---|
| `vTaskPrioritySet` / `uxTaskPriorityGet` | Enabled |
| `vTaskDelete` | **Enabled** |
| `vTaskSuspend` | **Enabled** |
| `xResumeFromISR` | Enabled |
| `vTaskDelayUntil` | **Enabled** |
| `vTaskDelay` | **Enabled** |
| `xTaskGetSchedulerState` | **Enabled** |
| `uxTaskGetStackHighWaterMark` | **Enabled** (튜닝용) |
| `eTaskGetState` | Enabled |
| `xTaskAbortDelay` | Enabled |

---

## 2. Tasks (Tasks and Queues 탭 → Tasks)

CubeMX에서 **defaultTask 하나만** 남기고 나머지 태스크는 코드(`app_main.c`)에서
`osThreadNew()` 로 생성합니다. 이렇게 하면 CubeMX 재생성 시 태스크 정의가
날아가지 않고, 태스크 인자·생성 순서를 애플리케이션이 통제할 수 있습니다.

**CubeMX에 등록할 태스크 (1개)**

| Task Name | Priority | Stack (Words) | Entry Function | Code Gen. Option | Parameter |
|---|---|---|---|---|---|
| `defaultTask` | `osPriorityNormal` | 512 | `StartDefaultTask` | Default | NULL |

`StartDefaultTask()` 본문(USER CODE)에서 `App_Main()` 을 호출하면 나머지 태스크가
생성되고 defaultTask 자신은 하트비트 태스크로 계속 동작합니다.

**애플리케이션이 코드로 생성하는 태스크 (7개)**

| # | 태스크 | 우선순위 (CMSIS v2) | 스택 (Words / Bytes) | 역할 | 블로킹 지점 |
|---|---|---|---|---|---|
| 1 | `tskFpga` | `osPriorityRealtime` (48) | 512 / 2048 | 트리거 세마포어 대기 → USART2 프레임 수신 → CRC 검증 → `qSample` push | `osSemaphoreAcquire(semTrig)` |
| 2 | `tskRouter` | `osPriorityAboveNormal` (32) | 640 / 2560 | `qSample` pop → ASCII 라인 포맷 → USART3/USB/WiFi 큐 분배 | `osMessageQueueGet(qSample)` |
| 3 | `tskWifi` | `osPriorityNormal` (24) | 1024 / 4096 | ESP32 AT 상태머신(리셋→AT→CWJAP→CIPSTART) + `qWifiTx` 소비 + 재연결 | UART RX 세마포어 / 큐 |
| 4 | `tskCliUart` | `osPriorityBelowNormal` (16) | 768 / 3072 | USART3(RS485) 라인 파싱 → 설정 read/write | `uartlink_readline()` |
| 5 | `tskCliUsb` | `osPriorityBelowNormal` (16) | 768 / 3072 | USB CDC 라인 파싱 (동일 파서) | `osMessageQueueGet(qUsbRx)` |
| 6 | `tskUsbTx` | `osPriorityLow` (8) | 384 / 1536 | `qUsbTx` 소비 → `CDC_Transmit_FS` (busy 재시도) | `osMessageQueueGet(qUsbTx)` |
| 7 | `defaultTask` | `osPriorityNormal` (24) | 512 / 2048 | 하트비트 LED, IWDG 리프레시, 통계 | `osDelay(500)` |

> **우선순위 근거**
> - FPGA 트리거는 1초 주기이지만 트리거 후 프레임이 곧바로 도착하므로 가장 높게.
>   (DMA circular + 링버퍼라 실제로는 데이터 유실 위험이 낮지만, 지연(latency)을 최소화)
> - Router는 FPGA보다 낮고 통신 태스크보다 높게 두어 큐가 쌓이지 않게 함.
> - WiFi는 AT 응답 대기로 수 초씩 블로킹될 수 있으므로 중간 우선순위.
> - CLI는 사람이 입력하는 저속 경로이므로 가장 낮은 축.
> - USB TX는 CDC busy 시 재시도 폴링이 있어 최하위.

---

## 3. Queues (Tasks and Queues 탭 → Queues)

CubeMX에는 등록하지 않고 `app_main.c` 에서 생성합니다. (태스크와 같은 이유)
아래는 코드에서 사용하는 실제 파라미터입니다.

| 큐 | 항목 크기 | 길이 | 총 바이트 | 생산자 → 소비자 | 가득 찼을 때 정책 |
|---|---|---|---|---|---|
| `qSample` | `sd_sample_t` (24 B) | **32** | 768 | tskFpga → tskRouter | 가장 오래된 항목 폐기 후 push (drop-oldest) |
| `qWifiTx` | `sd_line_t` (98 B) | **32** | 3136 | tskRouter/CLI → tskWifi | drop-oldest (전송 지연 누적 방지) |
| `qUsbTx` | `sd_line_t` (98 B) | **16** | 1568 | tskRouter/CLI → tskUsbTx | drop-oldest |
| `qUsbRx` | `uint8_t` | **512** | 512 | CDC_Receive_FS(ISR) → tskCliUsb | 폐기 (drop-newest) |

> `sd_line_t` = `{ uint16_t len; char data[96]; }`
> drop-oldest 는 `osMessageQueuePut(q, item, 0, 0)` 실패 시 `osMessageQueueGet(q,&tmp,0,0)`
> 로 하나 버리고 다시 put 하는 방식으로 구현했습니다 (`App/Src/app_main.c: sd_queue_put_overwrite`).

---

## 4. Semaphores / Mutexes

### 4.1 Binary / Counting Semaphores

| 이름 | 종류 | 초기값 | Give (누가) | Take (누가) | 목적 |
|---|---|---|---|---|---|
| `semTrig` | Counting (max 4) | 0 | `HAL_GPIO_EXTI_Falling_Callback` (ISR) | tskFpga | FPGA 트리거 이벤트 전달. 카운팅으로 두어 트리거가 몰려도 유실 없음 |
| `lnkUart1.rx_sig` | Counting (max 8) | 0 | `HAL_UARTEx_RxEventCallback` (ISR) | tskWifi | USART1 수신 데이터 도착 알림 |
| `lnkUart2.rx_sig` | Counting (max 8) | 0 | 〃 | tskFpga | USART2 수신 데이터 도착 알림 |
| `lnkUart3.rx_sig` | Counting (max 8) | 0 | 〃 | tskCliUart | USART3 수신 데이터 도착 알림 |
| `lnkUartX.tx_done` | Binary | **1 (available)** | `HAL_UART_TxCpltCallback` (ISR) | `uartlink_write()` | DMA 송신 완료 대기 |

> `tx_done` 을 "초기값 1" 로 두는 이유: 전송 시작 전에 acquire → DMA 시작 →
> 완료 ISR에서 release 하는 패턴이 아니라, **acquire(대기) → 전송 → 완료 시 release**
> 로 쓰기 위해 첫 전송이 즉시 통과해야 하기 때문입니다. 코드에서는
> `osSemaphoreNew(1, 1, ...)` (max=1, initial=1) 로 생성 후,
> 송신 시작 직전에 `osSemaphoreAcquire(tx_done, 0)` 로 잔여 토큰을 비우고
> 전송 완료를 `osSemaphoreAcquire(tx_done, timeout)` 로 기다립니다.

### 4.2 Mutexes

| 이름 | 종류 | 보호 대상 | 사용 태스크 |
|---|---|---|---|
| `mtxFlash` | Mutex (**priority inheritance**) | SPI2 + W25Q40 CS. 읽기/쓰기/소거 전 구간 | tskCliUart, tskCliUsb, defaultTask |
| `mtxCfg` | Mutex | RAM 상의 `g_cfg` 구조체 (read/modify/write) | 모든 CLI 태스크, tskWifi |
| `mtxAt` | Mutex | ESP32 AT 세션(명령-응답 원자성) | tskWifi (+ 진단 명령 시 CLI) |
| `lnkUartX.tx_mtx` | Mutex ×3 | 각 UART의 TX 경로 동시 접근 방지 | 해당 UART를 쓰는 모든 태스크 |

> **Recursive mutex 는 사용하지 않습니다.** 다만 `w25q40.c` 내부 함수가 서로를
> 호출할 수 있으므로 락은 **공개 API(`cfgstore_*`, `w25q_*`) 진입점에서만** 잡습니다.

### 4.3 Event Flags

| 이름 | 비트 | 의미 |
|---|---|---|
| `evtSys` | `SD_EVT_WIFI_UP` (0x01) | ESP32 AP 접속 완료 |
|  | `SD_EVT_TCP_UP` (0x02) | 서버 TCP 연결 완료 |
|  | `SD_EVT_FPGA_RUN` (0x04) | FPGA에 START 전송 완료, 수집 중 |
|  | `SD_EVT_CFG_DIRTY` (0x08) | 설정 변경됨(SAVE 필요) |
|  | `SD_EVT_WIFI_RECONF` (0x10) | 설정 변경으로 WiFi 재연결 요구 |

`tskWifi` 는 `SD_EVT_WIFI_RECONF` 를 폴링하여(비블로킹) 상태머신을 RESET으로 되돌립니다.

### 4.4 Software Timers

| 이름 | 주기 | 타입 | 콜백 동작 |
|---|---|---|---|
| `tmrStat` | 10 s | Periodic | 통계(수신/전송/드롭 카운트)를 `SD_LOG` 로 출력. 디버그용 |

> CubeMX Timers 탭에 등록하지 않고 코드에서 `osTimerNew()` 로 생성합니다.

---

## 5. 데이터 흐름 (한 사이클)

```
 FPGA ── falling edge ──▶ PA1 EXTI1 ISR
                              │ osSemaphoreRelease(semTrig)
                              ▼
                        [tskFpga  prio 48]
                              │ uartlink_read(USART2, 18 bytes, 200ms)
                              │ CRC8 검증 → sd_sample_t 생성
                              │ osMessageQueuePut(qSample)
                              ▼
                        [tskRouter prio 32]
                              │ "SD,seq,tick,ch0..ch5,st\r\n" 포맷
                    ┌─────────┼──────────────┬─────────────────┐
                    ▼         ▼              ▼                 ▼
             USART3 직접전송  qUsbTx      qWifiTx          (cfg.out_* 플래그로 on/off)
                    │          │              │
                    │      [tskUsbTx]     [tskWifi prio 24]
                    │          │              │ AT+CIPSEND=<len> → ">" → payload → "SEND OK"
                    ▼          ▼              ▼
                  RS485      USB CDC      ESP32 → TCP Server(50001)
```

---

## 6. 힙 / 스택 산정

### 6.1 FreeRTOS heap_4 (40 KB) 사용량
| 항목 | 계산 | 바이트 |
|---|---|---|
| 태스크 TCB ×8 | 8 × ~120 | 960 |
| 태스크 스택 | (512+640+1024+768+768+384+512+256) × 4 | 19,456 |
| `qSample` | 32 × 24 + 오버헤드 | ~850 |
| `qWifiTx` | 32 × 98 + 오버헤드 | ~3,230 |
| `qUsbTx` | 16 × 98 + 오버헤드 | ~1,650 |
| `qUsbRx` | 512 × 1 + 오버헤드 | ~600 |
| 세마포어/뮤텍스 ×12 | 12 × 80 | ~960 |
| 타이머 ×1 | ~50 | 50 |
| 여유 (fragmentation) | | ~12,000 |
| **합계** | | **40,960** |

### 6.2 정적 버퍼 (BSS)
| 버퍼 | 크기 |
|---|---|
| `u1_rx[512]` USART1 DMA 링 | 512 |
| `u2_rx[256]` USART2 DMA 링 | 256 |
| `u3_rx[512]` USART3 DMA 링 | 512 |
| `at_line[256]` ESP AT 파싱 | 256 |
| `cli_line[192]` ×2 | 384 |
| `g_cfg` + shadow | ~400 |
| **합계** | **약 2.4 KB** |

### 6.3 스택 사용량 확인 방법
`uxTaskGetStackHighWaterMark()` 가 활성화되어 있으므로,
CLI 에서 `STATUS` 명령을 치면 각 태스크의 남은 스택(Words)이 출력됩니다.
남은 값이 **64 Words 미만**이면 해당 태스크 스택을 키우십시오.

---

## 7. FreeRTOS 사용 시 주의사항 체크리스트

- [ ] **SYS Timebase Source 를 TIM6 로 변경** (SysTick 충돌 방지) — 01 문서 5장
- [ ] FreeRTOS API를 호출하는 모든 인터럽트의 Preemption Priority ≥ **5**
- [ ] `HAL_Delay()` 를 태스크 안에서 쓰지 말 것 → `osDelay()` 사용
      (본 코드는 `HAL_Delay` 를 초기화 구간 외에는 쓰지 않습니다)
- [ ] `printf` / `sprintf` 사용 시 태스크 스택 여유 확보 (본 코드는 `snprintf` 만 사용,
      부동소수점 포맷 미사용 → newlib-nano로 충분)
- [ ] DMA 버퍼는 반드시 **전역/정적** (태스크 스택에 두면 안 됨)
- [ ] `osKernelStart()` 이후에는 `main()` 으로 돌아오지 않음
- [ ] `configASSERT` 활성 상태에서 디버깅 후 릴리즈 시 비활성 검토
