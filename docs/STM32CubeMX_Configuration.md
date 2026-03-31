# STM32CubeMX Configuration Guide
## MCU: STM32L552RETx / FreeRTOS / Voltage-Current Monitoring

---

## 1. Project Settings

- **MCU**: STM32L552RETx (LQFP64)
- **Project Name**: VoltCurrentMonitor
- **Toolchain/IDE**: STM32CubeIDE
- **Min Heap Size**: 0x800
- **Min Stack Size**: 0x800

---

## 2. System Core

### 2.1 RCC (Reset and Clock Control)
- **HSE**: Crystal/Ceramic Resonator (8 MHz external)
- **LSE**: Crystal/Ceramic Resonator (32.768 kHz)
- **HSI48**: Enabled (for USB, optional)

### 2.2 Clock Configuration
- **PLL Source**: HSE (8 MHz)
- **PLLM**: /1
- **PLLN**: x25 (200 MHz VCO)
- **PLLR**: /2 -> **SYSCLK = 100 MHz**
- **PLLQ**: /4 -> 50 MHz (for FDCAN)
- **AHB Prescaler**: /1 -> HCLK = 100 MHz
- **APB1 Prescaler**: /1 -> APB1 = 100 MHz
- **APB2 Prescaler**: /1 -> APB2 = 100 MHz

### 2.3 SYS
- **Debug**: Serial Wire
- **Timebase Source**: TIM6 (IMPORTANT: FreeRTOS uses SysTick, so HAL must use a different timer)

### 2.4 NVIC (Nested Vectored Interrupt Controller)
| Interrupt                    | Priority Group | Preemption Priority | Sub Priority | Enabled |
|------------------------------|----------------|---------------------|--------------|---------|
| TIM6 global interrupt        | -              | 0                   | 0            | Yes     |
| SPI1 global interrupt        | -              | 5                   | 0            | Yes     |
| SPI2 global interrupt        | -              | 5                   | 0            | Yes     |
| USART1 global interrupt      | -              | 5                   | 0            | Yes     |
| FDCAN1 interrupt 0           | -              | 5                   | 0            | Yes     |
| FDCAN1 interrupt 1           | -              | 5                   | 0            | Yes     |
| UART4 global interrupt       | -              | -                   | -            | No (TX blocking) |

- **Priority Grouping**: 4 bits for preemption priority (NVIC_PRIORITYGROUP_4)

---

## 3. Peripherals Configuration

### 3.1 SPI1 - External ADC (MCP3465R)
- **Mode**: Full-Duplex Master
- **Frame Format**: Motorola
- **Data Size**: 8 bits
- **First Bit**: MSB First
- **Prescaler**: 32 (APB2/32 = 3.125 MHz, MCP3465R max 20 MHz)
- **Clock Polarity (CPOL)**: Low (idle low)
- **Clock Phase (CPHA)**: 1 Edge (data sampled on rising edge) -> SPI Mode 0,0
- **NSS**: Software (GPIO managed manually)
- **DMA**: Not used (register-level access)
- **NVIC**: SPI1 interrupt enabled

**GPIO Pins (SPI1):**
| Function | Pin  | Mode            | Pull   | Speed |
|----------|------|-----------------|--------|-------|
| SCK      | PA5  | AF5 (SPI1_SCK)  | No Pull| High  |
| MISO     | PA6  | AF5 (SPI1_MISO) | No Pull| High  |
| MOSI     | PA7  | AF5 (SPI1_MOSI) | No Pull| High  |
| CS (NSS) | PA4  | GPIO_Output      | Pull-Up| High  |

> Note: MCP3465R SPI Mode: supports Mode 0,0 and Mode 1,1. We use Mode 0,0.

### 3.2 SPI2 - External DAC (AD5641 x4)
- **Mode**: Transmit Only Master (DAC is write-only)
- **Frame Format**: Motorola
- **Data Size**: 8 bits
- **First Bit**: MSB First
- **Prescaler**: 16 (APB1/16 = 6.25 MHz, AD5641 max 30 MHz)
- **Clock Polarity (CPOL)**: Low
- **Clock Phase (CPHA)**: 2 Edge (data latched on falling edge) -> SPI Mode 0,1
- **NSS**: Software (4 individual CS pins for 4 DAC chips)

**GPIO Pins (SPI2):**
| Function   | Pin  | Mode             | Pull   | Speed |
|------------|------|------------------|--------|-------|
| SCK        | PB13 | AF5 (SPI2_SCK)   | No Pull| High  |
| MOSI       | PB15 | AF5 (SPI2_MOSI)  | No Pull| High  |
| DAC_CS0    | PB12 | GPIO_Output      | Pull-Up| High  |
| DAC_CS1    | PB14 | GPIO_Output      | Pull-Up| High  |
| DAC_CS2    | PB1  | GPIO_Output      | Pull-Up| High  |
| DAC_CS3    | PB2  | GPIO_Output      | Pull-Up| High  |

> Note: AD5641 data is latched on CS rising edge. CPOL=0, CPHA=1 (Mode 0,1) for AD5641.
> All CS pins initialized HIGH (inactive).

### 3.3 USART1 - PC Communication
- **Mode**: Asynchronous
- **Baud Rate**: 115200
- **Word Length**: 8 bits
- **Stop Bits**: 1
- **Parity**: None
- **Hardware Flow Control**: None
- **Oversampling**: 16
- **NVIC**: USART1 global interrupt enabled
- **DMA (optional for TX)**:
  - DMA1 Channel 1: USART1_TX, Memory to Peripheral, Byte, Normal mode
  - OR use interrupt-based TX

**GPIO Pins (USART1):**
| Function | Pin  | Mode              | Pull   | Speed  |
|----------|------|-------------------|--------|--------|
| TX       | PA9  | AF7 (USART1_TX)   | Pull-Up| High   |
| RX       | PA10 | AF7 (USART1_RX)   | Pull-Up| High   |

### 3.4 FDCAN1 - CAN Bus Communication
- **Mode**: Normal Mode
- **Frame Format**: Classic CAN / CAN FD (choose Classic for compatibility)
- **Nominal Bit Rate**: 500 kbps
- **Auto Retransmission**: Enable
- **Transmit Pause**: Enable
- **Protocol Exception Handling**: Disable

**FDCAN Clock & Bit Timing (500 kbps, FDCAN kernel clock = 50 MHz from PLLQ):**
- **Nominal Prescaler**: 5 -> Tq = 5/50MHz = 100ns
- **Nominal Time Seg1 (Prop + Phase1)**: 15 Tq
- **Nominal Time Seg2**: 4 Tq
- **Nominal Sync Jump Width**: 4 Tq
- **Bit Time** = (1 + 15 + 4) Tq = 20 Tq = 2000ns -> 500 kbps
- **Sample Point** = (1 + 15) / 20 = 80%

**Message RAM Configuration:**
- Standard Filter Elements: 1
- Extended Filter Elements: 0
- Rx FIFO0 Elements: 4
- Rx FIFO1 Elements: 0
- Tx FIFO/Queue Elements: 4
- Tx Event FIFO Elements: 0

**GPIO Pins (FDCAN1):**
| Function | Pin  | Mode              | Pull   | Speed |
|----------|------|-------------------|--------|-------|
| TX       | PA12 | AF9 (FDCAN1_TX)   | No Pull| High  |
| RX       | PA11 | AF9 (FDCAN1_RX)   | No Pull| High  |

### 3.5 UART4 - Debug Output (TX Only)
- **Mode**: Asynchronous
- **Direction**: TX Only (수신 불필요, 디버그 출력 전용)
- **Baud Rate**: 115200
- **Word Length**: 8 bits
- **Stop Bits**: 1
- **Parity**: None
- **Hardware Flow Control**: None
- **Oversampling**: 16
- **NVIC**: UART4 interrupt **disabled** (blocking TX 사용, 인터럽트 불필요)

**GPIO Pins (UART4):**
| Function | Pin  | Mode             | Pull   | Speed  |
|----------|------|------------------|--------|--------|
| TX       | PA0  | AF8 (UART4_TX)   | Pull-Up| High   |

> Note: UART4는 디버그 전용이므로 TX만 사용. PC에서 USB-UART 변환기로 PA0에 연결하여 모니터링.
> Debug 출력 형식: `[DBG][TAG  ] message\r\n`
> TAG: SYS(시스템), VCTRL(전압제어), FAULT(고장감지), COMM(통신)

### 3.6 GPIO - Status LEDs & Misc
| Label       | Pin | Mode        | Default | Purpose              |
|-------------|-----|-------------|---------|----------------------|
| LED_STATUS  | PC13| GPIO_Output | Low     | System heartbeat     |
| LED_FAULT   | PC14| GPIO_Output | Low     | Fault indicator      |

---

## 4. FreeRTOS Configuration (CRITICAL SECTION)

### 4.1 Middleware -> RTOS -> FreeRTOS
- **Interface**: CMSIS_V2
- **Version**: Use the latest available in CubeMX (e.g., V10.3.1+)

### 4.2 Config Parameters (FreeRTOS tab)

#### Kernel Settings
| Parameter                        | Value    | Reason                                           |
|----------------------------------|----------|--------------------------------------------------|
| USE_PREEMPTION                   | Enabled  | Preemptive scheduling for real-time control       |
| CPU_CLOCK_HZ                     | 100000000| Match SYSCLK                                     |
| TICK_RATE_HZ                     | 1000     | 1ms tick resolution for voltage control loop      |
| MAX_PRIORITIES                   | 7        | 7 priority levels (0=idle to 6=highest)           |
| MINIMAL_STACK_SIZE               | 128      | 128 words (512 bytes) minimum                     |
| TOTAL_HEAP_SIZE                  | 15360    | 15KB heap (STM32L552 has 256KB SRAM)              |
| MAX_TASK_NAME_LEN                | 16       | Task name length                                  |
| USE_16_BIT_TICKS                 | Disabled | 32-bit tick counter                               |
| IDLE_SHOULD_YIELD                | Enabled  | Idle yields to same-priority tasks                |
| USE_TASK_NOTIFICATIONS           | Enabled  | For inter-task signaling                          |
| USE_MUTEXES                      | Enabled  | SPI bus mutual exclusion                          |
| USE_RECURSIVE_MUTEXES            | Disabled | Not needed                                        |
| USE_COUNTING_SEMAPHORES          | Enabled  | For resource management                           |
| QUEUE_REGISTRY_SIZE              | 8        | For debug visibility                              |
| USE_QUEUE_SETS                   | Disabled | Not needed                                        |
| ENABLE_BACKWARD_COMPATIBILITY    | Disabled | Use CMSIS_V2 API                                  |
| NUM_THREAD_LOCAL_STORAGE_POINTERS| 0        | Not needed                                        |

#### Memory Management
| Parameter                        | Value    | Reason                                           |
|----------------------------------|----------|--------------------------------------------------|
| Memory Allocation                | Dynamic  | Dynamic allocation with heap_4                    |
| MEMORY_MANAGEMENT_SCHEME         | heap_4   | Best general-purpose allocator with fragmentation handling |

#### Hook Functions
| Parameter                        | Value    | Reason                                           |
|----------------------------------|----------|--------------------------------------------------|
| USE_IDLE_HOOK                    | Disabled | Not needed                                        |
| USE_TICK_HOOK                    | Disabled | Not needed                                        |
| USE_MALLOC_FAILED_HOOK           | Enabled  | Detect heap exhaustion                            |
| CHECK_FOR_STACK_OVERFLOW         | Option 2 | Stack overflow detection (watermark + pattern)    |
| USE_DAEMON_TASK_STARTUP_HOOK     | Disabled | Not needed                                        |

#### Timer / Software Timer
| Parameter                        | Value    | Reason                                           |
|----------------------------------|----------|--------------------------------------------------|
| USE_TIMERS                       | Enabled  | For periodic reporting timer                      |
| TIMER_TASK_PRIORITY              | 5        | High priority for timer callbacks                 |
| TIMER_TASK_STACK_DEPTH           | 256      | 1KB stack for timer task                          |
| TIMER_QUEUE_LENGTH               | 10       | Timer command queue size                          |

#### Run Time Stats
| Parameter                        | Value    | Reason                                           |
|----------------------------------|----------|--------------------------------------------------|
| GENERATE_RUN_TIME_STATS          | Disabled | Enable later for profiling if needed              |
| USE_TRACE_FACILITY               | Disabled | Enable later for debugging if needed              |
| USE_STATS_FORMATTING_FUNCTIONS   | Disabled | Not needed initially                              |

### 4.3 Tasks Configuration (CubeMX Tasks Tab)

Create the following tasks in CubeMX:

#### Task 1: VoltageControlTask
| Parameter      | Value                |
|----------------|----------------------|
| Task Name      | VoltCtrlTask         |
| Priority       | osPriorityAboveNormal (= 5) |
| Stack Size     | 512 (words = 2048 bytes) |
| Entry Function | StartVoltCtrlTask    |
| Code Gen Option| As external          |
| Allocation     | Dynamic              |

> **Purpose**: Closed-loop voltage control. Reads ADC, compares with target, adjusts DAC.
> **Period**: 10ms (100 Hz control loop)

#### Task 2: CommRxTask
| Parameter      | Value                |
|----------------|----------------------|
| Task Name      | CommRxTask           |
| Priority       | osPriorityNormal (= 4) |
| Stack Size     | 384 (words = 1536 bytes) |
| Entry Function | StartCommRxTask      |
| Code Gen Option| As external          |
| Allocation     | Dynamic              |

> **Purpose**: Receives and parses UART1 commands from PC.

#### Task 3: CommTxTask
| Parameter      | Value                |
|----------------|----------------------|
| Task Name      | CommTxTask           |
| Priority       | osPriorityBelowNormal (= 3) |
| Stack Size     | 384 (words = 1536 bytes) |
| Entry Function | StartCommTxTask      |
| Code Gen Option| As external          |
| Allocation     | Dynamic              |

> **Purpose**: Periodic transmission of voltage/current data via UART1 and FDCAN1.
> **Period**: 100ms (10 Hz reporting)

#### Task 4: FaultMonitorTask
| Parameter      | Value                |
|----------------|----------------------|
| Task Name      | FaultMonTask         |
| Priority       | osPriorityHigh (= 6) |
| Stack Size     | 256 (words = 1024 bytes) |
| Entry Function | StartFaultMonTask    |
| Code Gen Option| As external          |
| Allocation     | Dynamic              |

> **Purpose**: Monitors current for short circuit / open circuit detection.
> **Period**: 50ms (20 Hz fault check)

### 4.4 Mutexes (CubeMX Mutexes Tab)

#### Mutex 1: SPI1 Mutex
| Parameter  | Value       |
|------------|-------------|
| Name       | spi1Mutex   |
| Type       | osMutexNormal |
| Allocation | Dynamic     |

#### Mutex 2: SPI2 Mutex
| Parameter  | Value       |
|------------|-------------|
| Name       | spi2Mutex   |
| Type       | osMutexNormal |
| Allocation | Dynamic     |

### 4.5 Queues (CubeMX Queues Tab)

#### Queue 1: UART Command Queue
| Parameter  | Value          |
|------------|----------------|
| Name       | uartCmdQueue   |
| Queue Size | 8              |
| Item Size  | 32 (bytes)     |
| Allocation | Dynamic        |

### 4.6 Semaphores
- None explicitly needed (using task notifications instead)

### 4.7 Events (Event Flags)

#### Event Group 1: System Events
| Parameter  | Value        |
|------------|--------------|
| Name       | sysEventGroup|
| Allocation | Dynamic      |

> Flags: BIT0 = new command received, BIT1 = fault detected, BIT2 = calibration complete

---

## 5. Important CubeMX Notes

### 5.1 HAL Timebase vs FreeRTOS
- **SYS -> Timebase Source = TIM6** (NOT SysTick!)
- FreeRTOS uses SysTick internally
- If both HAL and FreeRTOS use SysTick, timing conflicts occur
- TIM6 is a basic timer, ideal for HAL timebase

### 5.2 Interrupt Priority Constraints
- FreeRTOS manages interrupts with priority >= configMAX_SYSCALL_INTERRUPT_PRIORITY
- configMAX_SYSCALL_INTERRUPT_PRIORITY = 5 (default in CubeMX)
- All peripheral interrupts that use FreeRTOS API (SPI, UART, CAN) must have priority >= 5
- TIM6 (HAL timebase) must have priority < 5 (set to 0) to not be masked by FreeRTOS

### 5.3 FDCAN Clock Source
- In RCC -> FDCAN Clock Mux -> select PLLQ (54 MHz)
- This allows precise bit timing for 500 kbps

### 5.4 Power Configuration
- Enable VDD = 3.3V
- VDDA = 3.3V (for internal ADC reference if used)
- External DAC/ADC powered by 5V with level shifting if needed, or 3.3V SPI logic

### 5.5 Code Generation Settings
- **Generate peripheral initialization as pair of .c/.h files**: Yes
- **Set all free pins as analog**: Yes (reduces power consumption)
- **Do not generate the main() function**: No (let CubeMX generate main.c as template)

---

## 6. Pin Mapping Summary (STM32L552RETx LQFP64)

```
PA0  - UART4_TX (DBG)   PA8  - (free)
PA1  - (free)           PA9  - USART1_TX
PA2  - (free)           PA10 - USART1_RX
PA3  - (free)           PA11 - FDCAN1_RX
PA4  - SPI1_CS (GPIO)   PA12 - FDCAN1_TX
PA5  - SPI1_SCK         PA13 - SWDIO
PA6  - SPI1_MISO        PA14 - SWCLK
PA7  - SPI1_MOSI        PA15 - (free)

PB0  - (free)           PB8  - (free)
PB1  - DAC_CS2 (GPIO)   PB9  - (free)
PB2  - DAC_CS3 (GPIO)   PB10 - (free)
PB3  - (free)           PB11 - (free)
PB4  - (free)           PB12 - DAC_CS0 (GPIO)
PB5  - (free)           PB13 - SPI2_SCK
PB6  - (free)           PB14 - DAC_CS1 (GPIO)
PB7  - (free)           PB15 - SPI2_MOSI

PC13 - LED_STATUS       PC14 - LED_FAULT
```

---

## 7. Hardware Connection Diagram

```
                    STM32L552R
                   +-----------+
    PC <--UART1--> |PA9  PA10  |
                   |           |
    CAN Bus <----> |PA12 PA11  | (via CAN transceiver, e.g., MCP2562)
                   |           |
    MCP3465R <---> |PA5(SCK)   |
    (ADC)          |PA6(MISO)  | SPI1
                   |PA7(MOSI)  |
                   |PA4(CS)    |
                   |           |
    AD5641 x4 <--- |PB13(SCK)  |
    (DAC)          |PB15(MOSI) | SPI2
                   |PB12(CS0)  |
                   |PB14(CS1)  |
                   |PB1 (CS2)  |
                   |PB2 (CS3)  |
                   +-----------+

    Debug <------> |PA0 (TX)    | UART4 (TX only, 115200)
                   |           |
    MCP3465R Channel Assignment:
    - CH0 ~ CH3: Voltage feedback from DAC CH0 ~ CH3
    - CH4 ~ CH7: Current sensing from DAC CH0 ~ CH3
                  (via shunt resistor + INA180 current sense amplifier)

    AD5641 x4: Each chip is single-channel 14-bit DAC
    - DAC0 (CS0/PB12): Channel 0 voltage output
    - DAC1 (CS1/PB14): Channel 1 voltage output
    - DAC2 (CS2/PB1):  Channel 2 voltage output
    - DAC3 (CS3/PB2):  Channel 3 voltage output
```
