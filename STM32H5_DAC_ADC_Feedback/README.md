# STM32H5 DAC/ADC 피드백 전압 제어 (FreeRTOS)

STM32H563을 기반으로 FreeRTOS를 사용하여 **4채널 전압을 DAC로 출력**하고,
**ADC 피드백**을 통해 부하 변동에도 일정 전압을 유지하는 **PID 폐루프 제어** 프로젝트.

---

## 하드웨어 구성

### MCU
| 항목 | 사양 |
|------|------|
| MCU | STM32H563 / STM32H573 |
| 코어 | Cortex-M33 @ 250 MHz |
| DAC | DAC1 (12-bit, 2채널 내장) |
| ADC | ADC1 (12-bit + 16x 오버샘플링) |

### 핀 배치

| 핀  | 기능           | 역할                    |
|-----|----------------|-------------------------|
| PA0 | ADC1_IN0       | 채널 0 피드백 입력      |
| PA1 | ADC1_IN1       | 채널 1 피드백 입력      |
| PA2 | ADC1_IN2       | 채널 2 피드백 입력      |
| PA3 | ADC1_IN3       | 채널 3 피드백 입력      |
| PA4 | DAC1_OUT1      | 채널 0 전압 출력        |
| PA5 | DAC1_OUT2      | 채널 1 전압 출력        |
| PA6 | TIM3_CH1 (PWM) | 채널 2 전압 출력 (RC)   |
| PA7 | TIM3_CH2 (PWM) | 채널 3 전압 출력 (RC)   |
| PD8 | USART3_TX      | 디버그 UART 출력        |
| PD9 | USART3_RX      | 명령 수신 (SET 명령)    |

### 채널 3, 4: PWM + RC 필터 (소프트웨어 DAC)

```
PA6/PA7 (PWM) ──[R=1kΩ]──┬── 출력 전압
                           │
                         [C=10µF]
                           │
                          GND
```

- PWM 주파수: ~984 Hz (PCLK1=125MHz, PSC=30, ARR=4095)
- RC 필터 fc ≈ 15.9 Hz → 리플 충분히 제거
- 해상도: 12-bit 등가 (DAC1과 동일)

---

## 소프트웨어 구조

```
Core/
├── Inc/
│   ├── main.h            # 전역 핸들, FreeRTOS 객체 extern 선언
│   ├── pid.h             # PID 제어기 API
│   ├── voltage_ctrl.h    # 4채널 전압 제어 레이어 API
│   └── FreeRTOSConfig.h  # FreeRTOS 설정
└── Src/
    ├── main.c            # HAL 초기화, 태스크 생성, 스케줄러 시작
    ├── pid.c             # PID 구현 (Anti-windup, DoM)
    ├── voltage_ctrl.c    # DAC/PWM 출력 + PID 연동
    └── freertos_tasks.c  # FreeRTOS 태스크 정의
```

### 태스크 구조

```
┌─────────────────────────────────────────┐
│  vADCTask (Priority: MAX-1)             │
│  ADC DMA 완료 세마포어 대기             │
│  → VCtrl_UpdateFeedback()               │
│  주기: ADC DMA 완료 이벤트 (~연속)      │
├─────────────────────────────────────────┤
│  vControlTask (Priority: MAX-2)         │
│  10 ms 주기 (vTaskDelayUntil)           │
│  → VCtrl_RunControl() [PID + 출력 갱신] │
├─────────────────────────────────────────┤
│  vMonitorTask (Priority: IDLE+1)        │
│  500 ms 주기 UART 상태 출력             │
└─────────────────────────────────────────┘
```

---

## 제어 알고리즘

### PID 제어기 특징
- **Derivative-on-Measurement**: setpoint 급변 시 미분 충격(Kick) 방지
- **Clamping Anti-Windup**: 포화 상태에서 적분 누적 중단
- **출력 클램핑**: 0 ~ 4095 (12-bit DAC 범위)

### 기본 PID 튜닝 파라미터

| 파라미터 | 값     | 설명                     |
|----------|--------|--------------------------|
| Kp       | 200.0  | 비례 이득 (빠른 응답)    |
| Ki       |  50.0  | 적분 이득 (정상오차 제거)|
| Kd       |   5.0  | 미분 이득 (진동 억제)    |
| dt       |  10 ms | 제어 주기                |

> **튜닝 방법**: 실제 부하 조건에서 Kp부터 올려가며 진동 없는 최대값 탐색 →
> Ki로 정상상태 오차 제거 → Kd로 과도응답 개선 (Ziegler-Nichols 방법 적용 가능)

---

## UART 명령어

터미널(115200 baud, `\r\n` 종료)에서 채널 목표 전압 변경 가능:

```
SET <채널번호> <전압V>
```

예시:
```
SET 0 2.5    → 채널 0을 2.5V로 설정
SET 3 1.2    → 채널 3을 1.2V로 설정
```

---

## 빌드 방법

### 사전 요구사항
- arm-none-eabi-gcc ≥ 12.x
- CMake ≥ 3.22
- STM32CubeH5 패키지 (FreeRTOS + HAL 드라이버)

### 빌드

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### 플래시 (OpenOCD)

```bash
openocd -f interface/stlink.cfg \
        -f target/stm32h5x.cfg \
        -c "program STM32H5_DAC_ADC_Feedback.hex verify reset exit"
```

---

## 피드백 회로 설계 참고

```
             DAC/PWM 출력                    ADC 피드백
             ───────────┐               ┌─── PA0~PA3
                        │               │
              부하       ▼               │ 분압기 (필요 시)
              ┌─── [Op-Amp Buffer] ─── [Load] ──── GND
              │         PA4~PA7                  │
              │                                  │
              └──────────────────────────────────┘
                        폐루프 피드백
```

- 출력 전압이 3.3V 이상이면 **외부 op-amp 증폭 회로** 필요
- ADC 입력은 반드시 0 ~ 3.3V 범위; 분압기 사용 시 `VCTRL_RAW_TO_VOLT()` 스케일 수정

---

## 라이선스

MIT License
