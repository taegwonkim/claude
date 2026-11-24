# STM32CubeMX 설정 가이드 — STM32H743 + QCA7005 (SLAC 충전기 / 차량)

이 문서는 `STM32H7_QCA7005_PLC` 프로젝트의 하드웨어 설정을 **STM32CubeMX(또는 STM32CubeIDE의 .ioc 편집기)에서
그대로 재현하는 방법**과 **각 값을 그렇게 정한 이유**를 설명합니다.

- 대상 : NUCLEO-H743ZI2 (STM32H743ZIT6, LQFP144)
- 기준 : STM32CubeMX 6.14 / STM32CubeH7 펌웨어 패키지 1.12 (다른 버전도 메뉴 이름만 약간 다릅니다)
- 결과 : 저장소의 `Core/Src/main.c`, `stm32h7xx_hal_msp.c`, `stm32h7xx_it.c`, `Core/Inc/main.h` 와
  **같은 코드**가 생성됩니다. 손으로 작성한 코드는 모두 `USER CODE BEGIN / END` 구역 안에 있으므로
  다시 생성해도 지워지지 않습니다

---

## 목차
0. [사용 방법 두 가지](#0-사용-방법-두-가지)
1. [새 프로젝트 만들기](#1-새-프로젝트-만들기)
2. [핀 배치 한눈에 보기](#2-핀-배치-한눈에-보기)
3. [System Core — RCC, SYS, CORTEX_M7, GPIO, NVIC](#3-system-core)
4. [Analog — ADC1 (CP 전압 측정)](#4-analog--adc1-cp-전압-측정)
5. [Timers — TIM1 (CP PWM, 충전기)](#5-timers--tim1-cp-pwm-충전기)
6. [Connectivity — SPI1 (QCA7005), USART3 (로그)](#6-connectivity)
7. [Security — RNG](#7-security--rng)
8. [Clock Configuration](#8-clock-configuration)
9. [Project Manager](#9-project-manager)
10. [코드 생성 후 할 일](#10-코드-생성-후-할-일)
11. [충전기 / 차량 역할과 CubeMX 설정](#11-충전기--차량-역할과-cubemx-설정)
12. [자주 하는 실수](#12-자주-하는-실수)
13. [다른 보드·핀으로 옮길 때](#13-다른-보드핀으로-옮길-때)

---

## 0. 사용 방법 두 가지

### A. 제공된 `.ioc` 열기 (빠름)
1. `STM32H7_QCA7005_PLC.ioc` 를 더블클릭하거나, CubeIDE에서 프로젝트를 Import 한 뒤 `.ioc` 를 엽니다
2. 펌웨어 패키지 이전(migrate) 질문이 나오면 **Migrate** 를 선택합니다
3. 이 문서의 3~9장 표와 화면 값이 같은지 확인합니다
4. **Project → Generate Code** (`Alt+K`)

> `.ioc` 파일은 이 문서의 설정을 바탕으로 **직접 작성한 파일이고, 실제 CubeMX에서 열어 확인하지는 못했습니다.**
> 열었을 때 경고가 나오거나 값이 다르면 B 방법대로 이 문서의 표에 맞춰 고치면 됩니다.
> 코드는 `.ioc` 와 별개로 이미 빌드·테스트가 된 상태이므로, CubeMX를 쓰지 않아도 프로젝트는 그대로 동작합니다.

### B. 처음부터 직접 설정 (권장: 한 번은 직접 따라 해 보기)
1장부터 순서대로 진행합니다. 설정이 끝나면 10장에 따라 애플리케이션 파일을 붙입니다.

---

## 1. 새 프로젝트 만들기

1. **File → New Project** → **Board Selector** 탭
2. Commercial Part Number 에 `NUCLEO-H743ZI2` 입력 → 보드 선택 → **Next**
3. Project Name `STM32H7_QCA7005_PLC`, Targeted Project Type **STM32Cube** → **Finish**
4. **"Initialize all peripherals with their default Mode?" → No**

   | 선택 | 결과 |
   |---|---|
   | Yes | ETH, USB OTG, USART3, LED, 버튼 등 보드 기본 기능이 모두 켜집니다. 이 프로젝트에 필요 없는 ETH(PA7 등)·USB 코드가 생겨 정리가 번거롭습니다 |
   | **No** | 핀만 보드 이름이 붙은 상태로 비어 있습니다. 필요한 것만 켭니다 |

5. Pinout 화면에서 핀에 미리 할당된 것이 있으면 **Pinout → Clear Pinouts** 로 지우고 시작해도 됩니다
   (PH0/PH1, PA13/PA14 는 3장에서 다시 설정합니다)

---

## 2. 핀 배치 한눈에 보기

핀을 클릭해 기능(Signal)을 고르고, 오른쪽 클릭 → **Enter User Label** 로 **라벨**을 붙입니다.
**라벨은 코드와 연결되는 이름이므로 반드시 표와 똑같이** 입력하세요. CubeMX가 `main.h` 에
`QCA_CS_Pin`, `QCA_CS_GPIO_Port` 같은 매크로를 만들고, `app_conf.h` 가 이 매크로를 사용합니다.

| 핀 | Signal (CubeMX에서 선택) | User Label | 연결 대상 | 사용하는 역할 |
|---|---|---|---|---|
| PH0 | RCC_OSC_IN | — | ST-LINK MCO 8 MHz | 공통 |
| PH1 | RCC_OSC_OUT | — | (Bypass라 사용 안 함) | 공통 |
| PA13 / PA14 | SYS_JTMS-SWDIO / SYS_JTCK-SWCLK | — | ST-LINK SWD | 공통 |
| **PA5** | SPI1_SCK | `QCA_SCK` | QCA7005 SPI_CLK | 공통 |
| **PA6** | SPI1_MISO | `QCA_MISO` | QCA7005 SPI_MISO | 공통 |
| **PB5** | SPI1_MOSI | `QCA_MOSI` | QCA7005 SPI_MOSI | 공통 |
| **PD14** | GPIO_Output | `QCA_CS` | QCA7005 SPI_CS_L | 공통 |
| **PE13** | GPIO_EXTI13 | `QCA_INT` | QCA7005 INTR | 공통 |
| **PE14** | GPIO_Output | `QCA_RST` | QCA7005 RESET_L | 공통 |
| **PA3** | ADC1_INP15 | `CP_ADC` | CP 분압·레벨 시프트 출력 | 공통 |
| **PE9** | TIM1_CH1 | `CP_PWM` | CP ±12 V 드라이버 입력 | 충전기 |
| **PE11** | GPIO_Output | `CP_S2` | S2 스위치 (1.3 kΩ) 구동 | 차량 |
| PD8 | USART3_TX | `STLINK_RX` | ST-LINK VCP | 공통 |
| PD9 | USART3_RX | `STLINK_TX` | ST-LINK VCP | 공통 |
| PB0 | GPIO_Output | `LED_GREEN` | LD1 | 공통 |
| PE1 | GPIO_Output | `LED_YELLOW` | LD2 | 공통 |
| PB14 | GPIO_Output | `LED_RED` | LD3 | 공통 |

> PD8/PD9 라벨이 STLINK_RX/STLINK_TX 로 엇갈린 것은 ST-LINK 쪽 기준 이름이라서입니다(NUCLEO 보드 기본 라벨).

**핀 선택 이유**
- **SPI1 (PA5/PA6/PB5)** : NUCLEO의 Arduino 헤더(D13/D12/D11)에 나와 있어 QCA7005 모듈을 점퍼선으로 연결하기 쉽습니다.
  NUCLEO-H743ZI2 는 PA7 이 ETH 용으로 쓰여서 MOSI 는 PB5 를 씁니다
- **CS 를 GPIO(PD14)로** : QCA7005는 레지스터 읽기 한 번(명령 2바이트 + 데이터 2바이트)이나 버스트 전송 내내 CS가
  Low로 유지되어야 합니다. 하드웨어 NSS는 HAL 호출마다 CS를 올렸다 내릴 수 있어서 소프트웨어 CS를 씁니다
- **PE13 (EXTI13)** : 다른 EXTI 라인과 겹치지 않는 핀입니다. EXTI15_10 인터럽트 하나만 씁니다
- **PA3 (ADC12_INP15)** : Arduino A0 위치이고, ADC 전용 아날로그 입력이라 다른 기능과 겹치지 않습니다
- **PE9 (TIM1_CH1)** : TIM1은 고급 타이머라 출력 극성·브레이크 제어가 되고, Arduino 헤더에 나와 있습니다

---

## 3. System Core

### 3-1. RCC
**Pinout & Configuration → System Core → RCC**

| 항목 | 값 | 이유 |
|---|---|---|
| High Speed Clock (HSE) | **BYPASS Clock Source** | NUCLEO-H743ZI2 는 크리스털 대신 ST-LINK의 MCO 8 MHz 클럭을 PH0 으로 받습니다. Crystal 로 두면 HSE가 기동하지 않습니다 |
| Low Speed Clock (LSE) | Disable | RTC 를 쓰지 않습니다 |
| Parameter Settings → Supply Source | **PWR_LDO_SUPPLY** | NUCLEO-H743ZI2 의 전원 구성(LDO). 잘못 고르면 부팅 직후 멈춥니다 |
| Parameter Settings → Power Regulator Voltage Scale | **Power Regulator Voltage Scale 1** | SYSCLK 400 MHz 동작 조건 |

HSE 를 BYPASS 로 고르면 PH0 은 `RCC_OSC_IN` 으로 자동 할당됩니다.

### 3-2. SYS

| 항목 | 값 | 이유 |
|---|---|---|
| Debug | **Serial Wire** | ST-LINK SWD 디버깅. PA13/PA14 가 SWD 로 잡힙니다 |
| Timebase Source | **SysTick** | `HAL_GetTick()` 1 ms 틱. SLAC 타이머가 모두 이 값을 씁니다. RTOS를 쓰지 않으므로 TIMx 로 바꿀 필요가 없습니다 |

### 3-3. CORTEX_M7

| 항목 | 값 | 이유 |
|---|---|---|
| CPU ICache | **Enabled** | 성능 |
| CPU DCache | **Enabled** | 성능. SPI·ADC 모두 **폴링 방식(DMA 없음)** 이라 캐시 일관성 문제가 생기지 않습니다 |
| Cortex Memory Protection Unit → **Speculation default mode Settings** | **Enabled** | MPU 영역 0: 4 GB 전체를 접근 금지(서브리전 0x87 제외)로 두어, 사용하지 않는 주소로의 투기적(speculative) 읽기를 막는 ST 권장 기본 설정 |
| 그 밖의 MPU Region | 사용 안 함 | DMA 버퍼가 없으므로 non-cacheable 영역이 필요 없습니다 |

> 나중에 SPI를 DMA로 바꾸면 SPI 버퍼를 non-cacheable 영역(예: D2 SRAM)에 두고 MPU Region 1 을 추가하거나,
> 전송 전후로 `SCB_CleanDCache_by_Addr` / `SCB_InvalidateDCache_by_Addr` 를 호출해야 합니다.

### 3-4. GPIO
**System Core → GPIO → GPIO 탭**에서 핀별로 설정합니다.

| 핀 (라벨) | GPIO output level | GPIO mode | Pull | Max output speed | 이유 |
|---|---|---|---|---|---|
| PD14 (`QCA_CS`) | **High** | Output Push Pull | No pull | **High** | 시작할 때 CS 비활성(High). SPI 클럭에 맞춰 빠르게 토글 |
| PE14 (`QCA_RST`) | **Low** | Output Push Pull | No pull | Low | 부팅 직후 QCA7005를 리셋 상태로 둡니다. 드라이버가 20 ms 뒤 High 로 풉니다 |
| PE11 (`CP_S2`) | **Low** | Output Push Pull | No pull | Low | S2 열림(상태 B). 차량에서 V2G 계층이 닫습니다 |
| PB0 (`LED_GREEN`) | Low | Output Push Pull | No pull | Low | LED 꺼짐 |
| PE1 (`LED_YELLOW`) | Low | Output Push Pull | No pull | Low | LED 꺼짐 |
| PB14 (`LED_RED`) | Low | Output Push Pull | No pull | Low | LED 꺼짐 |
| PE13 (`QCA_INT`) | — | **External Interrupt Mode with Rising edge trigger detection** | **Pull-down** | — | QCA7005 INTR 는 High 활성입니다(Linux 드라이버도 상승 에지). 리셋 중 핀이 떠 있지 않도록 풀다운 |

SPI1, USART3, TIM1, ADC1 핀(AF/Analog)은 각 주변장치 설정의 **GPIO Settings** 탭에서 따로 설정합니다(6장 등).

### 3-5. NVIC
**System Core → NVIC**

| 항목 | 값 | 이유 |
|---|---|---|
| Priority Group | **4 bits for pre-emption priority** | CubeMX 기본값 |
| **EXTI line[15:10] interrupts** | **Enabled**, Preemption Priority **5**, Sub 0 | QCA7005 INTR. 핸들러는 플래그만 세우고(`HAL_GPIO_EXTI_Callback`), SPI 처리는 메인 루프에서 합니다 |
| Time base: System tick timer | Enabled, Priority **15** | HAL 기본값 |
| SPI1 / ADC / TIM1 / RNG / USART3 global interrupt | **사용 안 함** | 모두 폴링 방식 |

**Code generation 탭**: EXTI line[15:10] 의 "Generate IRQ handler", "Call HAL handler" 가 체크되어 있어야
`stm32h7xx_it.c` 에 `EXTI15_10_IRQHandler()` → `HAL_GPIO_EXTI_IRQHandler(QCA_INT_Pin)` 이 생성됩니다.

---

## 4. Analog — ADC1 (CP 전압 측정)

**Pinout & Configuration → Analog → ADC1 → IN15: IN15 Single-ended**
(PA3 이 `ADC1_INP15` 로 잡힙니다)

### Parameter Settings

| 그룹 | 항목 | 값 | 이유 |
|---|---|---|---|
| ADCs_Common_Settings | Mode | Independent mode | ADC1 하나만 사용 |
| ADC_Settings | Clock Prescaler | **Asynchronous clock mode divided by 2** | 커널 클럭 per_ck 64 MHz → 32 MHz (8장 참고) |
| | Resolution | **ADC 12-bit resolution** | CP 판정에는 12비트로 충분하고, 변환이 빨라 2 ms 동안 샘플을 많이 얻습니다 |
| | Scan Conversion Mode | Disabled | 채널 1개 |
| | **Continuous Conversion Mode** | **Enabled** | 한 번 시작하면 계속 변환합니다. `cp.c` 가 2 ms 동안 EOC 플래그를 보며 값을 읽습니다 |
| | Discontinuous Conversion Mode | Disabled | |
| | End Of Conversion Selection | End of single conversion | |
| | **Overrun behaviour** | **Overrun data overwritten** | 측정하지 않는 동안에도 변환이 계속 돌기 때문에, 덮어쓰기로 두어야 ADC가 멈추지 않습니다 |
| | Conversion Data Management Mode | Regular Conversion data stored in DR register only | DMA 없이 DR 을 직접 읽습니다 |
| | Low Power Auto Wait | Disabled | |
| ADC_Regular_ConversionMode | Enable Regular Conversions | Enable | |
| | Enable Regular Oversampling | Disable | |
| | Number Of Conversion | 1 | |
| | External Trigger Conversion Source | **Regular Conversion launched by software** | `HAL_ADC_Start()` 로 한 번 시작 |
| | Rank 1 → Channel | **Channel 15** | PA3 |
| | Rank 1 → Sampling Time | **8.5 Cycles** | 변환 1회 약 0.5~1 µs (8.5 + 6.5 사이클, 실리콘 리비전에 따라 ADC 클럭 내부 분주가 다름). 5 % 듀티(High 50 µs)에서도 High 구간 샘플을 수십 개 얻습니다. CP 분압 회로 출력 임피던스가 크면 16.5/32.5 Cycles 로 늘리세요 |
| | Rank 1 → Offset Number | No offset | |

**캘리브레이션**은 CubeMX가 생성하지 않으므로 `main.c` 의 `USER CODE BEGIN ADC1_Init 2` 에 넣어 두었습니다.
```c
/* USER CODE BEGIN ADC1_Init 2 */
if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
{
  Error_Handler();
}
/* USER CODE END ADC1_Init 2 */
```

**측정 방식**(`cp.c`): DWT 사이클 카운터로 정확히 2 ms(1 kHz PWM 2주기) 동안 변환값을 모아
양(+) 샘플 비율 = 듀티, 양 샘플 평균 = CP High 전압(상태 A~F 판정)을 구합니다.
ADC 입력 회로 환산값은 `app_conf.h` 의 `CP_ADC_ZERO_MV`, `CP_ADC_MV_PER_V` 로 맞춥니다.

---

## 5. Timers — TIM1 (CP PWM, 충전기)

**Pinout & Configuration → Timers → TIM1**

### Mode

| 항목 | 값 |
|---|---|
| Clock Source | **Internal Clock** |
| Channel1 | **PWM Generation CH1** (PE9 이 `TIM1_CH1` 로 잡힘. 다른 핀으로 잡히면 PE9 을 직접 클릭해 TIM1_CH1 지정) |
| 나머지 Channel / Combined / BKIN | Disable |

### Parameter Settings

| 그룹 | 항목 | 값 | 이유 |
|---|---|---|---|
| Counter Settings | **Prescaler (PSC)** | **199** | 타이머 클럭 200 MHz ÷ (199+1) = **1 MHz** (1 µs 단위) |
| | Counter Mode | Up | |
| | **Counter Period (ARR)** | **999** | 1 MHz ÷ (999+1) = **1 kHz** (IEC 61851-1 CP 주파수) |
| | Internal Clock Division | No Division | |
| | Repetition Counter | 0 | |
| | **auto-reload preload** | **Enable** | 주기 중간에 값이 바뀌어도 다음 주기부터 적용 |
| Trigger Output (TRGO) | Master/Slave Mode | Disable | |
| | Trigger Event Selection | Reset | |
| Break And Dead Time | BRK / BRK2 State | Disable | 하드웨어 차단 신호를 쓰지 않습니다(필요하면 BKIN 으로 비상 정지 연결 가능) |
| | Automatic Output State | Disable | |
| PWM Generation Channel 1 | **Mode** | **PWM mode 1** | CNT < CCR 동안 High |
| | **Pulse** | **1000** | 시작값 100 % = **+12 V DC**(상태 A/B1, 통신 없음). CCR > ARR 이면 출력이 계속 High |
| | Output compare preload | Enable | |
| | Fast Mode | Disable | |
| | CH Polarity | **High** | High = +12 V 쪽. CP 드라이버가 반전형이면 Low 로 바꾸세요 |
| | CH Idle State | Reset | |

**듀티 계산**: CCR = 듀티(0.1 % 단위). `cp_evse_set_duty(50)` → CCR 50 → **5 %**
(디지털 통신 요청, ISO 15118 / DIN 70121), `cp_evse_set_duty(1000)` → +12 V DC.

> **타이머 클럭 200 MHz 인 이유**: APB2 = 100 MHz 이고 APB 분주가 1이 아니면 타이머 클럭은 2배(200 MHz)가 됩니다.
> Clock Configuration 화면의 "To APB2 Timer clocks" 값이 200 MHz 인지 확인하세요.

**PWM 시작**은 CubeMX가 하지 않습니다. 충전기 역할일 때만 `cp_init()` 에서 `HAL_TIM_PWM_Start()` 를 호출합니다
(고급 타이머 TIM1 의 MOE 비트도 이 함수가 켭니다).

---

## 6. Connectivity

### 6-1. SPI1 (QCA7005)

**Connectivity → SPI1**

#### Mode
| 항목 | 값 | 이유 |
|---|---|---|
| Mode | **Full-Duplex Master** | MCU가 클럭을 내고, MOSI/MISO 동시 사용 |
| Hardware NSS Signal | **Disable** | CS 는 PD14 GPIO 로 직접 제어 (2장 참고) |

#### Parameter Settings
| 그룹 | 항목 | 값 | 이유 |
|---|---|---|---|
| Basic Parameters | Frame Format | Motorola | |
| | **Data Size** | **8 Bits** | 드라이버가 바이트 단위로 보냅니다. 16비트 명령은 상위 바이트부터(빅엔디언) 보냅니다 |
| | **First Bit** | **MSB First** | QCA7000/7005 SPI 규격 |
| Clock Parameters | **Prescaler (for Baud Rate)** | **16** | SPI 커널 클럭 80 MHz ÷ 16 = **5.0 MBits/s**. QCA7005 최대 16 MHz, Linux 드라이버 기본 8 MHz. 배선이 짧고 깨끗하면 **8 (10 MBits/s)** 로 올려도 됩니다 |
| | **Clock Polarity (CPOL)** | **High** | **SPI 모드 3** (Linux `qca_spi.c` 의 `SPI_MODE_3`) |
| | **Clock Phase (CPHA)** | **2 Edge** | 〃 |
| Advanced Parameters | CRC Calculation | Disabled | |
| | NSSP Mode | Disabled | |
| | NSS Signal Type | Software | |
| | Fifo Threshold | Fifo Threshold 01 Data | 8비트 1개 단위로 처리 |
| | Tx/Rx CRC Initialization Pattern | All Zero Pattern | |
| | Master SS Idleness / Master Inter Data Idleness | 00 Cycle | 바이트 사이 지연 없음 |
| | Master Receiver Auto Suspend | Disable | |
| | **Master Keep IO State** | **Master Keep IO State Enable** | ⚠ **중요.** H7 HAL은 전송이 끝날 때마다 SPI를 끕니다. Disable 이면 그 사이 SCK 핀이 떠서 모드 3의 유휴 레벨(High)이 깨집니다. 이 드라이버는 CS를 Low로 둔 채 HAL 호출을 여러 번 나눠 하므로(명령 → 데이터) 반드시 Enable |
| | IO Swap | Disabled | |

#### GPIO Settings (SPI1 탭 하단)
| 핀 | Pull | Maximum output speed | 이유 |
|---|---|---|---|
| PA5 SCK (`QCA_SCK`) | **Pull-up** | **High** | 모드 3 유휴 레벨 High 유지 보조 |
| PA6 MISO (`QCA_MISO`) | **Pull-up** | **High** | QCA7005 가 리셋 중일 때 입력이 떠 있지 않게 |
| PB5 MOSI (`QCA_MOSI`) | No pull | **High** | |

#### 왜 DMA·인터럽트를 쓰지 않나
- SLAC 메시지는 60~130 바이트라 5 MHz 에서 0.2 ms 이하입니다. 폴링으로 충분합니다
- DMA를 쓰면 D-Cache 일관성(3-3 참고)과 D1/D2 메모리 배치를 신경 써야 합니다. 처리량이 필요해지면 그때 바꾸면 됩니다

### 6-2. USART3 (로그, ST-LINK VCP)

**Connectivity → USART3 → Mode: Asynchronous**, Hardware Flow Control: Disable

| 항목 | 값 |
|---|---|
| Baud Rate | **115200** Bits/s |
| Word Length | 8 Bits (including Parity) |
| Parity | None |
| Stop Bits | 1 |
| Data Direction | Receive and Transmit |
| Over Sampling | 16 Samples |
| Single Sample | Disable |
| Clock Prescaler | 1 |
| Fifo Mode | **Disable** |
| Txfifo / Rxfifo Threshold | 1 eighth full configuration |

핀은 PD8(TX)/PD9(RX), 라벨은 보드 기본값 `STLINK_RX` / `STLINK_TX` 를 그대로 둡니다.
`printf` 는 `main.c` 의 `__io_putchar()` (USER CODE 0) 로 USART3 에 출력됩니다.

---

## 7. Security — RNG

**Security → RNG → Activated** 체크

| 항목 | 값 | 이유 |
|---|---|---|
| Clock Error Detection | **Enable** | RNG 클럭(HSI48)이 너무 느리면 오류로 알립니다 |

**용도**: SLAC 의 **NMK(네트워크 키, 16바이트)**, **RunID(8바이트)**, 사운딩의 RND 값.
NMK 는 충전 세션마다 새로 만들어야 다른 차량이 같은 네트워크에 들어오지 못하므로, 하드웨어 난수를 씁니다.

---

## 8. Clock Configuration

**Clock Configuration 탭**에서 아래 값을 입력합니다. 빨간 칸이 없어야 합니다.

```
 HSE 8 MHz (Bypass) ─┬─ PLL Source Mux = HSE
                     │
                     └─ DIVM1 /4 ─ 2 MHz ─ DIVN1 x400 ─ VCO 800 MHz ─┬─ DIVP1 /2 → 400 MHz ── SYSCLK
                                                                    ├─ DIVQ1 /10 → 80 MHz ── SPI1,2,3 커널 클럭
                                                                    └─ DIVR1 /2 → 400 MHz (사용 안 함)
 SYSCLK 400 ─ D1CPRE /1 → CPU 400 MHz ─ HPRE /2 → HCLK 200 MHz (AXI/AHB)
                                         ├─ D1PPRE  /2 → APB3 100 MHz
                                         ├─ D2PPRE1 /2 → APB1 100 MHz (USART3), APB1 타이머 200 MHz
                                         ├─ D2PPRE2 /2 → APB2 100 MHz,          APB2 타이머 200 MHz (TIM1)
                                         └─ D3PPRE  /2 → APB4 100 MHz
 HSI 64 MHz  ── CKPER Mux = HSI → per_ck 64 MHz ── ADC Mux = per_ck (CLKP) → ADC 64 MHz (÷2 = 32 MHz)
 HSI48 48 MHz ─ RNG Mux = HSI48 → RNG 48 MHz
```

| 위치 | 항목 | 값 | 이유 |
|---|---|---|---|
| Input frequency | HSE | **8** MHz | ST-LINK MCO. 이 값이 `stm32h7xx_hal_conf.h` 의 `HSE_VALUE` 가 됩니다 |
| PLL Source Mux | | **HSE** | 정확한 클럭(UART 보레이트, SLAC 타이밍) |
| PLL1 | DIVM1 | **/4** | PLL 입력 2 MHz (RANGE_1: 2~4 MHz) |
| | DIVN1 | **x400** | VCO 800 MHz (Wide VCO) |
| | DIVP1 | **/2** | 400 MHz |
| | DIVQ1 | **/10** | **80 MHz → SPI1** |
| | DIVR1 | /2 | |
| System Clock Mux | | **PLLCLK** | |
| | D1CPRE | /1 | CPU 400 MHz |
| | HPRE | **/2** | HCLK 200 MHz (H743 AXI 최대 240 MHz) |
| | D1PPRE, D2PPRE1, D2PPRE2, D3PPRE | **/2** | 모든 APB 100 MHz |
| SPI1,2,3 Clock Mux | | **PLL1Q** | 80 MHz (커널 클럭) |
| CKPER Mux | | **HSI** | per_ck 64 MHz |
| ADC Clock Mux | | **per_ck (CLKP)** | ⚠ CubeMX 기본값은 **PLL2P** 입니다. PLL2 를 켜지 않으면 ADC에 클럭이 없어 `HAL_ADC_Init` 이 실패합니다. PLL2 를 따로 설정하지 않으려고 per_ck 를 씁니다 |
| RNG Clock Mux | | **HSI48** | HSI48 오실레이터가 자동으로 켜집니다 |
| USART2,3,4,5,7,8 Clock Mux | | **PCLK1** (D2PCLK1) | 100 MHz, 115200 bps 오차 무시 가능 |

> 400 MHz 는 H743 rev V 최대(480 MHz)보다 낮아 Voltage Scale 1 로 충분하고, 기존 LAN8770 프로젝트와 같은 값입니다.
> 480 MHz 로 올리려면 VOS0 과 DIVN1 x480 이 필요하며, 이때 SPI 커널 클럭이 바뀌므로 Prescaler 를 다시 계산하세요.

**CubeMX가 생성하는 위치**
- `SystemClock_Config()` : HSE/HSI/HSI48, PLL1, 버스 분주
- `PeriphCommonClock_Config()` : CKPER = HSI (여러 주변장치가 공유하는 클럭)
- 각 `HAL_xxx_MspInit()` 의 "Initializes the peripherals clock" : SPI1 = PLL1Q, ADC = CLKP, RNG = HSI48, USART3 = PCLK1

CubeMX 버전에 따라 ADC 클럭 선택이 `PeriphCommonClock_Config()` 로 들어가기도 합니다. 어느 쪽이든 동작은 같습니다.

---

## 9. Project Manager

### Project 탭
| 항목 | 값 | 이유 |
|---|---|---|
| Project Name | `STM32H7_QCA7005_PLC` | `.ioc` 파일 이름과 같게 |
| Project Location | 저장소 루트(이 폴더의 상위) | |
| Application Structure | Advanced | |
| **Generate Under Root** | **체크** | `Core/`, `Drivers/` 가 프로젝트 폴더 바로 아래에 생성됩니다 |
| Toolchain / IDE | **STM32CubeIDE** | `.project`, `.cproject` 와 맞춤 |
| Minimum Heap Size | 0x1000 | `printf` 정도만 사용 |
| Minimum Stack Size | 0x1000 | SLAC 상태 머신은 정적 메모리만 씁니다 |
| Firmware Package | STM32Cube FW_H7 (최신) | |

### Code Generator 탭
| 항목 | 값 | 이유 |
|---|---|---|
| STM32Cube MCU packages and embedded software packs | **Copy only the necessary library files** | 저장소 크기를 줄입니다 |
| Generate peripheral initialization as a pair of '.c/.h' files per peripheral | **체크 해제** | 초기화 코드가 모두 `main.c` 에 있는 구조를 유지 |
| Backup previously generated files when re-generating | 해제 (선택) | git 으로 관리하므로 |
| **Keep User Code when re-generating** | **체크** | ⚠ `USER CODE` 구역의 코드를 보존. 해제하면 애플리케이션 연결 코드가 지워집니다 |
| Delete previously generated files when not re-generated | 체크 | 주변장치를 끄면 해당 코드도 정리 |
| Set all free pins as analog | 해제 | 보드의 다른 회로에 영향 없도록 |
| Enable Full Assert | 해제 | |

### Advanced Settings 탭
| 항목 | 값 |
|---|---|
| Driver Selector | 모든 주변장치 **HAL** |
| Generated Function Calls 순서 | `SystemClock_Config` → `MX_GPIO_Init` → `MX_USART3_UART_Init` → `MX_SPI1_Init` → `MX_ADC1_Init` → `MX_TIM1_Init` → `MX_RNG_Init` |
| Do Not Generate Function Call / Visibility(Static) | 모두 기본값 (호출 생성, static) |

**순서 이유**: GPIO 를 먼저 초기화해야 CS=High, RESET_L=Low 가 SPI 초기화보다 앞서 정해집니다.
USART3 를 일찍 초기화하면 이후 오류도 로그로 볼 수 있습니다.

---

## 10. 코드 생성 후 할 일

> **lwIP 와 EXI 코덱은 CubeMX 로 생성하지 않습니다.** `.ioc` 에서 LWIP 미들웨어를 켜지 마세요.
> `Middlewares/Third_Party/LwIP` (IPv6 전용 설정 `V2G/lwipopts.h`)와 `Middlewares/Third_Party/cbv2g` 는
> 이 저장소가 직접 관리하는 파일이고, CubeMX 가 만든 적이 없으므로 다시 생성해도 지워지지 않습니다.
> CubeIDE 의 링커 옵션 **Use float with printf from newlib-nano** 는 켜 두었습니다(V2G 로그의 전압·전류 출력).

### 10-1. 이 저장소에서 생성한 경우 (A 방법)
`USER CODE` 구역이 이미 채워져 있으므로 **추가 작업이 없습니다.** 생성 후 `git diff` 로 바뀐 내용을 확인하세요.
CubeMX 버전 차이로 주석이나 순서가 조금 바뀌는 것은 정상입니다.

### 10-2. 빈 프로젝트에서 새로 생성한 경우 (B 방법)
1. **파일 복사** (이 저장소 → 새 프로젝트)
   - `PLC/` 폴더 전체 → 프로젝트 루트
   - `Core/Src/` : `app_plc.c`, `app_evse.c`, `app_ev.c`, `cp.c`, `qca7k_port_stm32.c`
   - `Core/Inc/` : `app_conf.h`, `app_plc.h`, `cp.h`, `qca7k_port_stm32.h`
2. **CubeIDE 경로 추가**
   - `PLC` 폴더 오른쪽 클릭 → **Add/remove include path...** → Debug/Release 모두 체크
   - **Project → Properties → C/C++ General → Paths and Symbols → Source Location → Add Folder... → `PLC`**
3. **USER CODE 구역 채우기**

`Core/Inc/main.h`
```c
/* USER CODE BEGIN Includes */
#include "app_conf.h"
/* USER CODE END Includes */
...
/* USER CODE BEGIN EFP */
extern SPI_HandleTypeDef  hspi1;
extern UART_HandleTypeDef huart3;
extern RNG_HandleTypeDef  hrng;
extern ADC_HandleTypeDef  hadc1;
extern TIM_HandleTypeDef  htim1;
/* USER CODE END EFP */
```

`Core/Src/main.c`
```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "app_plc.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 0 */
int __io_putchar(int ch)            /* printf -> USART3 */
{
  uint8_t c = (uint8_t)ch;
  HAL_UART_Transmit(&huart3, &c, 1, 10);
  return ch;
}
/* USER CODE END 0 */

  /* USER CODE BEGIN SysInit */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* DWT: CP 2 ms 측정 창 */
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  /* USER CODE END SysInit */

  /* USER CODE BEGIN ADC1_Init 2 */
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END ADC1_Init 2 */

  /* USER CODE BEGIN 2 */
  APP_PLC_Init();
  /* USER CODE END 2 */

    /* USER CODE BEGIN 3 */
    APP_PLC_Process();
  }
  /* USER CODE END 3 */

  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
  while (1) {}
  /* USER CODE END Error_Handler_Debug */
```

`Core/Src/stm32h7xx_it.c` (선택: HardFault 시 빨간 LED)
```c
  /* USER CODE BEGIN HardFault_IRQn 0 */
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
  /* USER CODE END HardFault_IRQn 0 */
```

`HAL_GPIO_EXTI_Callback()` 은 `qca7k_port_stm32.c` 에 있으므로 따로 추가하지 않습니다.

4. **확인**
   - `Core/Inc/stm32h7xx_hal_conf.h` 의 `HSE_VALUE` 가 **8000000** 인지 확인 (8장 Input frequency 값)
   - 같은 파일에서 `HAL_ADC_MODULE_ENABLED`, `HAL_RNG_MODULE_ENABLED`, `HAL_SPI_MODULE_ENABLED`,
     `HAL_TIM_MODULE_ENABLED`, `HAL_UART_MODULE_ENABLED` 가 켜져 있는지 확인
   - **Project → Build Project** 가 경고 없이 끝나는지 확인

---

## 11. 충전기 / 차량 역할과 CubeMX 설정

**CubeMX 설정(.ioc)은 한 개를 두 역할이 함께 씁니다.** 역할은 코드에서만 고릅니다.

```c
/* Core/Inc/app_conf.h */
#define PLC_ROLE PLC_ROLE_EVSE      /* 충전기 */
/* #define PLC_ROLE PLC_ROLE_EV */  /* 차량   */
```

| 주변장치 | 충전기 (EVSE) | 차량 (EV) |
|---|---|---|
| SPI1 / QCA_CS / QCA_INT / QCA_RST | 사용 | 사용 |
| ADC1 (PA3, CP 측정) | 사용 — 차량 연결(9 V/6 V) 감지 | 사용 — 5 % PWM, 상태 B/C 감지 |
| TIM1 CH1 (PE9) | **PWM 출력** (`cp_init` 에서 시작) | 초기화만 되고 **시작하지 않음** (출력 꺼짐) |
| PE11 (`CP_S2`) | Low 로 유지 (사용 안 함) | S2 스위치 구동 |
| RNG | NMK·RunID 생성 | RunID·개인 NMK 생성 |

**보드를 역할별로 따로 만든다면** (선택 사항)
- 차량 보드: TIM1 Channel1 을 Disable 하고 PE9 를 Reset_State 로 두어도 됩니다.
  이때 `MX_TIM1_Init` 이 생성되지 않으므로 `main.h`의 `extern TIM_HandleTypeDef htim1;` 은 빼고,
  `cp.c` 의 TIM1 코드는 이미 `#if PLC_ROLE == PLC_ROLE_EVSE` 로 막혀 있습니다
- 충전기 보드: PE11(`CP_S2`) 을 빼도 됩니다. 단 `app_conf.h` 의 `CP_S2_PORT/PIN` 매크로는 남아 있어야 하므로
  핀만 다른 빈 핀으로 옮기는 편이 간단합니다

---

## 12. 자주 하는 실수

| 증상 | 원인 | 확인할 설정 |
|---|---|---|
| 로그 글자가 깨짐, `HAL_Delay` 가 너무 느림/빠름 | `HSE_VALUE` 가 25 MHz (기본값) | 8장 Input frequency 8 MHz, `stm32h7xx_hal_conf.h` |
| 부팅 직후 `SystemClock_Config` 에서 멈춤 | HSE 를 Crystal 로 설정, 또는 Supply Source 오류 | 3-1 BYPASS, PWR_LDO_SUPPLY |
| `[QCA] no signature (read 0x0000)` 반복 | SPI 모드 틀림, 배선, QCA 리셋 유지 | 6-1 CPOL High / CPHA 2 Edge, PE14 가 풀렸는지, MISO/MOSI 교차 |
| `[QCA] no signature (read 0xFFFF)` 반복 | MISO 가 떠 있음 / 칩 전원 없음 | QCA7005 전원 3.3 V, MISO 배선 |
| 시그니처가 가끔만 맞음 | SPI 클럭 과다, 긴 점퍼선 | Prescaler 16 → 32, 배선 단축 |
| 레지스터 읽기는 되는데 프레임 송수신 오류 | Master Keep IO State 가 Disable | 6-1 ⚠ 항목 |
| `HAL_ADC_Init` 에서 Error_Handler | ADC 커널 클럭 없음 (PLL2P 선택, PLL2 꺼짐) | 8장 ADC Clock Mux = per_ck |
| RNG 가 HAL_ERROR, NMK 가 매번 비슷함 | HSI48 꺼짐 / RNG 클럭 미선택 | 8장 RNG Mux HSI48 |
| CP PWM 이 안 나옴 (충전기) | Channel1 이 PWM Generation 이 아님, 역할이 EV | 5장, `PLC_ROLE` |
| CP 듀티가 1 kHz 가 아님 | APB2 분주와 타이머 클럭 계산 착오 | 5장 Prescaler 199 / Period 999, APB2 Timer 200 MHz |
| QCA INT 인터럽트가 안 들어옴 | EXTI15_10 NVIC 미활성, Falling edge 선택 | 3-4, 3-5 (인터럽트가 없어도 20 ms 폴링으로 동작은 합니다) |
| 코드 재생성 후 동작 안 함 | Keep User Code 해제, USER CODE 밖에 코드 작성 | 9장, 10-2 |

---

## 13. 다른 보드·핀으로 옮길 때

1. CubeMX 에서 핀만 옮기고 **User Label 은 그대로** 둡니다 (`QCA_CS`, `QCA_INT`, …)
2. 다시 생성하면 `main.h` 의 `xxx_Pin` / `xxx_GPIO_Port` 가 바뀌고, `app_conf.h` 와 드라이버는 그 매크로를 따라갑니다
3. 단, 다음은 직접 맞춰야 합니다
   - `QCA_INT` 를 EXTI 10~15 가 아닌 핀으로 옮기면 `stm32h7xx_it.c` 에 생성되는 IRQ 핸들러 이름이 바뀝니다
     (CubeMX가 알아서 생성하므로 코드 수정은 없지만 NVIC 에서 해당 EXTI 라인을 켜야 합니다)
   - SPI 를 SPI2/3 등으로 바꾸면 `hspi1` 이름이 바뀌므로 `qca7k_port_stm32.c` 와 `main.h` 의 `extern` 을 고칩니다
   - ADC 채널이나 타이머를 바꾸면 `cp.c` 의 `hadc1`, `htim1`, `TIM_CHANNEL_1` 을 고칩니다
4. **다른 STM32 계열**(F4, G4 등)로 옮길 때는 `PLC/` 폴더는 그대로 쓰고 `qca7k_port_stm32.c`, `cp.c` 만 그 계열 HAL에 맞춥니다.
   H7 전용 항목(Master Keep IO State, D-Cache, MPU, per_ck)은 다른 계열에는 없습니다
