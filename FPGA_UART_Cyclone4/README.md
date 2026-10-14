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
