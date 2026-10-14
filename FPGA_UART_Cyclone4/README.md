# Cyclone IV FPGA UART — 바이트 사이 지연 문제 원인과 해결

> 증상
> 1. **송신(TX)** : FPGA 가 여러 바이트를 연속으로 보낼 때 바이트와 바이트 사이에 빈 시간(IDLE)이 생긴다.
> 2. **수신(RX)** : PC/MCU 가 바이트를 간격 없이 연속으로 보내면 데이터가 깨지거나 빠지고,
>    송신측에서 바이트 사이에 일정 지연(예: 1~2 ms, 혹은 수 비트)을 줘야만 정상 수신된다.

두 증상 모두 **UART 모듈 구조(상태기계·핸드셰이크·버퍼)** 에서 나오는 전형적인 문제이며,
보레이트 자체가 틀린 문제가 아닌 경우가 대부분입니다. 이 폴더에 원인을 제거한 RTL 과
검증 테스트벤치를 넣었습니다.

| 파일 | 설명 |
|------|------|
| `rtl/uart_tx.v`   | 송신기 — STOP 비트 마지막 클럭에 다음 바이트 로드 → **바이트 간 간격 0** |
| `rtl/uart_rx.v`   | 수신기 — 2FF 동기화, 16x 오버샘플, 3샘플 다수결, **STOP 중앙에서 IDLE 복귀** |
| `rtl/uart_fifo.v` | show-ahead 동기 FIFO (RX/TX 버퍼) |
| `rtl/uart_top.v`  | 예제 탑: RX → FIFO → (사용자 로직) → FIFO → TX, 에코 |
| `uart_top.sdc`    | 타이밍 제약 |
| `sim/tb_uart.v`   | 테스트벤치 (간격 측정 + 보레이트 오차 ±4.5% 연속 수신) |

---

## 1. 송신 시 바이트 사이에 지연이 생기는 이유

| # | 원인 | 지연 크기 | 설명 |
|---|------|-----------|------|
| T1 | **STOP 후 IDLE 상태를 한 번 더 거침** | 1~수 클럭 | `STOP → IDLE → (start 확인) → START` 처럼 상태를 거치면 매 바이트마다 클럭이 소모된다. |
| T2 | **STOP 비트를 1비트보다 길게 보냄** | 0.5~1 비트 | `cnt == DIV` 비교(`DIV+1` 클럭), STOP 이후 "IDLE 1비트 대기" 상태 추가, 1.5/2 STOP 설정 등 |
| T3 | **`tx_start` 를 상승엣지로 검출 / busy 핸드셰이크 왕복** | 수 클럭~수십 클럭 | 상위 로직이 `busy` 가 내려간 것을 보고 → 데이터 준비 → `start` 를 올렸다 내렸다 해야 다음 바이트 시작 |
| T4 | **버퍼(FIFO/홀딩 레지스터) 없음** | 상위 로직 처리 시간 전체 | 다음 바이트를 STOP 비트 송신 중에 미리 받아둘 곳이 없어서, 전송이 끝난 뒤에야 다음 바이트를 만든다 |
| T5 | **CPU(Nios II 등) 폴링 송신** | 수 µs~ms | `while(busy);` 후 1바이트씩 쓰기, `printf`/JTAG UART 경유, 인터럽트 지연 |
| T6 | **보레이트 카운터를 바이트마다 리셋하지만 위상이 어긋남** | 최대 1비트 | 비트 틱이 free-running 이면 START 가 다음 틱까지 기다린다 |
| T7 | **(PC 측) USB-UART 변환기 지연** | 1~16 ms | FTDI 의 *Latency Timer* 기본 16 ms. FPGA 출력은 연속이어도 PC 앱에서 덩어리로 끊겨 보임 |

> **T7 확인 방법**: 오실로스코프/로직분석기로 FPGA TX 핀을 직접 보면 됩니다. 핀에서 간격이 0 이면
> FPGA 문제는 아닙니다. Windows 장치관리자 → 포트 → 고급 → *Latency Timer* 를 1~2 ms 로 줄이세요.

### 흔한 문제 코드 (Before)

```verilog
// ❌ T1+T2+T3 : STOP 후 IDLE 로 가서 start 상승엣지를 다시 기다림
STOP: if (cnt == DIV) begin       // DIV+1 클럭 → 매 비트 1클럭씩 길어짐
          cnt <= 0; state <= IDLE; busy <= 0;
      end else cnt <= cnt + 1;
IDLE: if (start && !start_d) begin // 상승엣지 → 상위 로직이 start 를 내렸다 올려야 함
          shreg <= data; state <= START;
      end
```

### 수정 코드 (After, `rtl/uart_tx.v`)

```verilog
wire bit_end = busy && (acc_next >= CLK_HZ);                 // 정확한 비트 경계
assign tx_ready = !busy || (bit_end && bit_cnt == 4'd0);      // ★ STOP 마지막 클럭에도 ready

always @(posedge clk)
    if (tx_valid && tx_ready) begin                           // 레벨 valid/ready 핸드셰이크
        shreg <= {1'b1, tx_data};  txd <= 1'b0;               // 바로 START 비트
        bit_cnt <= 4'd9;  busy <= 1'b1;
        acc <= busy ? (acc_next - CLK_HZ) : 32'd0;            // 연속 송신 시 위상 유지
    end ...
```

- `tx_valid`/`tx_ready` **레벨 핸드셰이크** (엣지 검출 없음) → T3 제거
- **STOP 비트가 끝나는 그 클럭**에 다음 바이트를 받아 곧바로 START 출력 → T1, T2 제거
- TX 앞에 **FIFO** 를 두고 `tx_valid = ~fifo_empty` → 상위 로직 처리 시간과 무관 (T4, T5 완화)
- 보레이트는 **분수 누산기(NCO)** 로 생성: 정수 분주 반올림 오차 0, 연속 송신 시 위상 유지 (T6 제거)

---

## 2. 수신 시 바이트 사이에 간격을 줘야만 되는 이유

| # | 원인 | 왜 "간격을 주면 되는가" |
|---|------|------------------------|
| R1 | **STOP 비트 끝(또는 그 이후)까지 기다린 뒤 IDLE 복귀** ★가장 흔함 | 간격 0 이면 다음 START 하강엣지가 수신기가 아직 STOP 상태일 때 와서 놓친다. 이후 데이터 비트 중간의 하강엣지를 START 로 오인 → 쓰레기 데이터. 간격을 주면 IDLE 복귀 후에 엣지가 와서 정상 |
| R2 | **보레이트 오차 (FPGA 쪽이 느림)** | 정수 분주(예: 50 MHz/115200 = 434.03 → 434, 16x 시 27 → +0.47%) 오차 + 상대 MCU 크리스탈/RC 오차. FPGA 비트가 길면 프레임 끝이 늦어져 R1 과 겹쳐 실패. 간격이 오차를 흡수 |
| R3 | **START 검출 지연이 프레임마다 누적** | START 를 늦게 검출하면 그 프레임 전체 샘플 위치가 뒤로 밀리고, STOP 끝까지 기다리는 구조면 다음 프레임 검출이 또 늦어지는 식으로 누적 |
| R4 | **`rx_valid` 1클럭 펄스를 상위 로직이 놓침 / 데이터 레지스터 덮어쓰기** | 상위 FSM 이 다른 상태에 있을 때 들어온 바이트는 사라지거나 다음 바이트로 덮인다. 간격이 있으면 상위 로직이 처리를 끝내고 돌아올 시간이 생김 |
| R5 | **입력 동기화 FF 없음 / 엣지 검출을 비동기 신호로 직접** | 메타스테빌리티로 상태기계가 가끔 이상 상태로 진입. 연속 수신에서 확률이 더 높아짐 |
| R6 | **비트 샘플을 1회만 (중앙 다수결 없음)** | 노이즈·링잉에 약함. 바이트 연속일 때 엣지가 많아 오류율 상승 |

### 흔한 문제 코드 (Before)

```verilog
// ❌ R1 : STOP 비트 "끝"까지(혹은 STOP 이후 추가 대기) 기다림
STOP: if (os_cnt == 15) begin     // 중앙(8)에서 시작했다면 여기는 STOP 끝 + α
          rx_done <= 1; state <= IDLE;
      end
// ❌ R5 : 비동기 입력을 그대로 사용
IDLE: if (rxd == 1'b0) state <= START;
```

> 테스트벤치로 확인: 위처럼 "STOP 끝까지 대기" 하도록 바꾼 버전은 **보레이트 오차 0%** 에서도
> 간격 0 연속 수신 시 8번째 바이트 부근부터 데이터가 깨집니다(검출 지연 누적, R1+R3).

### 수정 코드 (After, `rtl/uart_rx.v`)

```verilog
reg [1:0] sync;  always @(posedge clk) sync <= {sync[0], rxd};   // ★ R5: 2FF 동기화
...
S_STOP: if (os_cnt == OS-1) begin      // ★ R1: STOP 비트 "중앙"에서 판정 후 즉시 IDLE
            rx_data  <= shreg;
            rx_valid <= maj;           // STOP=1 이면 유효
            rx_ferr  <= ~maj;          // STOP=0 이면 프레이밍 에러
            state    <= S_IDLE;        // 남은 반 비트 동안 다음 START 하강엣지 감시
        end
```

- **STOP 중앙에서 IDLE 복귀** → 다음 START 까지 0.5비트 여유. 송신측이 수 % 빨라도 엣지를 놓치지 않음
- 16x 오버샘플 + **3샘플 다수결**(R6), START 중앙에서 재확인해 글리치 제거
- 오버샘플 틱도 **NCO** 로 생성 → 평균 보레이트 오차 0 (R2)
- `rx_valid` 는 **바로 FIFO 에 기록** → 상위 로직은 `!empty` 일 때 천천히 꺼내면 됨 (R4)

---

## 3. 수정 사항 요약 (체크리스트)

| 구분 | 수정 전 (문제) | 수정 후 |
|------|---------------|---------|
| 보레이트 | `CLK/BAUD` 정수 분주, `cnt == DIV` 비교 | 분수 누산기(NCO) `acc += BAUD; if (acc >= CLK) acc -= CLK;` |
| TX 핸드셰이크 | `start` 엣지 + `busy` 폴링 | `valid/ready` 레벨, STOP 마지막 클럭에 `ready=1` |
| TX 버퍼 | 없음 | TX FIFO (`tx_valid = ~empty`) |
| TX STOP 이후 | IDLE 상태 경유/추가 대기 | 대기 바이트 있으면 바로 START |
| RX 입력 | 비동기 직접 사용 | 2단 동기화 FF (+ SDC false path) |
| RX 샘플 | 비트당 1회 | 16x 오버샘플, 중앙 3샘플 다수결 |
| RX STOP | STOP 끝까지 대기 | **STOP 중앙에서 IDLE 복귀** |
| RX 출력 | 1바이트 레지스터, 펄스 놓치면 손실 | RX FIFO, overflow/framing error 플래그 |
| PC 측 | — | USB-UART Latency Timer 1~2 ms, 송신 앱은 버퍼 단위로 Write |

추가로 확인할 것:
- **클럭 주파수 파라미터**가 실제 보드 클럭과 같은지 (`CLK_HZ`). PLL 출력 클럭을 쓰면 그 주파수로.
- **보레이트 허용 오차**: 8-N-1 에서 양측 합계 약 ±2~3% 이내여야 함. 이 설계는 FPGA 측 오차 ≈ 0 이므로
  상대 장치 오차를 거의 전부 흡수 (시뮬레이션에서 ±4.5% 연속 수신 통과).
- 상위 로직이 처리 속도보다 데이터가 빨리 들어오면 FIFO 깊이(`FIFO_AW`)를 늘리거나 RTS/CTS 흐름제어 사용.
- Nios II 를 쓰는 경우 Altera UART IP 대신 이 모듈 + FIFO 를 Avalon-MM 으로 래핑하거나,
  **JTAG UART 대신 FIFO 가 있는 UART(예: `altera_avalon_uart` + 인터럽트/DMA)** 를 사용.

---

## 4. 사용 방법 (Quartus)

1. `rtl/*.v`, `uart_top.sdc` 를 프로젝트에 추가, Top-level = `uart_top`
2. 파라미터 `CLK_HZ`, `BAUD` 를 보드에 맞게 설정 (기본 50 MHz / 115200)
3. 핀 할당 예 (보드 회로도 기준으로 변경)

   | 신호 | 설명 |
   |------|------|
   | `CLOCK_50` | 50 MHz 오실레이터 |
   | `RESET_N`  | 푸시버튼 (active low) |
   | `UART_RXD` | USB-UART TXD → FPGA (I/O 표준 3.3-V LVTTL, 필요시 Weak Pull-Up ON) |
   | `UART_TXD` | FPGA → USB-UART RXD |
   | `LED[0]`   | RX FIFO overflow (sticky) |
   | `LED[1]`   | Framing error 발생 (sticky) |

4. 기존 설계에 적용할 때: 사용자 로직은 `uart_top.v` 의 "사용자 로직 자리"에서
   RX FIFO(`rxf_dout`, `rxf_empty`, `rxf_rd`) 를 읽고 TX FIFO(`wr_en`, `din`, `txf_full`) 에 쓰면 됩니다.
   Quartus IP 를 선호하면 `uart_fifo` 를 `SCFIFO`(show-ahead ON) 로 바꿔도 동일하게 동작합니다.

## 5. 시뮬레이션

```bash
iverilog -g2005 -o sim.out -DERR_PPM=30000 sim/tb_uart.v rtl/*.v && vvp sim.out
```

`ERR_PPM` = 상대 송신기의 보레이트 오차 (+ 빠름 / − 느림). 결과:

| 테스트 | 결과 |
|--------|------|
| TEST1 `uart_tx` 연속 30바이트, 프레임 간 최대 간격 | **14.4 ns (0.002 비트, 1클럭 미만 = NCO 양자화)** |
| TEST2 간격 0 연속 200바이트 수신+에코, 오차 0 ppm | PASS |
| 〃 오차 ±30000 ppm (±3%) | PASS |
| 〃 오차 ±45000 ppm (±4.5%) | PASS |
| (비교) RX 를 "STOP 끝까지 대기" 로 바꾼 버전, 0 ppm | **FAIL** (8번째 바이트부터 깨짐) |

---

## 6. 실제 프로젝트(SGDT_DC_6CH, Nios II) 코드 분석과 수정

대상 FPGA : **EP4CE10E22C8N** (Cyclone IV E, 10,320 LE, M9K 46개, PLL 2개, EQFP144)

원본: [`nios_orig/`](nios_orig/) (main.c/h, nios_io.c/h, pc_cmd.c/h, adc_ltc2292.c/h, top.v)
수정본: [`nios_fix/`](nios_fix/) (위 C/H 파일 + fifo_uart_regs.h)

`top.v` 는 UART 핀(`UART1_RX/TX`, `UART2_RX/TX`)을 Platform Designer 의 `altera_avalon_uart`
에 그대로 연결하고 있으므로, UART 동작은 **Nios II 소프트웨어 + altera_avalon_uart 코어**가 결정합니다.

### 6-1. 송신 지연 원인

| # | 위치 | 내용 |
|---|------|------|
| S1 ★ | `nios_io.c` `uart1_string_write()` | 호출할 때마다 `MCU_RX` Low → **20 ms 대기** → 송신 → **3 ms 대기**. 메시지 사이에 최소 23 ms 공백. `ADC_Data_to_MCU_All()` 이 채널별로 여러 번 호출하면 그만큼 곱해짐 (6채널이면 약 140 ms) |
| S2 | `uart1/2_string_write()` 폴링 루프 | `altera_avalon_uart` 의 TX 버퍼는 **TXDATA 1바이트 + 시프트 레지스터** 뿐. 송신 루프 도중 `timer_1ms_isr`/UART ISR 이 끼어들어 1문자 시간 이상 걸리면 그 시간만큼 바이트 사이에 공백 (보레이트가 높거나 Nios II/e 일수록 잘 보임) |
| S3 ★ | `uart2_string_write()` (RS485) | 마지막 바이트를 TXDATA 에 쓰고 **TRDY 만 보고** `for(i<3){}`(최적화되면 0클럭) 후 바로 `UART2_DE` Low. TRDY 는 "홀딩 레지스터가 비었다"는 의미라 **마지막 1~2바이트가 아직 송신 중**에 드라이버가 꺼짐 → 상대는 끝 바이트가 깨짐/누락. 그래서 "지연을 넣어야 된다"는 증상으로 보임 |

### 6-2. 수신 시 간격이 필요한 원인

| # | 위치 | 내용 |
|---|------|------|
| R1 ★ | `altera_avalon_uart` 코어 | **RX 버퍼가 1바이트(FIFO 없음)**. ISR 이 1문자 시간(115200bps = 86.8 µs) 안에 RXDATA 를 읽지 못하면 다음 바이트가 덮어씀(ROE). HAL 인터럽트 진입 오버헤드 + timer ISR 과 겹치면 발생. 원본 ISR 은 STATUS 의 ROE 를 보지 않아 **조용히 사라짐** |
| R2 ★ | `uart1_rxd_isr()` ↔ `Mcu_Command_Proc()` | 패킷 버퍼(`UART1_RxBuf`) 하나를 ISR 과 메인루프가 공유. 메인이 패킷을 처리하는 중에 다음 패킷의 STX 가 들어오면 ISR 이 `RxCnt=0`, `RxOK_Flag=0` 으로 만들고 버퍼를 덮어씀 → MCU 가 연속으로 보내면 깨지고, 간격을 주면 정상 |
| R3 | `main.h` / `main.c` | `volatile` 선언이 주석 처리되어 있음. `TickCnt`, `WaitCnt` 는 main.c 에서 일반 변수로 정의, nios_io.c 에서는 `extern volatile` 로 선언(불일치). 최적화(-O1 이상)에서 ISR 이 바꾼 값을 못 볼 수 있음 |
| R4 | `uart1_rxd_isr()` | `UART1_RxCnt > RXBUFSIZE` 검사가 쓰기 **후**, 그리고 `>` 라서 `RxBuf[100]`, `RxBuf[101]`… 계속 씀 → '\n' 없는 쓰레기가 들어오면 인접 전역변수 오염 |
| R5 | `uart2` RS485 | 트랜시버 /RE 가 항상 Low 면 송신 중 자기 에코가 UART2 RX 로 들어와 STX 로 오인 |

### 6-3. 수정 1 — 소프트웨어만 수정 (`nios_fix/`, 하드웨어 변경 없음)

| 파일 | 수정 내용 |
|------|-----------|
| `nios_io.c` | UART ISR 을 이곳으로 이동. **ISR 은 RXDATA 를 읽어 512바이트 링버퍼에 넣기만** 함 (최소 실행시간 → R1 완화). STATUS 의 ROE/FE/PE/BRK 확인 후 클리어, 횟수를 `Uart1_Ring.hw_error` 에 기록 |
| `nios_io.c` | `Uart_Rx_Poll()` 추가: 메인루프에서 링버퍼를 읽어 패킷 조립. 원본과 같은 프로토콜/플래그(`UART1_RxOK_Flag`, `UART1_RxBuf` …)를 쓰므로 **pc_cmd.c 수정 불필요**. 패킷 1개 완성 시 리턴하고 나머지는 링버퍼에 남김 → 처리 중 덮어쓰기 없음 (R2), 버퍼 경계 검사 (R4) |
| `nios_io.c` | `uart1/2_string_write()`: 마지막 바이트 후 **TMT(송신 완료)** 까지 기다린 뒤 DE/MCU_RX 해제 (S3). 빈 for 루프 제거. 20 ms/3 ms 대기는 `nios_io.h` 의 `UART1_PRE_DELAY_MS`, `UART1_POST_DELAY_MS` 로 분리 (S1) |
| `nios_io.c` | RS485 송신 중 UART2 수신 바이트 버림 (`UART2_DISCARD_ECHO`, R5) |
| `main.c` | ISR 제거 → `Uart_Init()` 호출, 메인루프 맨 앞에 `Uart_Rx_Poll()` 추가. `TickCnt`, `WaitCnt` 를 `volatile` 로 정의. ISR 시그니처를 `void isr(void *context)` 로 정리 |
| `main.h` | `extern volatile unsigned long TickCnt, WaitCnt;` (R3) |

적용 방법: `nios_fix/` 의 5개 파일을 Eclipse 프로젝트에 덮어쓰고 빌드.

> **반드시 확인/조정할 것**
> - `UART1_PRE_DELAY_MS`(20)/`UART1_POST_DELAY_MS`(3) 는 MCU 측 핸드셰이크 요구사항에 맞춰 줄이세요.
>   메시지 간 지연의 대부분이 이 값입니다. 또 `ADC_Data_to_MCU_All()` 에서 채널마다
>   `uart1_string_write()` 를 부르고 있다면, **한 버퍼에 모아 한 번만 호출**하면 23 ms × N 이 23 ms 로 줄어듭니다.
> - 디버깅 시 `Uart1_Ring.hw_error` 가 증가하면 R1(하드웨어 overrun)이 여전히 발생 중인 것 →
>   아래 수정 2 (FIFO UART) 적용.

### 6-4. 수정 2 — 하드웨어: altera_avalon_uart → FIFO UART 교체 (근본 해결)

`altera_avalon_uart` 는 RX 1바이트 버퍼라 ISR 지연이 1문자 시간을 넘으면 데이터 손실을
피할 수 없습니다. `rtl/fifo_uart_avalon.v` 는 위 1~3절의 수신기/송신기에 **RX/TX 256바이트 FIFO** 를
붙인 Avalon-MM 컴포넌트입니다.

1. `rtl/fifo_uart_avalon.v`, `uart_rx.v`, `uart_tx.v`, `uart_fifo.v`, `fifo_uart_hw.tcl` 을
   Quartus 프로젝트 폴더(또는 `ip/fifo_uart/`)에 복사
2. Platform Designer 에서 `uart1` 의 Base 주소와 IRQ 번호를 메모 후 삭제 →
   IP Catalog *Interface Protocols > Serial > FIFO UART (8N1)* 추가, **이름을 `uart1`** 로,
   BAUD 설정, clock/reset/`s1`(Nios data_master)/`irq` 연결, Base·IRQ 를 원래 값으로
3. `external_connection` 을 Export 하고 이름을 `uart1_external_connection` 으로 →
   포트 이름이 기존과 같아 **top.v 수정 불필요** (`uart1_external_connection_rxd/txd`)
4. `uart2` 도 같은 방법으로 교체 (선택)
5. Generate HDL → Quartus 컴파일 → BSP Generate → `nios_io.h` 에서 `#define USE_FIFO_UART 1`

| 항목 | altera_avalon_uart | FIFO UART |
|------|-------------------|-----------|
| RX 버퍼 | 1 바이트 | 257 바이트 (ISR 이 256문자 시간까지 늦어도 손실 없음) |
| TX 버퍼 | 1 바이트 + 시프트 | 256 바이트 (CPU 가 한 번에 쓰고 다른 일 가능, 바이트 간 간격 0) |
| RX 판정 | 비트 중앙 1회 샘플 | 16x 오버샘플 + 3샘플 다수결, STOP 중앙 복귀 |
| 에러 | ROE/FE/PE (ISR 이 직접 확인) | RX_OVF / RX_FERR sticky 비트 |

시뮬레이션 (`sim/tb_fifo_uart_avalon.v`, 921600bps 루프백): CPU 가 64바이트를 한 번에 TX FIFO 에 쓰고,
**전송이 모두 끝난 뒤에야** RX 를 읽어도 64바이트 전부 정상 수신, overrun 0 — **PASS**.
(altera_avalon_uart 였다면 마지막 1바이트만 남고 63바이트 손실)

> 실제 보드/Quartus 에서의 컴파일·동작은 아직 확인하지 않았습니다. `fifo_uart_hw.tcl` 로드나
> 타이밍에서 문제가 생기면 메시지를 알려주세요.


### 6-5. pc_cmd.c / adc_ltc2292.c 분석 (추가)

| # | 위치 | 문제 | 수정 |
|---|------|------|------|
| A1 ★ | `ADC_Data_to_MCU_All()` | `uart1_string_write(txbuf, 100)` — **문자열 길이와 상관없이 항상 100바이트 전송**. 문자열(보통 40~70자) 뒤에 `0x00` 이 30~60개 붙어서 MCU 로 나감. 반대로 카운트 값이 커지면 문자열이 최대 105자까지 되어 **끝의 `\r\n` 이 잘림** → MCU 가 패킷 끝을 못 찾고 다음 패킷과 합쳐짐 | `snprintf` 반환값(실제 길이)으로 전송 |
| A2 | `ADC_Data_to_MCU_All()` | `fAdcAvrgSum / AdcAvrgCnt` 에서 카운트가 0 이면 `nan`/`inf` 출력 | 0 이면 0 출력 |
| A3 | `ADC_Data_Read_from_Dpram()`, `ADC_Data_Average()` | DPRAM 을 일반 포인터로 읽음. Nios II/f(데이터 캐시)면 **캐시된 옛 값**을 읽고, 최적화 시 읽기가 생략될 수 있음 | `IORD_32DIRECT()` (캐시 우회) |
| P1 | `Mcu_Command_Check()` | `UART1_RxBuf[i+1]` 를 i=99 까지 읽어 `RxBuf[100]`(범위 밖) 접근. `McuCmdBuf` 를 지우지 않아 이전의 긴 명령 잔여 데이터가 남음 | 범위 검사 + `memset` |
| P2 | `Mcu_Command_Proc()` | 파라미터 탐색이 `'\0'` 뒤까지(`n + McuCmdSize + 1`) 읽고, `McuParaPoint[20]` 범위 검사 없음 → 잔여 데이터의 ',' 로 배열 넘침 가능 | 문자열 끝까지만, 20개 제한 |
| P3 | `Mcu_Command_Check()` | 받은 명령마다 RS485(uart2)로 디버그 에코 → **송신 끝날 때까지 메인루프 정지** | `MCU_CMD_DEBUG_ECHO` 로 on/off (기본 1 = 원본 동작, 양산 시 0 권장) |
| P4 | `ADC_Data_Read_Command()` | `(long unsigned int)AdcPeakVal` → 배열 **주소**를 출력 | `AdcPeakVal[0]`, `AdcPeakCnt[0]` |

`Mcu_Command_Check()` 가 `UART1_RxOK_Flag` 를 직접 0 으로 지우는 구조임을 확인했습니다.
6-3 의 `Uart_Rx_Poll()`(패킷 1개 완성 시 리턴, 다음 호출에서 다음 패킷)과 그대로 맞물립니다.
(PC 에서 `"\x02SW_REFV,1234\r\n\x02SW_ALL,5000,100,2\r\n\x02START\r\n"` 을 **간격 없이 한 번에** 넣고
3개 명령이 순서대로 정상 처리되는 것을 단위 테스트로 확인)

**코드는 바꾸지 않았지만 MCU 측과 확인이 필요한 프로토콜 사항**

- `SR_OFFSV`, `SR_RDTIME` 응답은 끝이 `ETX(0x03)`, 나머지 응답은 `\r\n` — MCU 파서가 `\n` 을 기다리면 이 두 응답은 끝을 못 찾음
- `SW_ALL` 은 `ReadInterval = 값 × 1000`, `SW_RDTIME` 은 `ReadInterval = 값` (단위 불일치)
- `SW_ALL` 의 응답은 uart1(MCU)이 아니라 **uart2(디버그)** 로 나감 — MCU 가 응답을 기다린다면 타임아웃
- `"%.0f"` 출력은 BSP 에서 **Small C library 를 끄고** 빌드해야 동작 (켜져 있으면 float 이 빈 문자열)

### 6-6. EP4CE10 리소스 관련

- `rtl/uart_fifo.v` 는 **동기 읽기 구조로 M9K 블록 RAM 에 합성**되도록 작성 (비동기 읽기로 쓰면
  256바이트 FIFO 하나가 LE 2,000개 이상 — EP4CE10 의 20% 이상)
- FIFO UART 1개 = M9K 2개(RX/TX 각 256×8) + 로직 수백 LE 수준으로 예상.
  2개(uart1, uart2) 교체 시 M9K 4개. 기존 DPRAM 6개와 Nios 메모리 사용량을 고려해
  Quartus Fitter 리포트의 *Total memory bits / M9K blocks* 여유를 확인하세요.
  M9K 가 부족하면 Platform Designer 에서 `FIFO_AW` 를 6(64바이트)으로 줄여도 충분합니다.
- top.v 는 이미 PLL 2개(altpll_0, altpll_1)를 모두 사용 중 — FIFO UART 는 PLL 을 쓰지 않으므로 영향 없음.
- (아직 Quartus 합성으로 실제 사용량을 확인하지는 않았습니다)
