# STM32CubeMX 세부 설정 가이드

## MCU: STM32L552RE (LQFP64)

---

## 1. Clock Configuration (RCC)

| 항목 | 설정값 |
|------|--------|
| HSE | Crystal/Ceramic Resonator, 16MHz |
| PLL Source | HSE |
| PLLM | /1 |
| PLLN | ×10 |
| PLLR | /2 → SYSCLK = 80MHz |
| SYSCLK | 80 MHz |
| AHB Prescaler | /1 → HCLK = 80 MHz |
| APB1 Prescaler | /1 → PCLK1 = 80 MHz |
| APB2 Prescaler | /1 → PCLK2 = 80 MHz |

---

## 2. SPI1 – DAC AD5641 (4채널)

| 항목 | 설정값 |
|------|--------|
| Mode | Full-Duplex Master |
| Hardware NSS | Disabled (Software CS per channel) |
| Data Size | 16 Bits |
| First Bit | MSB First |
| Prescaler | 8 → Baud Rate = 10 MHz |
| CPOL | Low |
| CPHA | 1 Edge (Mode 0) |
| CRC | Disabled |

**Pin 할당:**
| Signal | Pin |
|--------|-----|
| SPI1_SCK | PA5 |
| SPI1_MOSI | PA7 |
| DAC_CS0 (CH1) | PC0 (GPIO Output, High) |
| DAC_CS1 (CH2) | PC1 (GPIO Output, High) |
| DAC_CS2 (CH3) | PC2 (GPIO Output, High) |
| DAC_CS3 (CH4) | PC3 (GPIO Output, High) |

---

## 3. SPI2 – ADC MCP3465R

| 항목 | 설정값 |
|------|--------|
| Mode | Full-Duplex Master |
| Hardware NSS | Disabled |
| Data Size | 8 Bits |
| First Bit | MSB First |
| Prescaler | 8 → Baud Rate = 10 MHz |
| CPOL | Low |
| CPHA | 1 Edge (Mode 0) |
| CRC | Disabled |

**Pin 할당:**
| Signal | Pin |
|--------|-----|
| SPI2_SCK | PB13 |
| SPI2_MISO | PB14 |
| SPI2_MOSI | PB15 |
| ADC_CS | PB12 (GPIO Output, High) |
| ADC_IRQ | PB11 (GPIO Input, EXTI) |

---

## 4. USART1 – PC 통신

| 항목 | 설정값 |
|------|--------|
| Mode | Asynchronous |
| Baud Rate | 115200 |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |
| HW Flow Control | None |
| DMA TX | USART1_TX (DMA1 CH4, Normal, Byte) |
| DMA RX | USART1_RX (DMA1 CH5, Circular, Byte) |
| Global Interrupt | Enable |

**Pin 할당:**
| Signal | Pin |
|--------|-----|
| USART1_TX | PA9 |
| USART1_RX | PA10 |

---

## 5. FDCAN1 – CAN FD 통신

| 항목 | 설정값 |
|------|--------|
| Nominal Prescaler | 4 → TQ = 50ns (20MHz 기준) |
| Nominal Time Seg1 | 14 |
| Nominal Time Seg2 | 5 → Nominal Baud = 1 Mbps |
| Nominal SJW | 4 |
| Data Prescaler | 2 |
| Data Time Seg1 | 8 |
| Data Time Seg2 | 3 → Data Baud = 2 Mbps |
| Data SJW | 3 |
| Frame Format | FD with BRS |
| Mode | Normal |
| Auto Retransmission | Enable |
| FIFO0 Messages | 8 |
| Global Filter | Reject non-matching std/ext frames |

**Pin 할당:**
| Signal | Pin |
|--------|-----|
| FDCAN1_RX | PD0 |
| FDCAN1_TX | PD1 |

> FDCAN 클럭: APB1 = 80MHz, FDCAN 전용 PLL 설정 필요 시 PLL1Q 사용

---

## 6. FreeRTOS (CMSIS-RTOS v2) 설정

### 6.1 Middleware > FreeRTOS

| 항목 | 설정값 |
|------|--------|
| Interface | CMSIS_V2 |
| configTICK_RATE_HZ | 1000 (1ms tick) |
| configTOTAL_HEAP_SIZE | 20480 (20KB) |
| configMAX_PRIORITIES | 7 |
| configUSE_MUTEXES | 1 |
| configUSE_RECURSIVE_MUTEXES | 1 |
| configUSE_COUNTING_SEMAPHORES | 1 |
| configUSE_TASK_NOTIFICATIONS | 1 |
| configUSE_TIMERS | 0 |
| configUSE_TRACE_FACILITY | 1 |
| configGENERATE_RUN_TIME_STATS | 0 |
| INCLUDE_vTaskDelay | 1 |
| INCLUDE_vTaskDelayUntil | 1 |
| USE_NEWLIB_REENTRANT | 0 |

### 6.2 Tasks 설정

| Task Name | Priority | Stack (Words) | Entry Function |
|-----------|----------|---------------|----------------|
| UARTRxTask | osPriorityHigh (6) | 256 | vTaskUARTReceive |
| ADCMeasureTask | osPriorityAboveNormal (5) | 256 | vTaskADCMeasure |
| DACControlTask | osPriorityAboveNormal (5) | 256 | vTaskDACControl |
| FaultDetectTask | osPriorityNormal (4) | 128 | vTaskFaultDetect |
| DataTxTask | osPriorityBelowNormal (3) | 256 | vTaskDataTransmit |

### 6.3 Queues 설정

| Queue Name | Queue Length | Item Size |
|------------|-------------|-----------|
| xQueueUARTCmd | 10 | sizeof(UartCmd_t) = 12 bytes |
| xQueueTxData | 4 | sizeof(TxPacket_t) = 64 bytes |

### 6.4 Mutexes 설정

| Mutex Name | 용도 |
|------------|------|
| xMutexSPI1 | DAC SPI1 버스 접근 보호 |
| xMutexSPI2 | ADC SPI2 버스 접근 보호 |
| xMutexChannelData | 채널 공유 데이터 보호 |

### 6.5 Semaphores 설정

| Semaphore Name | Type | 용도 |
|----------------|------|------|
| xSemADCReady | Binary | ADC 측정 완료 신호 |

---

## 7. GPIO 추가 설정

| Pin | Mode | Label | 설명 |
|-----|------|-------|------|
| PC0 | Output Push-Pull | DAC_CS0 | CH1 DAC CS |
| PC1 | Output Push-Pull | DAC_CS1 | CH2 DAC CS |
| PC2 | Output Push-Pull | DAC_CS2 | CH3 DAC CS |
| PC3 | Output Push-Pull | DAC_CS3 | CH4 DAC CS |
| PB12 | Output Push-Pull | ADC_CS | MCP3465R CS |
| PB11 | Input | ADC_IRQ | MCP3465R IRQ (EXTI11) |
| PC13 | Output Push-Pull | LED_STATUS | 상태 LED |

---

## 8. NVIC 인터럽트 우선순위

| 인터럽트 | Preemption Priority | Sub Priority |
|----------|--------------------|--------------|
| SysTick | 15 (FreeRTOS) | 0 |
| PendSV | 15 (FreeRTOS) | 0 |
| SVCall | 0 | 0 |
| USART1_IRQn | 5 | 0 |
| EXTI15_10_IRQn (ADC IRQ) | 6 | 0 |
| FDCAN1_IT0_IRQn | 7 | 0 |
| DMA1_Channel4_IRQn (TX) | 6 | 0 |
| DMA1_Channel5_IRQn (RX) | 6 | 0 |

> FreeRTOS는 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY(=5) 이상 우선순위에서만 FromISR API 사용 가능

---

## 9. Power Mode

- Voltage Range: Range 1 (최고 성능, 1.2V core)
- ICACHE: Enable
- DCACHE: Enable

---

## 10. Project Settings (STM32CubeIDE)

| 항목 | 설정값 |
|------|--------|
| Toolchain | STM32CubeIDE |
| Linker Script | STM32L552RETX_FLASH.ld |
| Generate peripheral init | Yes, as pair of .c/.h |
| Keep user code sections | Yes |
