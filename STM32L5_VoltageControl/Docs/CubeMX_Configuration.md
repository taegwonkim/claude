# STM32L5 FreeRTOS Voltage Control - STM32CubeMX 설정 가이드

## 1. 타겟 MCU 선택
- MCU: **STM32L552ZETxQ** (또는 STM32L552CETx)
- 패키지: LQFP144

---

## 2. 클럭 설정 (Clock Configuration)

### RCC 설정
```
HSE: Disabled (내부 클럭 사용)
HSI16: Enabled
PLL Source: HSI16
PLLM: 1
PLLN: 10
PLLP: 7
PLLQ: 2
PLLR: 2  → SYSCLK = 80 MHz
```

### Clock Tree
```
SYSCLK  = 80 MHz (PLL)
HCLK    = 80 MHz (AHB Prescaler = 1)
APB1    = 80 MHz (APB1 Prescaler = 1)
APB2    = 80 MHz (APB2 Prescaler = 1)
```

---

## 3. 핀 설정 (Pinout)

### UART (PC 통신)
| 핀  | 기능      | 설명         |
|-----|-----------|--------------|
| PA9 | USART1_TX | PC → STM32   |
| PA10| USART1_RX | STM32 → PC   |

### SPI (MCP3465R ADC)
| 핀  | 기능      | 설명                  |
|-----|-----------|----------------------|
| PA5 | SPI1_SCK  | SPI 클럭              |
| PA6 | SPI1_MISO | MISO                  |
| PA7 | SPI1_MOSI | MOSI                  |
| PB6 | GPIO_OUT  | MCP3465R CS (액티브 로우) |
| PB7 | GPIO_IN   | MCP3465R IRQ (데이터 준비)|

### PWM 출력 (4채널 전압 출력 → RC필터로 아날로그 변환)
| 핀  | 기능       | 채널 | 설명                     |
|-----|-----------|------|--------------------------|
| PA0 | TIM2_CH1  | CH1  | 0~3.3V (RC 필터 후)       |
| PA1 | TIM2_CH2  | CH2  | 0~3.3V (RC 필터 후)       |
| PA2 | TIM2_CH3  | CH3  | 0~3.3V (RC 필터 후)       |
| PA3 | TIM2_CH4  | CH4  | 0~3.3V (RC 필터 후)       |

### 상태 LED
| 핀  | 기능      | 설명           |
|-----|-----------|----------------|
| PC7 | GPIO_OUT  | 시스템 상태 LED |

---

## 4. 주변장치 상세 설정

### 4.1 USART1 설정
```
Mode: Asynchronous
Baud Rate: 115200 Bits/s
Word Length: 8 Bits
Parity: None
Stop Bits: 1
Data Direction: Receive and Transmit
Over Sampling: 16 Samples
DMA:
  - USART1_RX: DMA1 Channel 1, Circular, Normal
  - USART1_TX: DMA1 Channel 2, Normal
NVIC:
  - USART1 global interrupt: Enabled (Priority 5)
  - DMA1 Channel1 global: Enabled (Priority 5)
  - DMA1 Channel2 global: Enabled (Priority 5)
```

### 4.2 SPI1 설정 (MCP3465R)
```
Mode: Full-Duplex Master
Hardware NSS: Disabled (Software NSS로 GPIO 직접 제어)
Baud Rate: 10 MHz (APB2/8 = 10MHz, MCP3465R max 85MHz이므로 여유)
Clock Polarity (CPOL): Low
Clock Phase (CPHA): 1 Edge  → Mode 0,0
Data Size: 8 Bits
First Bit: MSB First
NSSP Mode: Disabled
NVIC:
  - SPI1 global interrupt: Enabled (Priority 5)
```

### 4.3 TIM2 설정 (PWM 4채널)
```
Clock Source: Internal Clock
Channel1: PWM Generation CH1
Channel2: PWM Generation CH2
Channel3: PWM Generation CH3
Channel4: PWM Generation CH4

Counter Settings:
  Prescaler: 0 (80MHz / (0+1) = 80MHz)
  Counter Mode: Up
  Counter Period (ARR): 4095  → PWM Frequency = 80MHz/4096 ≈ 19.5kHz
  Auto-Reload Preload: Enable

PWM Generation:
  Mode: PWM mode 1
  Pulse (CCR1~4): 0 (초기값)
  Output compare preload: Enable
  Fast Mode: Disable

NVIC: TIM2 global interrupt: Disabled (필요시 Enable)
```

### 4.4 ADC1 설정 (내부 온도/기준전압 모니터링용, 선택사항)
```
Mode:
  IN0 (PA0): 미사용 (PWM 핀과 중복 불가)
  Temperature Sensor Channel: Enable
  VREFINT Channel: Enable
Resolution: 12 bits
External Trigger: Software trigger
```

---

## 5. FreeRTOS 설정 (가장 중요!)

### 5.1 Middleware → FreeRTOS 활성화
```
Interface: CMSIS_V2
```

### 5.2 Config Parameters 탭

#### 메모리 설정
```
configTOTAL_HEAP_SIZE: 20480  (20KB - L5는 256KB SRAM 보유)
configMINIMAL_STACK_SIZE: 128  (128 words = 512 bytes)
```

#### 태스크 스케줄링
```
configUSE_PREEMPTION: 1          (선점형 스케줄링 사용)
configUSE_TIME_SLICING: 1        (동일 우선순위 시간 분할)
configUSE_TICKLESS_IDLE: 0       (저전력 불필요시 0, 필요시 1)
TICK_RATE_HZ: 1000               (1ms tick)
configMAX_PRIORITIES: 7          (0~6, 숫자 클수록 높은 우선순위)
```

#### 동기화 객체
```
configUSE_MUTEXES: 1             (Mutex 사용)
configUSE_RECURSIVE_MUTEXES: 1  (재귀 Mutex)
configUSE_COUNTING_SEMAPHORES: 1 (카운팅 세마포어)
configUSE_QUEUE_SETS: 0
configQUEUE_REGISTRY_SIZE: 10   (디버깅용)
```

#### 타이머
```
configUSE_TIMERS: 1              (소프트웨어 타이머)
configTIMER_TASK_PRIORITY: 2     (타이머 태스크 우선순위)
configTIMER_QUEUE_LENGTH: 10
configTIMER_TASK_STACK_DEPTH: 256
```

#### 디버깅/통계
```
configUSE_TRACE_FACILITY: 1      (태스크 상태 조회)
configUSE_STATS_FORMATTING_FUNCTIONS: 1
configGENERATE_RUN_TIME_STATS: 0
configCHECK_FOR_STACK_OVERFLOW: 2  (스택 오버플로우 감지!)
configUSE_MALLOC_FAILED_HOOK: 1    (메모리 할당 실패 콜백)
configUSE_IDLE_HOOK: 0
configUSE_TICK_HOOK: 0
```

#### 인터럽트 우선순위 (L5 Cortex-M33 중요!)
```
configLIBRARY_LOWEST_INTERRUPT_PRIORITY: 15
configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY: 5
  → 우선순위 0~4: FreeRTOS API 사용 불가 (하드웨어 인터럽트 전용)
  → 우선순위 5~15: FreeRTOS API 사용 가능 (FromISR 함수)
```

### 5.3 Tasks and Queues 탭 설정

#### Tasks (태스크 생성)

| Task Name           | Priority | Stack Size | Entry Function        | 설명                    |
|--------------------|----------|------------|----------------------|-------------------------|
| UartRxTask          | osPriorityNormal (3)    | 512  | vUartRxTask          | UART 수신/명령 파싱       |
| UartTxTask          | osPriorityBelowNormal(2)| 512  | vUartTxTask          | UART 송신                |
| AdcReadTask         | osPriorityAboveNormal(4)| 512  | vAdcReadTask         | MCP3465R 읽기            |
| VoltageControlTask  | osPriorityHigh (5)      | 512  | vVoltageControlTask  | PWM 피드백 제어           |
| CurrentMonitorTask  | osPriorityAboveNormal(4)| 256  | vCurrentMonitorTask  | 단락/단선 감지            |

#### Queues (큐 생성)

| Queue Name       | Queue Length | Item Size | 설명                      |
|-----------------|-------------|-----------|---------------------------|
| xCmdQueue        | 10           | 32 bytes  | PC→펌웨어 명령 큐          |
| xRespQueue       | 10           | 64 bytes  | 펌웨어→PC 응답 큐           |
| xAdcResultQueue  | 8            | 32 bytes  | ADC 측정결과 큐             |

#### Mutexes (뮤텍스 생성)

| Mutex Name    | 설명                        |
|--------------|-----------------------------|
| xSpiMutex    | SPI 버스 접근 보호            |
| xUartTxMutex | UART TX 접근 보호             |

#### Semaphores (이진 세마포어)

| Semaphore Name  | 설명                             |
|----------------|----------------------------------|
| xAdcDataReady   | ADC 변환완료 알림 (IRQ→Task)     |
| xUartRxReady    | UART DMA 수신완료 알림           |

### 5.4 Include Parameters 탭
```
vTaskDelay: Enabled
vTaskDelayUntil: Enabled  (주기적 태스크에 필수!)
uxTaskGetStackHighWaterMark: Enabled  (스택 사용량 모니터링)
xTaskGetSchedulerState: Enabled
vTaskList: Enabled  (태스크 목록 출력)
```

---

## 6. NVIC 우선순위 설정

| 인터럽트             | Priority | Sub-Priority | FreeRTOS API |
|--------------------|----------|-------------|--------------|
| SysTick (FreeRTOS) | 15       | 0           | 내부 사용     |
| USART1 global      | 6        | 0           | 사용 가능     |
| DMA1 Channel1      | 6        | 0           | 사용 가능     |
| DMA1 Channel2      | 6        | 0           | 사용 가능     |
| SPI1 global        | 6        | 0           | 사용 가능     |
| EXTI9_5 (IRQ핀)    | 5        | 0           | 사용 가능     |
| TIM2 global        | 7        | 0           | 사용 가능     |

> **중요**: STM32L5 Cortex-M33은 4비트 우선순위 → 0(최고)~15(최저)
> FreeRTOS configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
> → 우선순위 0~4 ISR에서는 절대 FreeRTOS API 호출 금지!

---

## 7. 프로젝트 설정

### Project Manager 탭
```
Project Name: STM32L5_VoltageControl
Project Location: (원하는 경로)
Toolchain/IDE: STM32CubeIDE
Heap Size: 0x4000  (16KB, FreeRTOS가 별도 관리하므로 최소)
Stack Size: 0x1000  (4KB)
```

### Code Generator 탭
```
[√] Generate peripheral initialization as a pair of .c/.h files per peripheral
[√] Keep User Code when re-generating
[√] Delete previously generated files when not re-generated
```

---

## 8. 하드웨어 회로 메모

### RC 저역통과 필터 (PWM → DC 전압)
```
PWM 주파수: ~19.5kHz
목표 차단 주파수: ~50Hz (리플 충분히 제거)
R = 10kΩ, C = 330nF → fc = 1/(2π×10k×330n) ≈ 48Hz
```

### MCP3465R 연결
```
VDD: 3.3V
VREF: 3.3V (외부 정밀 레퍼런스 권장: REF3033)
AGND/DGND: GND
SDI: SPI1_MOSI (PA7)
SDO: SPI1_MISO (PA6)
SCK: SPI1_SCK  (PA5)
CS:  PB6 (GPIO, 액티브 로우)
IRQ: PB7 (GPIO 입력, EXTI, 액티브 로우)

채널 배선:
  CH0/CH1 (차동): 채널1 전압 측정  (RC필터 출력)
  CH2/CH3 (차동): 채널2 전압 측정
  CH4/CH5 (차동): 채널3 전압 측정
  CH6/CH7 (차동): 채널4 전압 측정
  (단일 종단 사용 시 홀수 채널을 AGND에 연결)
```

### 전류 측정 (단락/단선 감지)
```
각 채널 출력에 0.1Ω 전류감지 저항 직렬 삽입
INA199 (전류감지 앰프, 게인 50V/V) 사용
→ 최대 1A → INA199 출력 = 1A × 0.1Ω × 50 = 5V
→ MCP3465R 입력 범위 초과! 분압 필요 (1/2 분압 → 2.5V)
또는 낮은 게인 버전 사용 (INA199A1, 게인 20V/V)
→ 1A × 0.1Ω × 20 = 0.2V (측정 가능, 분해능 좋음)
```
