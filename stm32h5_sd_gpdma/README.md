# STM32H5 SD Card with GPDMA1/2 프로젝트

UART 및 기타 인터럽트와 충돌 없이 SD 카드를 안정적으로 읽고 쓰기 위해
GPDMA1/2를 활용한 비동기 DMA 전송 구현 예제입니다.

---

## 하드웨어 환경

| 항목 | 값 |
|------|-----|
| MCU  | STM32H563xx / STM32H573xx |
| 클럭 | HSE 24MHz → PLL1 250MHz (SYSCLK) |
| SD 인터페이스 | SDMMC1 (4-bit 버스, 최대 50MHz) |
| DMA | GPDMA1 Ch0 (SDMMC1_RX), Ch1 (SDMMC1_TX) |
| UART | USART1 (디버그 로그, GPDMA2 Ch0 TX) |
| FatFS | 미들웨어 연동 |

---

## STM32CubeMX 설정

### 1. RCC (Clock)

```
RCC → HSE: Crystal/Ceramic Resonator (24MHz)
RCC → PLL Source: HSE
PLL1:
  - DIVM1 = 4   → VCO 입력 = 6MHz
  - MULN1 = 125 → VCO 출력 = 750MHz
  - DIVP1 = 3   → SYSCLK = 250MHz
  - DIVQ1 = 6   → PLL1Q = 125MHz (SDMMC 클럭 소스)
  - DIVR1 = 2   → PLL1R = 375MHz

Clock Configuration:
  SYSCLK  = 250MHz (PLL1P)
  HCLK    = 250MHz (AHB prescaler = 1)
  APB1CLK = 125MHz
  APB2CLK = 125MHz
  SDMMC1  클럭 소스 → PLL1Q (125MHz, CubeMX RCC 탭에서 설정)
```

### 2. SDMMC1

```
Connectivity → SDMMC1:
  Mode: SD 4-bit Wide bus

Parameter Settings:
  Clock Edge         : Rising Edge
  Clock Power Save   : Disable
  Bus Wide           : 4-bit Wide Bus
  Hardware Flow Ctrl : Disable
  Clock Div          : 2   (SDMMCclk / (2+2) = 125MHz / 4 ≈ 31.25MHz)
                          ※ SD 초기화는 400kHz 이하 필요 → HAL이 자동 처리
DMA Settings:
  Add DMA Request: SDMMC1_RX  → GPDMA1 Channel 0 (Direction: Periph→Memory)
  Add DMA Request: SDMMC1_TX  → GPDMA1 Channel 1 (Direction: Memory→Periph)

  각 DMA 설정:
    Mode            : Normal
    Source/Dest Inc : SDMMC=Fixed, Memory=Increment
    Data Width      : Word (32-bit)
    Burst Size      : 4 beats
    Priority        : High

NVIC Settings:
  SDMMC1 global interrupt    : Enable, Priority 5
  GPDMA1 Channel0 interrupt  : Enable, Priority 5
  GPDMA1 Channel1 interrupt  : Enable, Priority 5
```

### 3. USART1 (디버그)

```
Connectivity → USART1:
  Mode: Asynchronous
  Baud Rate : 115200
  Word Len  : 8 bits
  Parity    : None
  Stop Bits : 1

DMA Settings:
  Add DMA Request: USART1_TX → GPDMA2 Channel 0
    Direction : Memory→Periph
    Mode      : Normal
    Priority  : Low

NVIC:
  USART1 global interrupt  : Enable, Priority 6
  GPDMA2 Channel0 interrupt: Enable, Priority 6
```

### 4. NVIC 우선순위 요약

| 인터럽트 | Priority (Preempt) | 비고 |
|----------|--------------------|------|
| SysTick  | 15 (최저) | HAL Tick |
| GPDMA1 Ch0/1 | 5 | SDMMC DMA |
| SDMMC1 | 5 | SD 이벤트 |
| GPDMA2 Ch0 | 6 | UART TX DMA |
| USART1 | 6 | UART RX |

> **핵심**: SDMMC DMA 우선순위를 UART보다 높게 설정하여 전송 중 끊김 방지

### 5. GPIO 핀 매핑 (NUCLEO-H563ZI 기준)

```
SDMMC1_CK  → PC12
SDMMC1_CMD → PD2
SDMMC1_D0  → PC8
SDMMC1_D1  → PC9
SDMMC1_D2  → PC10
SDMMC1_D3  → PC11
SD_DETECT  → PG2  (GPIO Input, Pull-Up, 필요 시 EXTI 설정)

USART1_TX  → PA9
USART1_RX  → PA10
```

### 6. FatFS 미들웨어

```
Middleware → FatFS:
  Interface: SD Card

Configuration:
  USE_LFN          : Enabled with dynamic working buffer
  MAX_SS / MIN_SS  : 512
  FS_EXFAT         : Disable (필요 시 Enable)
  WORD_ACCESS      : 0 (정렬 접근)

FreeRTOS 미사용 시:
  _FS_REENTRANT    : 0
```

---

## 아키텍처 설명

```
[Application Layer]
        │  FatFS API (f_open / f_read / f_write)
        ▼
[FatFS Middleware]
        │  disk_read / disk_write (user_diskio.c)
        ▼
[SD Card Driver]  ← sd_card.c
        │  HAL_SD_ReadBlocks_DMA / HAL_SD_WriteBlocks_DMA
        ▼
[SDMMC1 Peripheral]
    ┌───┴───┐
[GPDMA1]   [SDMMC IRQ]
Ch0(RX)  Ch1(TX)
```

**GPDMA 사용의 장점:**
- CPU 개입 없이 SD ↔ SRAM 데이터 이동
- UART/TIM 등 인터럽트 지연 시간 최소화
- 전송 완료 콜백으로 비동기 처리
- Linked-List 모드로 연속 블록 전송 지원

---

## 빌드 환경

- STM32CubeIDE 1.15 이상
- STM32CubeH5 FW v1.3.0 이상
- ARM GCC 12.3.rel1
