# STM32L562CEU6 ↔ FPGA(EP4CE10, Nios II) UART(TTL) 링크

STM32L562CEU6(UFQFPN48) 이 FPGA(SGDT_DC_6CH, Nios II `uart1`) 와 3.3V TTL UART 로
**바이트 손실 없이, 바이트 간 지연 요구 없이** 통신하기 위한 방법과 코드입니다.

- 수신 : **UART + DMA 순환 버퍼 + IDLE 라인 감지** (`HAL_UARTEx_ReceiveToIdle_DMA`)
- 송신 : **명령 큐 + DMA**, 명령 사이 최소 간격 보장
- 프로토콜 : FPGA 펌웨어(`pc_cmd.c`, `adc_ltc2292.c`)와 동일한 `STX … \r\n` / `STX … ETX`
- 프로토콜 처리부(`fpga_proto.c`)는 HAL 과 분리 → PC 에서 단위 테스트

---

## 1. DMA 를 써야 하나? — 결론: **수신은 DMA 필수, 송신은 선택**

| 방식 | 수신 | 송신 |
|------|------|------|
| **폴링** (`HAL_UART_Receive` / RXNE 확인) | ✗ 메인루프가 1문자 시간(115200bps ≈ 87 µs) 이상 멈추면 바이트 손실. L5 의 8바이트 RX FIFO 를 켜도 약 0.7 ms 까지만 버팀. 같은 MCU 가 ESP32 AT 명령을 **블로킹으로 기다리는** 구조(`esp32_at.c`)라 사실상 불가 | ○ 명령이 짧아(10~30바이트, 1~3 ms) 블로킹 송신도 가능 |
| **바이트 인터럽트** (`HAL_UART_Receive_IT(1바이트)` 반복) | △ 115200bps 에서 초당 11,520회 인터럽트. 콜백에서 다시 `Receive_IT` 를 거는 사이에 다음 바이트가 오면 **ORE(오버런) → HAL 이 수신 중단**. 흔히 "바이트 사이 간격을 줘야 된다"는 증상의 원인 | ○ |
| **DMA 순환 + IDLE** (이 코드) | ◎ 바이트마다 CPU 개입 0. 버퍼 절반/끝/IDLE 마다 1번 콜백. 메인루프가 수 초 멈춰도 패킷 큐(8개)가 찰 때까지 손실 없음. 간격 0 연속 수신 OK | ◎ CPU 를 붙잡지 않음 + 명령 간격 스케줄링 |

- STM32L562 는 **D-cache 가 없으므로**(ICACHE 만 있음) DMA 버퍼 캐시 정합성 문제는 없습니다.
- DMA 채널 : DMA1 CH1 (RX, Circular), DMA1 CH2 (TX, Normal), DMAMUX 요청 `UART4_RX/TX`.

## 2. 배선

```
 STM32L562CEU6                         FPGA EP4CE10 (top.v)
 ─────────────                         ─────────────────────
 PA0  UART4_TX  ──[33~100Ω]──────────▶ UART1_RX   (Nios uart1 rxd)
 PA1  UART4_RX  ◀──────────[33~100Ω]── UART1_TX   (Nios uart1 txd)
 PB1  GPIO 입력 ◀───────────────────── MCU_RX     (Nios PIO mcu_rx : 송신 전 Low)   ※ 선택
 PA8  GPIO 출력 ──────────────────────▶ MCU_TX     (Nios PIO mcu_tx : 현재 미사용)   ※ 선택
 GND ─────────────────────────────────── GND       (반드시 공통)
```

| 항목 | 내용 |
|------|------|
| 전압 | 둘 다 3.3V LVTTL — 레벨 변환 불필요. FPGA 해당 뱅크 VCCIO 가 3.3V 인지 확인 |
| 직렬 저항 | 송신단 가까이 33~100Ω → 링잉/EMI 감소, 한쪽 전원만 켜졌을 때 역전류 제한 |
| 풀업 | STM32 RX(PA1) 내부 풀업 사용(코드에서 설정). FPGA 는 Quartus *Pin Planner → Weak Pull-Up* 을 UART1_RX 에 지정. FPGA 컨피그·Nios 부팅 중 TX 가 Hi-Z 여도 라인이 High(IDLE) 유지 |
| 전원 순서 | 한쪽이 꺼진 상태에서 다른 쪽이 High 를 내보내면 보호 다이오드로 역급전 → 동시 전원 또는 직렬 저항 |
| 핀 | UART4 = **PA0/PA1 (AF8)** 은 USART1(PA9/10, ESP32), USART2(PA2/3, 디버그)와 겹치지 않음. 다른 포트가 필요하면 `fpga_link.h` 상단만 변경 (예: USART3 PB10/PB11 AF7). **CubeMX 에서 해당 핀이 UFQFPN48 에 있는지 확인**하세요 |

## 3. 보레이트

| 보레이트 | STM32 (HSI16, OVER16) | FPGA (50 MHz, altera_avalon_uart) | 판정 |
|----------|----------------------|-----------------------------------|------|
| **115200** | BRR 139 → −0.08% | 분주 434 → +0.01% | ◎ 권장 (현재) |
| 230400 | BRR 69 → +0.6% | 217 → −0.02% | ○ |
| 460800 | BRR 35 → −0.8% | 109 → −0.5% | △ |
| 921600 | BRR 17 → +2.1% | 54 → +0.5% | ✗ HSI16 으로는 불가 |

- UART 커널 클럭을 **HSI16** 으로 고정 → SYSCLK 을 바꿔도 보레이트 불변. HSI16 자체 정확도(온도 포함 약 ±1% 수준, 데이터시트 확인)가
  오차의 대부분이므로 8-N-1 허용치(양측 합 약 ±3%) 안에 들어옵니다. 더 빠르게 하려면 HSE 또는 PCLK(110 MHz) 로 클럭 변경.
- 실제 데이터량은 ADC 프레임 약 50~70바이트/초 (ReadInterval 1 s) → 115200bps 의 0.6% 수준이라 속도를 올릴 이유는 없습니다.

## 4. 프로토콜

| 방향 | 프레임 | 예 |
|------|--------|----|
| MCU → FPGA | `STX` + `CMD[,p1,…]` + `\r\n` | `\x02START\r\n`, `\x02SW_ALL,5000,100,1\r\n` |
| FPGA → MCU (ADC) | `STX` + `"p0,c0,p1,c1,…,p5,c5"` + `\r\n` | 12개 값 = 채널별 피크(mV 환산)/카운트 |
| FPGA → MCU (응답) | `STX` + 값 + `\r\n` 또는 `ETX` | `SR_ALL` → `5000,100,1000` / `SR_OFFSV` → `100` + ETX |

수신 파서 규칙 (FPGA 쪽 파서와 동일하게 맞춤)
- `STX` 는 항상 새 프레임 시작 (끊긴 프레임은 버려짐)
- `\n` 또는 `ETX` 로 종료, `\r` 은 무시, 프레임 밖 바이트는 버림
  → 원본 FPGA 펌웨어가 보내는 **0x00 패딩(100바이트 고정 전송)도 자동으로 걸러짐** (`stats.rx_garbage` 로 카운트)
- payload 128자 초과 시 버림 (`stats.rx_too_long`)

## 5. 파일

```
Core/Inc/fpga_link.h     ← ★ 핀 / UART / DMA 채널 / 보레이트 / 버퍼 / 명령 간격 설정, API
Core/Src/fpga_link.c     ← HAL 초기화(클럭·GPIO·DMA·NVIC), DMA 수신 이벤트, 송신 큐, IRQ 핸들러, HAL 콜백
Core/Inc/fpga_proto.h    ← 프레임 형식, 패킷/통계 구조체
Core/Src/fpga_proto.c    ← 프레임 파서, 패킷 큐(ISR→main), 명령 생성, 숫자 추출 (HAL 비의존)
Core/Inc/fpga_app.h
Core/Src/fpga_app.c      ← 사용 예: START/STOP/SW_ALL/SR_ALL 송신, ADC 12개 값 수신 처리
test/test_fpga_proto.c   ← PC 단위 테스트
```

## 6. 프로젝트에 넣는 방법 (STM32CubeIDE)

1. 위 6개 파일을 `Core/Inc`, `Core/Src` 에 복사
2. **CubeMX 에서 UART4 / DMA1 CH1·CH2 를 설정하지 마세요** — `FPGA_Link_Init()` 이 클럭·GPIO·DMA·NVIC 를 직접 설정합니다.
   (CubeMX 로 설정하고 싶다면 같은 값으로 설정한 뒤 `FPGA_LINK_DEFINE_IRQ_HANDLERS 0` 으로 하고,
   CubeMX 가 만든 IRQ 핸들러를 그대로 쓰면 됩니다)
3. `stm32l5xx_hal_conf.h` 에 `HAL_UART_MODULE_ENABLED`, `HAL_DMA_MODULE_ENABLED` 가 켜져 있는지 확인
4. `main.c`
   ```c
   /* USER CODE BEGIN Includes */
   #include "fpga_app.h"
   ...
   /* USER CODE BEGIN 2 */
   FPGA_App_Init();                 /* UART4+DMA 시작, SR_ALL, START 전송 */
   ...
   while (1)
   {
     FPGA_App_Process();            /* 송신 스케줄 + 수신 패킷 처리 */
     if (FPGA_App_AdcUpdated())
     {
       const FPGA_AdcData *a = FPGA_App_GetAdc();
       /* a->peak_val[0..5], a->peak_cnt[0..5] 사용 (Wi-Fi 로 서버 전송 등) */
     }
     ...
   }
   ```
5. 다른 곳에서 `HAL_UART_TxCpltCallback` / `HAL_UART_ErrorCallback` / `HAL_UARTEx_RxEventCallback` 을 이미 정의했다면
   `FPGA_LINK_DEFINE_HAL_CALLBACKS 0` 으로 하고 기존 콜백 안에서 `FPGA_Link_TxCpltHandler()` 등을 호출

> `STM32L562_ESP32C3_WiFi` 프로젝트와 합칠 때: 그 프로젝트는 USART1/2 를 폴링으로 쓰고 HAL UART 콜백을 정의하지 않으므로
> 그대로 추가하면 됩니다. ESP AT 명령을 블로킹으로 기다리는 동안에도 FPGA 데이터는 DMA + 인터럽트로 계속 패킷 큐에 쌓입니다
> (1초 주기 데이터 기준 약 7초까지 손실 없음, 더 필요하면 `FPGA_PKT_QUEUE_LEN` 증가).

## 7. FPGA(Nios II) 쪽과 맞출 것

| 항목 | 권장 |
|------|------|
| `uart1_string_write()` 의 송신 전 20 ms 대기 (`UART1_PRE_DELAY_MS`) | MCU 가 DMA 로 항상 수신 대기 중이므로 **0~1 ms 로 줄여도 됨**. 단, MCU 가 STOP 저전력 모드에서 MCU_RX 하강엣지(PB1 EXTI)로 깨어나는 구조라면 MCU 기동 시간만큼 남길 것 |
| ADC 프레임 100바이트 고정 전송 | MCU 파서가 0x00 을 걸러내므로 동작은 하지만, 문자열이 100자를 넘으면 `\r\n` 이 잘림 → FPGA 쪽 `strlen` 수정(`FPGA_UART_Cyclone4/nios_fix`) 적용 권장 |
| MCU → FPGA 명령 간격 | `FPGA_CMD_MIN_GAP_MS` = 5 ms 기본. 원본 Nios 펌웨어는 패킷 처리 중 다음 패킷이 오면 버퍼를 덮어쓰므로(R2) 간격 필요. 수정된 Nios 펌웨어(링버퍼)면 0 으로 해도 됨 |
| 응답 종료문자 | `SR_OFFSV`, `SR_RDTIME` 은 ETX, 나머지는 `\r\n` — 이 파서는 둘 다 처리 |
| `SW_ALL` 응답 | 원본 FPGA 는 uart2(디버그)로 응답 → MCU 는 응답을 받지 못함. 필요하면 `SR_ALL` 로 다시 읽어 확인 (`FPGA_App_ReadAll()`) |

## 8. 디버깅

`FPGA_Link_GetStats()` 로 통계를 확인 (디버거 Live Expression 에 `*FPGA_Link_GetStats()`)

| 값 | 정상 | 증가하면 |
|----|------|----------|
| `rx_packets` | 1초마다 +1 (START 이후) | — |
| `uart_errors` | 0 | 보레이트 불일치(FE), 배선 노이즈(NE), 접지 확인 |
| `rx_garbage` | 0 (원본 FPGA 펌웨어면 프레임마다 +30~60) | 수정된 FPGA 펌웨어인데 증가 → 보레이트/노이즈 |
| `rx_queue_full` | 0 | 메인루프가 `FPGA_App_Process()` 를 너무 드물게 호출 → 큐 크기 증가 |
| `rx_too_long` | 0 | FPGA 프레임에 `\n` 누락 (100바이트 잘림 문제) |
| `rx_restarts` | 1 (초기화 시) | ORE/DMA 에러로 재시작됨 |

## 9. 검증

- `arm-none-eabi-gcc 13.2` + STM32CubeL5 HAL(GitHub 최신) 로 `-Wall -Wextra -Werror` 컴파일 및 HAL 과 링크 확인
  (코드 약 2.2 KB, RAM 약 2.8 KB)
- `test/test_fpga_proto.c` (PC): 원본 FPGA 출력(0x00 패딩), ETX 응답, 끊긴 프레임, 1/4/7바이트 단위 분할 입력,
  긴 프레임, 큐 가득 참, 명령 생성 — PASS
  ```bash
  gcc -Wall -ICore/Inc test/test_fpga_proto.c Core/Src/fpga_proto.c -o t && ./t
  ```
- **실제 보드에서는 아직 시험하지 않았습니다.** 특히 UFQFPN48 핀 배치(PA0/PA1/PA8/PB1)는 CubeMX 에서 확인해 주세요.
