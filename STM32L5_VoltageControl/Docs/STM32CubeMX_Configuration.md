# STM32L5 FreeRTOS Voltage Control - STM32CubeMX 상세 설정 가이드

## 1. 프로젝트 개요

- **MCU**: STM32L552ZET6Q (또는 STM32L5 시리즈)
- **IDE**: STM32CubeIDE
- **RTOS**: FreeRTOS (CMSIS_V2 API)
- **기능**:
  - PC UART 명령으로 4채널 전압 출력 (DAC + 외부 PWM→DAC)
  - MCP3465R (외부 SPI ADC)로 출력 전압 측정
  - UART 및 FDCAN으로 측정값 PC 전송
  - PI 제어 루프로 출력/측정값 일치 보정
  - 부하 전류 측정으로 단락/단선 감지

---

## 2. STM32CubeMX 설정 상세

### 2.1 System Core

#### RCC (Reset and Clock Control)
```
- HSE: Crystal/Ceramic Resonator (8 MHz 외부 크리스탈)
- LSE: Crystal/Ceramic Resonator (32.768 kHz)
- PLL Source: HSE
- PLL_M: 1
- PLL_N: 55
- PLL_R: 2
- System Clock: 110 MHz (최대 클럭)
- AHB Prescaler: 1 (HCLK = 110 MHz)
- APB1 Prescaler: 1 (PCLK1 = 110 MHz)
- APB2 Prescaler: 1 (PCLK2 = 110 MHz)
```

#### SYS
```
- Debug: Serial Wire
- Timebase Source: TIM6 (SysTick는 FreeRTOS가 사용하므로 반드시 TIM6 등 다른 타이머 사용!)
```

> **중요**: FreeRTOS 사용 시 HAL Timebase를 SysTick 이외의 타이머로 설정해야 합니다.
> SysTick는 FreeRTOS 스케줄러가 독점 사용합니다.

#### NVIC
```
- Time base: TIM6 global interrupt → Enabled, Priority 0 (최고)
- SysTick → FreeRTOS가 관리 (직접 설정 불필요)
- USART1 global interrupt → Enabled, Priority 5
- USART2 global interrupt → Enabled, Priority 5
- SPI1 global interrupt → Enabled, Priority 5
- FDCAN1 interrupt 0 → Enabled, Priority 5
- DMA interrupts → Enabled, Priority 5
- ADC1 global interrupt → Enabled, Priority 6
```

> **중요**: FreeRTOS에서 인터럽트 우선순위는 configMAX_SYSCALL_INTERRUPT_PRIORITY (기본 5)
> 이상의 숫자(=낮은 우선순위)여야 FreeRTOS API 호출이 가능합니다.

---

### 2.2 UART 설정

#### USART1 (PC 통신 - 명령 수신/데이터 송신)
```
Mode: Asynchronous
Baud Rate: 115200
Word Length: 8 Bits
Stop Bits: 1
Parity: None
Hardware Flow Control: None
Over Sampling: 16

DMA Settings:
  - USART1_RX: DMA1 Channel 1, Circular mode, Byte, Memory increment
  - USART1_TX: DMA1 Channel 2, Normal mode, Byte, Memory increment

NVIC:
  - USART1 global interrupt: Enabled
  - DMA1 Channel 1 interrupt: Enabled
  - DMA1 Channel 2 interrupt: Enabled

GPIO:
  - PA9  → USART1_TX (AF7)
  - PA10 → USART1_RX (AF7)
```

#### USART2 (디버그용 - 선택사항)
```
Mode: Asynchronous
Baud Rate: 115200
Word Length: 8 Bits
GPIO:
  - PA2 → USART2_TX (AF7)
  - PA3 → USART2_RX (AF7)
```

---

### 2.3 SPI 설정 (MCP3465R 외부 ADC)

#### SPI1
```
Mode: Full-Duplex Master
Prescaler: 64 (SPI Clock = 110MHz/64 ≈ 1.72 MHz, MCP3465R max 20MHz)
Frame Format: Motorola
Data Size: 8 Bits
First Bit: MSB First
CPOL: Low (Clock Polarity = 0)
CPHA: 1 Edge (Clock Phase = 0) → SPI Mode 0,0
NSS: Software (GPIO로 CS 제어)
CRC: Disabled

GPIO:
  - PA5 → SPI1_SCK  (AF5)
  - PA6 → SPI1_MISO (AF5)
  - PA7 → SPI1_MOSI (AF5)
  - PA4 → GPIO_Output (CS, Software NSS) → 초기값 High, Push-Pull, No Pull
```

---

### 2.4 DAC 설정 (전압 출력 채널 1, 2)

#### DAC1
```
Channel 1:
  - Output Buffer: Enabled
  - Trigger: Software Trigger
  - Connected to external pin: PA4 → 주의: SPI CS와 충돌 시 PA4 대신 다른 핀 사용
  - 실제 핀: PA4 (DAC1_OUT1) → SPI CS를 PB0로 변경

Channel 2:
  - Output Buffer: Enabled
  - Trigger: Software Trigger
  - Connected to external pin: PA5 → 주의: SPI SCK와 충돌
  - 실제로 STM32L5에서 DAC1_OUT1=PA4, DAC1_OUT2=PA5 이므로
    DAC 채널과 SPI를 다른 핀 그룹으로 분리 필요
```

#### 핀 재배치 (충돌 해결)
```
SPI1 재배치:
  - PB3 → SPI1_SCK  (AF5)
  - PB4 → SPI1_MISO (AF5)
  - PB5 → SPI1_MOSI (AF5)
  - PB0 → GPIO_Output (SPI_CS) → Push-Pull, High

DAC1:
  - PA4 → DAC1_OUT1 (채널 1 전압 출력)
  - PA5 → DAC1_OUT2 (채널 2 전압 출력)
```

---

### 2.5 TIM + PWM 설정 (전압 출력 채널 3, 4 - PWM to Analog)

> DAC가 2채널만 지원하므로, 나머지 2채널은 TIM PWM + RC 필터로 아날로그 전압 생성

#### TIM2 (채널 3 전압 출력)
```
Clock Source: Internal Clock
Channel 1: PWM Generation CH1
Prescaler: 0
Counter Period (ARR): 4095 (12-bit 해상도)
PWM Mode: PWM Mode 1
Pulse: 0 (초기값)
Output Polarity: High

GPIO:
  - PA0 → TIM2_CH1 (AF1) → 외부 RC 필터 연결 (R=10kΩ, C=1μF)
```

#### TIM3 (채널 4 전압 출력)
```
Clock Source: Internal Clock
Channel 1: PWM Generation CH1
Prescaler: 0
Counter Period (ARR): 4095 (12-bit 해상도)
PWM Mode: PWM Mode 1
Pulse: 0 (초기값)

GPIO:
  - PA6 → TIM3_CH1 (AF2) → 외부 RC 필터 연결 (R=10kΩ, C=1μF)
  → PA6이 SPI MISO와 충돌하면 PB4로 SPI 이동했으므로 PA6 사용 가능
```

---

### 2.6 내부 ADC 설정 (부하 전류 측정)

#### ADC1
```
Clock Prescaler: Asynchronous clock mode, Div 4
Resolution: 12 Bits
Data Alignment: Right
Scan Conversion Mode: Enabled
Continuous Conversion Mode: Disabled
DMA Continuous Requests: Enabled
End of Conversion: End of Sequence
Number of Conversions: 4

Channel Configuration:
  Rank 1: Channel 1  (PA0 → 전류센서1) → 주의: TIM2와 충돌
  Rank 2: Channel 2  (PA1 → 전류센서2)
  Rank 3: Channel 3  (PA2 → 전류센서3) → 주의: USART2_TX와 충돌
  Rank 4: Channel 4  (PA3 → 전류센서4) → 주의: USART2_RX와 충돌

→ 충돌 해결: 전류 측정은 다른 ADC 채널 사용
  Rank 1: Channel 10 (PC0 → 전류센서1)
  Rank 2: Channel 11 (PC1 → 전류센서2)
  Rank 3: Channel 12 (PC2 → 전류센서3)
  Rank 4: Channel 13 (PC3 → 전류센서4)

Sampling Time: 각 채널 47.5 Cycles (안정적 측정)

DMA:
  - ADC1: DMA1 Channel 3, Circular mode, Half Word, Memory Increment

NVIC:
  - ADC1 interrupt: Enabled
  - DMA channel interrupt: Enabled
```

---

### 2.7 FDCAN 설정

#### FDCAN1
```
Frame Format: Classic CAN (또는 FD CAN)
Mode: Normal Mode
Auto Retransmission: Enabled
Transmit Pause: Disabled
Protocol Exception Handling: Enabled

Bit Timing (500 kbps):
  Nominal Prescaler: 11
  Nominal Time Seg1: 14
  Nominal Time Seg2: 5
  Nominal Sync Jump Width: 4
  → Nominal Bit Rate = 110MHz / (11 * (1 + 14 + 5)) = 500 kbps

Data Bit Timing (FD mode, 2 Mbps):
  Data Prescaler: 11
  Data Time Seg1: 3
  Data Time Seg2: 1
  Data Sync Jump Width: 1

Std Filters Nbr: 1
Rx FIFO0 Elmts Nbr: 3
Tx FIFO Queue Elmts Nbr: 3

GPIO:
  - PB8 → FDCAN1_RX (AF9)
  - PB9 → FDCAN1_TX (AF9)

NVIC:
  - FDCAN1 interrupt 0: Enabled, Priority 5
```

---

### 2.8 FreeRTOS 상세 설정 ★★★

#### Middleware → FreeRTOS → Interface: CMSIS_V2

#### Config Parameters (핵심 설정)
```
=== Kernel Settings ===
USE_PREEMPTION:                Enabled        (선점형 스케줄링)
CPU_CLOCK_HZ:                  110000000      (110 MHz)
TICK_RATE_HZ:                  1000           (1ms 틱)
MAX_PRIORITIES:                7              (우선순위 레벨 수)
MINIMAL_STACK_SIZE:            256            (최소 스택 512 bytes)
MAX_TASK_NAME_LEN:             16
USE_16_BIT_TICKS:              Disabled       (32-bit 틱 카운터)
IDLE_SHOULD_YIELD:             Enabled
USE_TASK_NOTIFICATIONS:        Enabled        (태스크 알림 사용)
USE_MUTEXES:                   Enabled        (뮤텍스 사용)
USE_RECURSIVE_MUTEXES:         Enabled
USE_COUNTING_SEMAPHORES:       Enabled        (카운팅 세마포어)
QUEUE_REGISTRY_SIZE:           8
USE_QUEUE_SETS:                Disabled
USE_TIME_SLICING:              Enabled        (동일 우선순위 시 타임슬라이싱)
USE_NEWLIB_REENTRANT:          Disabled
ENABLE_BACKWARD_COMPATIBILITY: Disabled
NUM_THREAD_LOCAL_STORAGE_POINTERS: 0
USE_MINI_LIST_ITEM:            Enabled

=== Memory Management ===
Memory Allocation:             Dynamic         (동적 메모리 할당)
TOTAL_HEAP_SIZE:               32768          (32 KB - STM32L5 SRAM 256KB 중)
Memory Management scheme:      heap_4          (가장 범용적, 단편화 방지)
USE_APPLICATION_TASK_TAG:      Disabled

=== Hook Functions ===
USE_IDLE_HOOK:                 Disabled
USE_TICK_HOOK:                 Disabled
USE_MALLOC_FAILED_HOOK:        Enabled        (메모리 할당 실패 감지!)
CHECK_FOR_STACK_OVERFLOW:      Option 2       (스택 오버플로우 감지 - 필수!)
USE_DAEMON_TASK_STARTUP_HOOK:  Disabled

=== Run Time and Task Stats ===
GENERATE_RUN_TIME_STATS:       Disabled       (필요 시 Enabled)
USE_TRACE_FACILITY:            Enabled        (디버깅용)
USE_STATS_FORMATTING_FUNCTIONS: Enabled

=== Co-routine Settings ===
USE_CO_ROUTINES:               Disabled

=== Software Timer Settings ===
USE_TIMERS:                    Enabled        (소프트웨어 타이머 사용)
TIMER_TASK_PRIORITY:           5              (타이머 태스크 높은 우선순위)
TIMER_QUEUE_LENGTH:            10
TIMER_TASK_STACK_DEPTH:        256

=== Interrupt Nesting ===
LIBRARY_LOWEST_INTERRUPT_PRIORITY:        15
LIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY:   5
```

#### Tasks (태스크 정의)
```
Task Name          | Priority      | Stack Size | Entry Function
-------------------|---------------|------------|-------------------
UartCmdTask        | osPriorityHigh      (5) | 512 words  | StartUartCmdTask
VoltageCtrlTask    | osPriorityAboveNormal(4)| 512 words  | StartVoltageCtrlTask
AdcReadTask        | osPriorityNormal    (3) | 512 words  | StartAdcReadTask
FaultDetectTask    | osPriorityNormal    (3) | 384 words  | StartFaultDetectTask
CommTxTask         | osPriorityBelowNormal(2)| 512 words  | StartCommTxTask
```

> **태스크 우선순위 설계 근거**:
> - UartCmdTask (5): 사용자 명령 즉시 수신 처리 (가장 높은 우선순위)
> - VoltageCtrlTask (4): PI 제어 루프 - 실시간성 중요
> - AdcReadTask (3): 주기적 ADC 읽기 - 안정적 주기 필요
> - FaultDetectTask (3): 안전 관련이지만 ADC 데이터 의존
> - CommTxTask (2): 데이터 전송은 약간의 지연 허용

#### Queues (큐 정의)
```
Queue Name         | Queue Size | Item Size        | 용도
-------------------|------------|------------------|-------------------------
UartCmdQueue       | 10         | sizeof(UartCmd_t) | UART→명령처리 태스크
AdcDataQueue       | 10         | sizeof(AdcData_t) | ADC→제어/전송 태스크
CommTxQueue        | 20         | sizeof(CommMsg_t) | 제어→UART/CAN 전송
FaultEventQueue    | 10         | sizeof(FaultEvt_t)| 결함 이벤트 전달
```

#### Mutexes (뮤텍스 정의)
```
Mutex Name         | Type       | 용도
-------------------|------------|----------------------------------
SpiMutex           | Normal     | SPI 버스 공유 보호 (MCP3465R 접근)
DacMutex           | Normal     | DAC 레지스터 동시 접근 보호
UartTxMutex        | Normal     | UART TX 동시 전송 보호
```

#### Semaphores (세마포어 정의)
```
Semaphore Name     | Type       | 용도
-------------------|------------|----------------------------------
UartRxSem          | Binary     | UART 수신 완료 알림
AdcConvSem         | Binary     | ADC 변환 완료 알림
CanTxSem           | Binary     | CAN 전송 완료 알림
```

#### Software Timers
```
Timer Name         | Period     | Type       | 용도
-------------------|------------|------------|-------------------------
CtrlLoopTimer      | 10 ms      | Periodic   | PI 제어 루프 주기 트리거
HeartbeatTimer     | 1000 ms    | Periodic   | 시스템 상태 LED 점멸
```

---

### 2.9 GPIO 최종 핀 배치 요약

```
핀     | 기능              | 설정
-------|-------------------|----------------------------------
PA4    | DAC1_OUT1         | Analog (채널1 전압출력)
PA5    | DAC1_OUT2         | Analog (채널2 전압출력)
PA0    | TIM2_CH1 (PWM)    | AF1 (채널3 전압출력→RC필터)
PA6    | TIM3_CH1 (PWM)    | AF2 (채널4 전압출력→RC필터)
PA9    | USART1_TX         | AF7
PA10   | USART1_RX         | AF7
PB3    | SPI1_SCK          | AF5
PB4    | SPI1_MISO         | AF5
PB5    | SPI1_MOSI         | AF5
PB0    | SPI1_CS (GPIO)    | Output Push-Pull, High
PB8    | FDCAN1_RX         | AF9
PB9    | FDCAN1_TX         | AF9
PC0    | ADC1_CH10         | Analog (전류센서1)
PC1    | ADC1_CH11         | Analog (전류센서2)
PC2    | ADC1_CH12         | Analog (전류센서3)
PC3    | ADC1_CH13         | Analog (전류센서4)
PC13   | LED_STATUS (GPIO) | Output Push-Pull
PB1    | LED_FAULT (GPIO)  | Output Push-Pull
```

---

### 2.10 Clock Configuration 요약

```
         ┌─────────┐
HSE 8MHz─┤  PLL    ├─→ PLLCLK = 110 MHz
         │ M=1     │
         │ N=55    │   → SYSCLK = 110 MHz
         │ R=2     │   → HCLK   = 110 MHz
         └─────────┘   → APB1   = 110 MHz
                        → APB2   = 110 MHz
                        → ADC    = 110/4 = 27.5 MHz
                        → SPI1   = 110/64 ≈ 1.72 MHz
                        → FDCAN  = 110 MHz (kernel clock)
```

---

### 2.11 Project Manager 설정

```
Project Name: STM32L5_VoltageControl
Project Location: (원하는 경로)
Toolchain / IDE: STM32CubeIDE
Firmware Package: STM32Cube_FW_L5 (최신)

Code Generator:
  ✅ Generate peripheral initialization as a pair of '.c/.h' files per peripheral
  ✅ Keep User Code when re-generating
  ✅ Set all free pins as analog (to reduce power consumption)
```

---

## 3. 하드웨어 연결 참고

### MCP3465R 외부 ADC 연결
```
MCP3465R Pin → STM32L5 Pin
SCK          → PB3 (SPI1_SCK)
SDI (MOSI)   → PB5 (SPI1_MOSI)
SDO (MISO)   → PB4 (SPI1_MISO)
CS           → PB0 (GPIO Output)
VDD          → 3.3V
AVDD         → 3.3V (또는 별도 기준전압)
VSS          → GND

MCP3465R ADC 입력:
CH0 → 채널1 전압 출력 피드백
CH1 → 채널2 전압 출력 피드백
CH2 → 채널3 전압 출력 피드백
CH3 → 채널4 전압 출력 피드백
```

### 전류 센서 연결 (예: INA180 전류 감지 앰프)
```
각 채널의 부하 전류를 션트 저항(0.1Ω)으로 측정
INA180 출력 → PC0~PC3 (ADC1 CH10~CH13)
```

### FDCAN 트랜시버
```
STM32 PB9 (FDCAN1_TX) → CAN Transceiver TXD (예: SN65HVD230)
STM32 PB8 (FDCAN1_RX) → CAN Transceiver RXD
```
