# STM32L552R — 4채널 전압 PID 제어 (FreeRTOS)

## 시스템 개요

```
┌─────────────────────────────────────────────────────────────┐
│                      STM32L552R (110 MHz)                    │
│                                                              │
│  [FreeRTOS PIDControlTask - 10ms]                           │
│   MCP3465R ──→ ADC Read ──→ PID Compute ──→ AD5641 DAC     │
│   (실제 전압 측정)    (오차 보정)          (제어 전압 출력)    │
└──────────┬──────────────────────────────────────┬───────────┘
           │ SPI2                                 │ SPI1
    ┌──────▼──────┐                     ┌─────────▼──────────┐
    │ MCP3465R    │                     │ AD5641 × 4개        │
    │ 24-bit ADC  │                     │ 14-bit DAC          │
    │ 4채널       │                     │ 채널 0~3            │
    └──────┬──────┘                     └─────────┬──────────┘
    CH0~CH3│ (실제 전압 측정)                      │ (전력단 제어)
           │                                       │
    ┌──────▼───────────────────────────────────────▼──────┐
    │         전력단 + 가변 부하                             │
    │   VOUT_CH0~CH3 ──→ 전압 분배기 ──→ ADC 입력          │
    └────────────────────────────────────────────────────┘
```

## 하드웨어 핀 배치

| 핀    | 기능                | 설명                          |
|-------|---------------------|-------------------------------|
| PA5   | SPI1_SCK            | AD5641 클럭                   |
| PA7   | SPI1_MOSI           | AD5641 데이터 입력             |
| PA9   | USART1_TX           | 디버그 출력 (115200 baud)      |
| PA10  | USART1_RX           | 디버그 입력                   |
| PB0   | CS_DAC_CH0          | AD5641 CH0 칩 선택 (Active-L) |
| PB1   | CS_DAC_CH1          | AD5641 CH1 칩 선택 (Active-L) |
| PB2   | CS_DAC_CH2          | AD5641 CH2 칩 선택 (Active-L) |
| PB10  | CS_DAC_CH3          | AD5641 CH3 칩 선택 (Active-L) |
| PB13  | SPI2_SCK            | MCP3465R 클럭                 |
| PB14  | SPI2_MISO           | MCP3465R 데이터 출력           |
| PB15  | SPI2_MOSI           | MCP3465R 데이터 입력           |
| PC0   | ADC_INT (EXTI0)     | MCP3465R 변환 완료 인터럽트    |
| PC7   | CS_ADC              | MCP3465R 칩 선택 (Active-L)   |

## 프로젝트 구조

```
Core/
├── Inc/
│   ├── main.h              — 핀 정의, 외부 HAL 핸들 선언
│   ├── pid.h               — PID 제어기 인터페이스
│   ├── ad5641.h            — AD5641 14-bit DAC 드라이버
│   ├── mcp3465r.h          — MCP3465R 24-bit ADC 드라이버
│   ├── voltage_control.h   — 4채널 전압 제어 시스템
│   ├── FreeRTOSConfig.h    — FreeRTOS 설정
│   ├── stm32l5xx_it.h      — ISR 프로토타입
└── Src/
    ├── main.c              — HAL 초기화, 클럭 설정, 진입점
    ├── freertos.c          — FreeRTOS 태스크 정의
    ├── pid.c               — PID 알고리즘 구현
    ├── ad5641.c            — AD5641 SPI 드라이버
    ├── mcp3465r.c          — MCP3465R SPI 드라이버
    ├── voltage_control.c   — 제어 루프 로직
    ├── stm32l5xx_hal_msp.c — HAL 저수준 핀 초기화
    └── stm32l5xx_it.c      — 인터럽트 핸들러
```

## STM32CubeIDE 프로젝트 생성 방법

1. **새 프로젝트 생성**
   - File → New → STM32 Project
   - MCU: `STM32L552RETx`
   - Project Name: `stm32-pid-voltage-control`

2. **CubeMX 설정 (.ioc)**
   - **SPI1**: Transmit Only Master, CPOL=0, CPHA=0, 8-bit, PSC=16
   - **SPI2**: Full-Duplex Master, CPOL=0, CPHA=0, 8-bit, PSC=16
   - **USART1**: Asynchronous, 115200 baud
   - **GPIO Output**: PB0, PB1, PB2, PB10, PC7 (CS 핀, High 초기값)
   - **GPIO Input**: PC0, Pull-Up, EXTI Line0, Falling Edge
   - **NVIC**: EXTI Line0 우선순위 5, FreeRTOS 활성화
   - **Middleware → FreeRTOS**: CMSIS-RTOS v2, Heap4, 10kB

3. **코드 생성 후 사용자 파일 복사**
   - 이 저장소의 `Core/Inc/*.h`, `Core/Src/*.c` 파일을 프로젝트에 복사
   - CubeMX가 생성한 `main.c`의 USER CODE 영역에 아래를 추가:
     ```c
     /* USER CODE BEGIN Includes */
     #include "voltage_control.h"
     /* USER CODE END Includes */
     ```

## FreeRTOS 태스크

| 태스크          | 주기   | 우선순위 | 스택   | 역할                          |
|----------------|--------|----------|--------|-------------------------------|
| PIDControlTask | 10 ms  | High     | 2 kB   | ADC 읽기 → PID → DAC 갱신    |
| MonitorTask    | 500 ms | Normal   | 1 kB   | UART로 채널 상태 출력          |

## PID 파라미터 튜닝

`voltage_control.h`에서 기본값 수정:
```c
#define VCTRL_DEFAULT_KP   1.2f   // 비례 이득
#define VCTRL_DEFAULT_KI   0.5f   // 적분 이득
#define VCTRL_DEFAULT_KD   0.05f  // 미분 이득
```

런타임 변경:
```c
App_SetChannelPIDGains(0, 1.5f, 0.8f, 0.02f);  // CH0 이득 변경
App_SetChannelVoltage(0, 2.0f);                  // CH0 목표 2.0V
```

## MCP3465R 설정 (mcp3465r.c 내 cfg[] 배열)

| 레지스터 | 값   | 설정 내용                               |
|---------|------|-----------------------------------------|
| CONFIG0 | 0xB2 | 내부 2.4V Vref, 내부 클럭, 대기 모드     |
| CONFIG1 | 0x14 | OSR=1024 (≈0.5ms/채널), 프리스케일러=1  |
| CONFIG2 | 0x8B | Boost=1x, Gain=1x, Auto-zero Vref ON   |
| CONFIG3 | 0x30 | One-shot 모드, 32-bit CH_ID 포함 형식   |
| IRQ     | 0x0A | INT핀 비활성=HIGH/활성=LOW, FastCmd ON   |

## 주의사항

- **전압 범위**: MCP3465R 내부 Vref=2.4V → ADC 입력은 반드시 0~2.4V 이내
  - 높은 전압 측정 시 저항 분배기 사용 후 `MCP3465R_RawToVoltage()`에 스케일 인수 추가
- **AD5641 Vref**: 기본 2.5V 설정 (`VCtrl_Init()` 내 `AD5641_Init` 인수 수정)
- **SPI 공유 주의**: SPI1에 연결된 4개 AD5641은 CS 핀으로 구분
- **FreeRTOS 우선순위**: EXTI0 인터럽트는 우선순위 5 이하로 설정 (syscall 임계값)
