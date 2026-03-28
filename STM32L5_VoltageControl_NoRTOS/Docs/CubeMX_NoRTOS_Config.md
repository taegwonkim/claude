# STM32L5 전압 제어 - FreeRTOS 없음 (Bare-metal HAL) CubeMX 설정 가이드

## RTOS 버전과의 핵심 차이점

| 항목 | RTOS 버전 | Non-RTOS 버전 |
|------|-----------|--------------|
| 멀티태스킹 | FreeRTOS 태스크 5개 | 인터럽트 + 슈퍼루프 |
| 제어 주기 | vTaskDelayUntil | **TIM6 인터럽트 (10ms)** |
| ADC 읽기 | AdcReadTask | **TIM7 인터럽트 (20ms)** |
| UART 수신 | DMA + 세마포어 | **DMA + IDLE 라인 인터럽트** |
| 동기화 | 큐/뮤텍스/세마포어 | **플래그 변수 + volatile** |
| 힙 메모리 | FreeRTOS heap_4 | HAL 기본 (정적 할당) |

---

## 1. 프로젝트 설정 (CubeMX → Project Manager)

```
Project Name: STM32L5_VoltageControl_NoRTOS
Toolchain: STM32CubeIDE
MCU: STM32L552ZETxQ
Heap:  0x800  (2KB, 동적 할당 최소화)
Stack: 0x1000 (4KB)
```

> **Middleware → FreeRTOS: 사용 안 함** (체크 해제)

---

## 2. 클럭 설정 (RTOS 버전과 동일)

```
HSI16 → PLL → SYSCLK 80MHz
PLLM=1, PLLN=10, PLLR=2
HCLK=80MHz, APB1=80MHz, APB2=80MHz
```

---

## 3. 핀 배치 (RTOS 버전과 동일)

| 핀      | 기능         | 설명                    |
|---------|-------------|------------------------|
| PA0~PA3 | TIM2_CH1~4  | PWM 전압 출력 (RC 필터)  |
| PA5     | SPI1_SCK    | MCP3465R               |
| PA6     | SPI1_MISO   | MCP3465R               |
| PA7     | SPI1_MOSI   | MCP3465R               |
| PA9     | USART1_TX   | PC 통신                  |
| PA10    | USART1_RX   | PC 통신                  |
| PB6     | GPIO_OUT    | MCP3465R CS             |
| PB7     | GPIO_IN     | MCP3465R IRQ (EXTI)    |
| PC7     | GPIO_OUT    | 상태 LED                 |

---

## 4. 주변장치 설정

### 4.1 USART1 (RTOS 버전과 동일)
```
Asynchronous, 115200, 8N1
DMA RX: DMA1_Ch1, Circular  ← 핵심! 원형 버퍼로 idle 감지
DMA TX: DMA1_Ch2, Normal
NVIC: USART1 global interrupt Enable (Priority 6)
      DMA1 Ch1/Ch2 Enable (Priority 6)
```

### 4.2 SPI1 (RTOS 버전과 동일)
```
Master, Full-Duplex, 8bit, Mode0(CPOL=0,CPHA=0)
Software NSS, BRP=8 → 10MHz
NVIC: SPI1 global interrupt Enable (Priority 6)
```

### 4.3 TIM2 (PWM - RTOS 버전과 동일)
```
CH1~CH4: PWM Generation
PSC=0, ARR=4095 → ~19.5kHz
Auto-Reload Preload: Enable
```

### 4.4 TIM6 ★ Non-RTOS 핵심 추가!
```
기능: 전압 PI 제어 타이머 (10ms 주기)
Clock Source: Internal Clock
Prescaler: 799    (80MHz / 800 = 100kHz)
Counter Period: 999  (100kHz / 1000 = 100Hz = 10ms)
Auto-Reload Preload: Enable
NVIC: TIM6 global interrupt Enable  ← Priority: 7
      (우선순위 7: 제어 루프, 높을수록 중요)
```

### 4.5 TIM7 ★ Non-RTOS 핵심 추가!
```
기능: ADC 읽기 트리거 타이머 (20ms 주기)
Clock Source: Internal Clock
Prescaler: 799   (80MHz / 800 = 100kHz)
Counter Period: 1999  (100kHz / 2000 = 50Hz = 20ms)
Auto-Reload Preload: Enable
NVIC: TIM7 global interrupt Enable  ← Priority: 7
```

---

## 5. NVIC 우선순위 설정 (Non-RTOS는 단순!)

| 인터럽트             | Priority | 설명                          |
|--------------------|----------|-------------------------------|
| SysTick            | 15       | HAL_GetTick() 기준 (최저 우선) |
| TIM6 global        | 7        | 10ms 제어 루프 (중간)           |
| TIM7 global        | 7        | 20ms ADC 읽기 트리거            |
| USART1 global      | 6        | UART IDLE 감지                  |
| DMA1 Channel1      | 6        | UART RX DMA 완료               |
| DMA1 Channel2      | 6        | UART TX DMA 완료               |
| SPI1 global        | 5        | SPI 완료 (약간 높게)            |
| EXTI9_5 (PB7)      | 6        | MCP3465R IRQ                   |

> **Non-RTOS는 인터럽트 우선순위 제약이 없음!**
> RTOS와 달리 어떤 우선순위 ISR에서도 일반 함수 호출 가능.
> 단, 같은 자원에 접근하는 ISR 간 우선순위를 잘 설계해야 함.

---

## 6. Code Generator 설정

```
[√] Generate peripheral initialization as a pair of .c/.h files
[√] Keep User Code when re-generating
HAL_NVIC_SetPriorityGrouping: NVIC_PRIORITYGROUP_4 (4비트 주선순위, 서브 없음)
```

---

## 7. 아키텍처 설명

```
[인터럽트 기반 이벤트 처리]

TIM6 ISR (10ms)
  └→ g_flags.ctrl_tick = 1  ─→ main loop: PI 제어 실행

TIM7 ISR (20ms)
  └→ g_flags.adc_tick = 1   ─→ main loop: MCP3465R 읽기 시작

USART1 IDLE ISR / DMA Rx Half/Complete
  └→ g_flags.uart_rx_ready = 1  ─→ main loop: 명령 파싱

MCP3465R IRQ EXTI ISR (PB7 하강엣지)
  └→ g_flags.adc_ready = 1  ─→ main loop: ADC 결과 읽기

[메인 루프 (슈퍼루프)]
while(1) {
  if (ctrl_tick)    → VoltageCtrl_Update()
  if (adc_tick)     → MCP3465R_StartScan()
  if (adc_ready)    → MCP3465R_ReadResult()
  if (uart_rx_ready)→ Proto_ParseAndExecute()
  if (tx_pending)   → HAL_UART_Transmit_DMA()
  LED 하트비트
}
```

---

## 8. RC 필터 / 전류감지 회로 (RTOS 버전과 동일)

```
PWM → RC 필터: R=10kΩ, C=330nF (fc≈48Hz)
전류감지: 0.1Ω + INA199A1(게인 20V/V)
MCP3465R:
  CH0~CH3: 전압 측정 (PWM 출력 피드백)
  CH4~CH7: 전류 측정 (INA199 출력)
```
