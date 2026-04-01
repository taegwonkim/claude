
# STM32CubeMX Configuration Guide
## STM32L552RET6 - Voltage Feedback Current Monitor

---

## 1. Project Settings

| 항목 | 설정값 |
|------|--------|
| MCU | STM32L552RET6 (LQFP64) |
| Project Name | VoltageMonitor |
| Toolchain/IDE | STM32CubeIDE |
| TrustZone | **Disabled** (Project Manager → Code Generator) |
| Min Heap Size | 0x800 |
| Min Stack Size | 0x1000 |

---

## 2. RCC (Clock Configuration)

### 2.1 RCC Mode
| 항목 | 설정값 |
|------|--------|
| HSE | Crystal/Ceramic Resonator (8MHz 외부 크리스탈) |
| LSE | Disable (미사용) |

### 2.2 Clock Configuration (Clock Configuration 탭)

```
HSE = 8 MHz
  → PLL Source Mux: HSE
  → PLLM = /1 (8 MHz)
  → PLLN = ×25 (200 MHz VCO)
  → PLLR = /2 (100 MHz)
  → System Clock Mux: PLLCLK
  → SYSCLK = 100 MHz
  → AHB Prescaler = /1 → HCLK = 100 MHz
  → APB1 Prescaler = /1 → PCLK1 = 100 MHz
  → APB2 Prescaler = /1 → PCLK2 = 100 MHz
```

### 2.3 CubeMX Clock Configuration 탭 상세 입력값

| 항목 | 값 |
|------|------|
| Input frequency (HSE) | 8 MHz |
| PLL Source | HSE |
| PLLM | /1 |
| PLLN | x25 |
| PLLR | /2 |
| System Clock Mux | PLLCLK |
| SYSCLK | 100 MHz |
| AHB Prescaler | /1 |
| APB1 Prescaler | /1 |
| APB2 Prescaler | /1 |
| FDCAN Clock Source | PCLK1 (100 MHz) |

---

## 3. GPIO Configuration

### 3.1 SPI1 CS Pins (ADC MCP3465R × 4채널)

| 핀 | Label | 설정 |
|----|-------|------|
| PC0 | ADC_CS1 | GPIO_Output, Push-Pull, No Pull, High (초기 High) |
| PC1 | ADC_CS2 | GPIO_Output, Push-Pull, No Pull, High |
| PC2 | ADC_CS3 | GPIO_Output, Push-Pull, No Pull, High |
| PC3 | ADC_CS4 | GPIO_Output, Push-Pull, No Pull, High |

### 3.2 SPI2 CS Pins (DAC AD5641 × 4채널)

| 핀 | Label | 설정 |
|----|-------|------|
| PC4 | DAC_CS1 | GPIO_Output, Push-Pull, No Pull, High (초기 High) |
| PC5 | DAC_CS2 | GPIO_Output, Push-Pull, No Pull, High |
| PC6 | DAC_CS3 | GPIO_Output, Push-Pull, No Pull, High |
| PC7 | DAC_CS4 | GPIO_Output, Push-Pull, No Pull, High |

### 3.3 상태 표시 LED (선택사항)

| 핀 | Label | 설정 |
|----|-------|------|
| PB4 | LED_STATUS | GPIO_Output, Push-Pull, No Pull, Low |
| PB5 | LED_ERROR | GPIO_Output, Push-Pull, No Pull, Low |

---

## 4. SPI1 Configuration (ADC - MCP3465R)

Connectivity → SPI1 → Mode: **Full-Duplex Master**

### Parameter Settings

| 항목 | 설정값 | 비고 |
|------|--------|------|
| Frame Format | Motorola | |
| Data Size | 8 Bits | |
| First Bit | MSB First | MCP3465R 사양 |
| Prescaler | 16 | PCLK2/16 = 6.25 MHz (max 20MHz) |
| Clock Polarity (CPOL) | Low | SPI Mode 0 |
| Clock Phase (CPHA) | 1 Edge | SPI Mode 0 |
| CRC Calculation | Disabled | |
| NSS Signal | Software | CS는 GPIO로 제어 |

### Pin Assignment

| 기능 | 핀 |
|------|-----|
| SPI1_SCK | PA5 |
| SPI1_MISO | PA6 |
| SPI1_MOSI | PA7 |

### DMA Settings (선택사항 - 폴링 사용 시 불필요)

| DMA Request | Channel | Direction | Priority |
|-------------|---------|-----------|----------|
| SPI1_RX | DMA1 Channel 1 | Peripheral to Memory | Medium |
| SPI1_TX | DMA1 Channel 2 | Memory to Peripheral | Medium |

---

## 5. SPI2 Configuration (DAC - AD5641)

Connectivity → SPI2 → Mode: **Transmit Only Master**

### Parameter Settings

| 항목 | 설정값 | 비고 |
|------|--------|------|
| Frame Format | Motorola | |
| Data Size | 8 Bits | |
| First Bit | MSB First | AD5641 사양 |
| Prescaler | 8 | PCLK1/8 = 12.5 MHz (max 30MHz) |
| Clock Polarity (CPOL) | High | SPI Mode 3 (AD5641) |
| Clock Phase (CPHA) | 2 Edge | SPI Mode 3 |
| CRC Calculation | Disabled | |
| NSS Signal | Software | CS는 GPIO로 제어 |

### Pin Assignment

| 기능 | 핀 |
|------|-----|
| SPI2_SCK | PB13 |
| SPI2_MISO | (없음 - Transmit Only) |
| SPI2_MOSI | PB15 |

---

## 6. USART1 Configuration (PC 통신)

Connectivity → USART1 → Mode: **Asynchronous**

### Parameter Settings

| 항목 | 설정값 |
|------|--------|
| Baud Rate | 115200 |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |
| Over Sampling | 16 |
| Hardware Flow Control | None |

### Pin Assignment

| 기능 | 핀 |
|------|-----|
| USART1_TX | PA9 |
| USART1_RX | PA10 |

### NVIC Settings

| Interrupt | Enabled | Priority |
|-----------|---------|----------|
| USART1 global interrupt | **Yes** | 6 (FreeRTOS 이상) |

### DMA Settings

| DMA Request | Channel | Direction | Mode | Priority |
|-------------|---------|-----------|------|----------|
| USART1_RX | DMA1 Ch3 | Peripheral to Memory | Circular | High |
| USART1_TX | DMA1 Ch4 | Memory to Peripheral | Normal | Medium |

---

## 7. UART4 Configuration (디버그 포트)

Connectivity → UART4 → Mode: **Asynchronous**

### Parameter Settings

| 항목 | 설정값 |
|------|--------|
| Baud Rate | 115200 |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |

### Pin Assignment

| 기능 | 핀 |
|------|-----|
| UART4_TX | PA0 |
| UART4_RX | PA1 |

### NVIC Settings

| Interrupt | Enabled | Priority |
|-----------|---------|----------|
| UART4 global interrupt | **Yes** | 7 |

---

## 8. FDCAN1 Configuration

Connectivity → FDCAN1 → Mode: **Activated**

### Parameter Settings

| 항목 | 설정값 | 비고 |
|------|--------|------|
| Frame Format | Classic | Classic CAN 2.0 사용 |
| Mode | Normal | |
| Auto Retransmission | Enable | |
| Transmit Pause | Enable | |
| Nominal Prescaler | 10 | 100MHz / 10 = 10 MHz |
| Nominal Time Seg1 | 7 | |
| Nominal Time Seg2 | 2 | |
| Nominal Sync Jump Width | 1 | |
| **Nominal Bit Rate** | **1 Mbps** | 10MHz / (7+2+1) = 1Mbps |
| Std Filters Nbr | 1 | |
| Rx FIFO0 Elmts Nbr | 3 | |
| Tx Fifo Queue Elmts Nbr | 3 | |

### Pin Assignment

| 기능 | 핀 |
|------|-----|
| FDCAN1_RX | PB8 |
| FDCAN1_TX | PB9 |

### NVIC Settings

| Interrupt | Enabled | Priority |
|-----------|---------|----------|
| FDCAN1 interrupt 0 | **Yes** | 6 |

> **주의**: 외부 CAN 트랜시버(예: SN65HVD230) 필요

---

## 9. FreeRTOS Configuration (★ 핵심 설정)

Middleware → FREERTOS → Interface: **CMSIS_V2**

### 9.1 FreeRTOS Kernel Settings

| 항목 | 설정값 | 비고 |
|------|--------|------|
| USE_PREEMPTION | Enabled | 선점형 스케줄링 |
| TICK_RATE_HZ | 1000 | 1ms tick |
| MAX_PRIORITIES | 7 | 0(최저)~6(최고) |
| MINIMAL_STACK_SIZE | 128 | Words (512 bytes) |
| MAX_TASK_NAME_LEN | 16 | |
| IDLE_SHOULD_YIELD | Enabled | |
| USE_MUTEXES | Enabled | SPI 버스 공유용 |
| USE_RECURSIVE_MUTEXES | Disabled | |
| USE_COUNTING_SEMAPHORES | Enabled | |
| QUEUE_REGISTRY_SIZE | 8 | |
| USE_TASK_NOTIFICATIONS | Enabled | |
| RECORD_STACK_HIGH_ADDRESS | Enabled | 디버깅용 |

### 9.2 Memory Management

| 항목 | 설정값 | 비고 |
|------|--------|------|
| Memory Allocation | Dynamic | |
| TOTAL_HEAP_SIZE | 32768 | 32 KB (충분한 여유) |
| Memory Management Scheme | heap_4 | 단편화 방지 |
| USE_HEAP_PROTECTOR | Enabled | 힙 오버플로 감지 |

### 9.3 Hook Functions

| 항목 | 설정값 | 비고 |
|------|--------|------|
| USE_IDLE_HOOK | Disabled | |
| USE_TICK_HOOK | Disabled | |
| USE_MALLOC_FAILED_HOOK | Enabled | 메모리 할당 실패 감지 |
| CHECK_FOR_STACK_OVERFLOW | Option 2 | 스택 오버플로 감지 |

### 9.4 Timer (Software Timer)

| 항목 | 설정값 | 비고 |
|------|--------|------|
| USE_TIMERS | Enabled | 주기적 리포트용 |
| TIMER_TASK_PRIORITY | 5 | 높은 우선순위 |
| TIMER_TASK_STACK_DEPTH | 256 | Words |
| TIMER_QUEUE_LENGTH | 10 | |

### 9.5 Timebase Source

> **중요**: SysTick 대신 TIM6를 HAL Timebase로 사용

| 항목 | 설정값 |
|------|--------|
| SYS → Timebase Source | **TIM6** |
| FreeRTOS → Timebase Source | SysTick (기본) |

> CubeMX에서 SYS → Timebase Source를 TIM6로 변경해야 FreeRTOS와 HAL이 충돌하지 않음

### 9.6 Tasks (CubeMX에서 생성)

| Task Name | Priority | Stack Size (Words) | Entry Function | 설명 |
|-----------|----------|-------------------|----------------|------|
| UartRxTask | osPriorityAboveNormal (5) | 512 | StartUartRxTask | UART1 명령 수신/파싱 |
| VoltCtrlTask | osPriorityHigh (6) | 512 | StartVoltCtrlTask | 전압 피드백 제어 (PI) |
| CurrMonTask | osPriorityNormal (4) | 384 | StartCurrMonTask | 전류 측정/단락·단선 감지 |
| ReportTask | osPriorityBelowNormal (3) | 512 | StartReportTask | UART1 + FDCAN1 데이터 전송 |
| DebugTask | osPriorityLow (2) | 256 | StartDebugTask | UART4 디버그 출력 |

### 9.7 Queues (CubeMX에서 생성)

| Queue Name | Queue Size | Item Size | 설명 |
|------------|-----------|-----------|------|
| CmdQueue | 8 | sizeof(VoltageCmd_t)=8 | UART→VoltCtrl 명령 전달 |
| ReportQueue | 4 | sizeof(ReportData_t)=48 | 측정 데이터→Report 전달 |
| DebugQueue | 8 | sizeof(DebugMsg_t)=64 | 디버그 메시지 전달 |

### 9.8 Mutexes (CubeMX에서 생성)

| Mutex Name | 설명 |
|------------|------|
| SPI1_Mutex | SPI1(ADC) 버스 접근 보호 |
| SPI2_Mutex | SPI2(DAC) 버스 접근 보호 |
| UART1_Mutex | UART1 TX 접근 보호 |

### 9.9 Semaphores

| Semaphore Name | Type | 설명 |
|----------------|------|------|
| UART1_RxSem | Binary | UART1 수신 완료 알림 |

### 9.10 NVIC Priority 설정 (★ FreeRTOS 호환)

> **중요**: FreeRTOS에서 API를 호출하는 ISR의 우선순위는
> `configMAX_SYSCALL_INTERRUPT_PRIORITY` (기본 5) 이상이어야 함

| 항목 | 설정값 |
|------|--------|
| configLIBRARY_LOWEST_INTERRUPT_PRIORITY | 15 |
| configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY | 5 |

| Interrupt | Preemption Priority |
|-----------|---------------------|
| TIM6 (HAL Timebase) | 0 (최고) |
| SysTick (FreeRTOS) | 15 (최저) |
| USART1 | 6 |
| UART4 | 7 |
| FDCAN1_IT0 | 6 |
| DMA1 Channel 1~4 | 6 |

---

## 10. Project Manager 설정

### Code Generator

| 항목 | 설정값 |
|------|--------|
| Generate peripheral init as pair of .c/.h | **Yes** |
| Keep User Code when re-generating | **Yes** |
| Delete previously generated files | **Yes** |

---

## 11. 전체 핀 배치 요약 (STM32L552RET6 LQFP64)

```
PA0  → UART4_TX          PA8  → (Reserved)
PA1  → UART4_RX          PA9  → USART1_TX
PA5  → SPI1_SCK          PA10 → USART1_RX
PA6  → SPI1_MISO         PA15 → (Reserved)
PA7  → SPI1_MOSI

PB4  → LED_STATUS        PB8  → FDCAN1_RX
PB5  → LED_ERROR         PB9  → FDCAN1_TX
PB13 → SPI2_SCK          PB14 → (NC - TX Only Mode)
PB15 → SPI2_MOSI

PC0  → ADC_CS1 (GPIO)    PC4  → DAC_CS1 (GPIO)
PC1  → ADC_CS2 (GPIO)    PC5  → DAC_CS2 (GPIO)
PC2  → ADC_CS3 (GPIO)    PC6  → DAC_CS3 (GPIO)
PC3  → ADC_CS4 (GPIO)    PC7  → DAC_CS4 (GPIO)
```

---

## 12. 하드웨어 연결도 요약

```
                    STM32L552R
                   ┌──────────┐
    PC(UART) ──────┤ USART1   │
    PC(Debug)──────┤ UART4    │
    CAN Bus  ──────┤ FDCAN1   │──── CAN Transceiver
                   │          │
         ┌─────── ┤ SPI1     │ ──── MCP3465R #1 (CS=PC0) ←── Vout1/Iout1
         │ ┌───── ┤          │ ──── MCP3465R #2 (CS=PC1) ←── Vout2/Iout2
         │ │ ┌─── ┤          │ ──── MCP3465R #3 (CS=PC2) ←── Vout3/Iout3
         │ │ │ ┌─ ┤          │ ──── MCP3465R #4 (CS=PC3) ←── Vout4/Iout4
         │ │ │ │  │          │
         │ │ │ │  ┤ SPI2     │ ──── AD5641 #1 (CS=PC4) ──→ Vout1
         │ │ │ │  ┤          │ ──── AD5641 #2 (CS=PC5) ──→ Vout2
         │ │ │ │  ┤          │ ──── AD5641 #3 (CS=PC6) ──→ Vout3
         │ │ │ │  ┤          │ ──── AD5641 #4 (CS=PC7) ──→ Vout4
                   └──────────┘

    각 채널 회로:
    AD5641(DAC) ──→ [Op-Amp Buffer] ──→ 부하
                                     │
                    MCP3465R CH0 ←────┘ (전압 피드백)
                    MCP3465R CH1 ←── [Shunt Resistor] (전류 측정)
```
