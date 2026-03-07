# STM32H5 FileX + GPDMA1/2 프로젝트 설정 가이드

## 개요
- MCU: STM32H563xx / STM32H573xx (STM32H5 시리즈)
- FileX: RTOS 없이 SD 카드 읽기/쓰기
- GPDMA1: SDMMC1 DMA 전송 (SD 카드)
- GPDMA2: USART1 DMA 전송 (디버그 UART)

---

## STM32CubeMX 설정

### 1. Clock Configuration

```
HSE: 외부 크리스탈 25MHz (보드에 따라 조정)
PLL1:
  - PLL1 Source: HSE
  - PLL1M: 5    → VCO Input = 5MHz
  - PLL1N: 100  → VCO Output = 500MHz
  - PLL1P: 2    → PLL1P = 250MHz (System Clock)
  - PLL1Q: 4    → PLL1Q = 125MHz (SDMMC, USB)
  - PLL1R: 2    → PLL1R = 250MHz

System Clock: PLL1P = 250MHz
AHB  Prescaler: /1  → HCLK  = 250MHz
APB1 Prescaler: /4  → PCLK1 = 62.5MHz
APB2 Prescaler: /2  → PCLK2 = 125MHz
APB3 Prescaler: /4  → PCLK3 = 62.5MHz
```

---

### 2. SDMMC1 Configuration

**Pinout & Configuration 탭:**
```
Mode: SD 4 bits Wide bus

Parameter Settings:
  - Clock Edge: Rising
  - Clock Power Save: Disable
  - Bus Wide: 4 bits
  - Hardware Flow Control: Disable
  - SDMMC Clock divider bypass: Disable
  - Clock Divide Factor (CLKDIV): 4   → SD Clock = PLL1Q / (2*(4+1)) = 12.5MHz (초기화)
  - Init Clock Divide Factor: 118     → 초기화 시 ~400kHz
```

**DMA Settings 탭:**
```
DMA Request: SDMMC1 (TX)
  - DMA: GPDMA1
  - Channel: Channel 0
  - Direction: Memory To Peripheral
  - Priority: High
  - Src Data Width: Word (32-bit)
  - Dst Data Width: Word (32-bit)
  - Src Burst Length: 4
  - Dst Burst Length: 4

DMA Request: SDMMC1 (RX)
  - DMA: GPDMA1
  - Channel: Channel 1
  - Direction: Peripheral To Memory
  - Priority: High
  - Src Data Width: Word (32-bit)
  - Dst Data Width: Word (32-bit)
  - Src Burst Length: 4
  - Dst Burst Length: 4
```

**NVIC Settings 탭:**
```
SDMMC1 global interrupt: Enable, Priority: 5
GPDMA1 Channel0 global interrupt: Enable, Priority: 5
GPDMA1 Channel1 global interrupt: Enable, Priority: 5
```

**GPIO Pins (보드에 따라 조정):**
```
PC8  → SDMMC1_D0  (AF12)
PC9  → SDMMC1_D1  (AF12)
PC10 → SDMMC1_D2  (AF12)
PC11 → SDMMC1_D3  (AF12)
PC12 → SDMMC1_CK  (AF12)
PD2  → SDMMC1_CMD (AF12)
PD3  → SD_DETECT  (GPIO Input, Pull-Up)  ← 카드 감지 핀 (선택)
```

---

### 3. USART1 Configuration

**Pinout & Configuration 탭:**
```
Mode: Asynchronous

Parameter Settings:
  - Baud Rate: 115200
  - Word Length: 8 Bits
  - Parity: None
  - Stop Bits: 1
  - Data Direction: Receive and Transmit
  - Over Sampling: 16 Samples
```

**DMA Settings 탭:**
```
DMA Request: USART1_TX
  - DMA: GPDMA2
  - Channel: Channel 0
  - Direction: Memory To Peripheral
  - Priority: Low
  - Src Data Width: Byte
  - Dst Data Width: Byte
  - Src Burst Length: 1
  - Dst Burst Length: 1

DMA Request: USART1_RX
  - DMA: GPDMA2
  - Channel: Channel 1
  - Direction: Peripheral To Memory
  - Priority: Low
  - Src Data Width: Byte
  - Dst Data Width: Byte
  - Src Burst Length: 1
  - Dst Burst Length: 1
```

**NVIC Settings 탭:**
```
USART1 global interrupt: Enable, Priority: 6
GPDMA2 Channel0 global interrupt: Enable, Priority: 6
GPDMA2 Channel1 global interrupt: Enable, Priority: 6
```

**GPIO Pins:**
```
PA9  → USART1_TX (AF7)
PA10 → USART1_RX (AF7)
```

---

### 4. GPDMA1 Configuration (Pinout & Configuration → DMA → GPDMA1)

```
Channel 0:
  - Request: SDMMC1           (TX 방향)
  - Direction: Memory-to-Peripheral
  - Circular Mode: Disable
  - Src Address Increment: Enable
  - Dst Address Increment: Disable
  - Data Width (Src/Dst): Word/Word
  - Burst (Src/Dst): 4 beats / 4 beats
  - Priority: High
  - Transfer Event Mode: At block level

Channel 1:
  - Request: SDMMC1           (RX 방향)
  - Direction: Peripheral-to-Memory
  - Circular Mode: Disable
  - Src Address Increment: Disable
  - Dst Address Increment: Enable
  - Data Width (Src/Dst): Word/Word
  - Burst (Src/Dst): 4 beats / 4 beats
  - Priority: High
  - Transfer Event Mode: At block level
```

---

### 5. GPDMA2 Configuration (GPDMA2)

```
Channel 0:
  - Request: USART1_TX
  - Direction: Memory-to-Peripheral
  - Src Address Increment: Enable
  - Dst Address Increment: Disable
  - Data Width: Byte/Byte
  - Priority: Low

Channel 1:
  - Request: USART1_RX
  - Direction: Peripheral-to-Memory
  - Src Address Increment: Disable
  - Dst Address Increment: Enable
  - Data Width: Byte/Byte
  - Priority: Low
```

---

### 6. NVIC 우선순위 요약 (Priority Group: 4-bit Preemption)

| Interrupt              | Preempt Priority | Sub Priority | 용도                  |
|------------------------|------------------|--------------|-----------------------|
| SysTick                | 15               | 0            | HAL 타이머            |
| SDMMC1                 | 5                | 0            | SD카드 이벤트         |
| GPDMA1 Channel0        | 5                | 0            | SDMMC TX DMA          |
| GPDMA1 Channel1        | 5                | 0            | SDMMC RX DMA          |
| USART1                 | 6                | 0            | UART 이벤트           |
| GPDMA2 Channel0        | 6                | 0            | UART TX DMA           |
| GPDMA2 Channel1        | 6                | 0            | UART RX DMA           |

> **포인트:** SDMMC/GPDMA1 우선순위(5)를 UART/GPDMA2(6)보다 높게 설정하여
> SD 전송 중 UART 인터럽트가 SD DMA를 방해하지 않도록 함.

---

### 7. FileX Middleware 설정

**Middleware & Software Packs → FileX:**
```
Mode: Enable

Configuration:
  - Default Sector Size: 512
  - Max Long File Name Length: 256
  - Max Path Length: 512
  - Default Cache Size: 512 bytes (필요시 증가)
```

FileX 드라이버는 별도 구현 (fx_stm32_sd_driver.c 참조).

---

### 8. Project Settings

**Project Manager 탭:**
```
Project Name: STM32H5_FileX_GPDMA
Toolchain: STM32CubeIDE (Makefile / IAR도 가능)
Minimum Heap Size: 0x1000  (4KB)
Minimum Stack Size: 0x800  (2KB)

Advanced Settings:
  - SD: HAL 드라이버 (HAL_SD_*)
  - FileX: ST 미들웨어 포함
```

---

## 핵심 설계 원칙

```
1. GPDMA1  →  SDMMC1 전용
   - SD 카드 읽기/쓰기 시 CPU 개입 없이 DMA로 직접 메모리 전송
   - 512바이트 섹터 단위 블록 전송에 최적화

2. GPDMA2  →  USART1 전용
   - UART 송수신을 별도 DMA 컨트롤러로 분리
   - SDMMC DMA 전송과 버스 경합 없음

3. 인터럽트 충돌 방지 전략
   - GPDMA1/GPDMA2 채널을 물리적으로 분리
   - 우선순위 계층으로 중요도 구분
   - FileX 드라이버에서 DMA 완료 플래그 폴링으로 타이밍 보장

4. FileX without RTOS
   - ThreadX 없이 standalone 모드로 동작
   - DMA 완료 대기: HAL_SD_GetCardState() 폴링 방식
   - Timeout 처리로 행(hang) 방지
```
