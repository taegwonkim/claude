# STM32CubeMX 설정 가이드
# STM32H563ZI – SD 카드(SDMMC1 IDMA) + USART3(GPDMA1) 충돌 없는 구성

대상 보드: **NUCLEO-H563ZI**
IDE: STM32CubeIDE 1.16 이상 / HAL 드라이버

---

## 1. 프로젝트 생성

| 항목 | 값 |
|------|-----|
| MCU | STM32H563ZI |
| Project Name | STM32H5_SD_GPDMA |
| Toolchain | STM32CubeIDE (GCC) |
| Heap Size | 0x2000 (8 KB) |
| Stack Size | 0x1000 (4 KB) |

---

## 2. 클럭 설정 (Clock Configuration)

```
RCC → HSE : Crystal/Ceramic Resonator (NUCLEO: 8 MHz)
RCC → PLL Source : HSE

PLL1 설정:
  PLLM = 1
  PLLN = 125
  PLLP = 4   → SYSCLK = 250 MHz
  PLLQ = 10  → PLL1Q  = 100 MHz  ← SDMMC1 커널 클럭
  PLLR = 2   → PLL1R  = 500 MHz

System Clock Mux → PLL1P (250 MHz)
AHB  Prescaler  → /1  → HCLK  = 250 MHz
APB1 Prescaler  → /2  → PCLK1 = 125 MHz  (USART3 소속)
APB2 Prescaler  → /2  → PCLK2 = 125 MHz
APB3 Prescaler  → /2  → PCLK3 = 125 MHz

Flash Latency → 5 WS  (250 MHz, VOS0)
Power → VOS0 (최고 성능)

SDMMC1 Kernel Clock → PLL1Q (100 MHz)
```

---

## 3. SDMMC1 설정 (SD 카드 IDMA)

> STM32H5 SDMMC는 전용 Internal DMA(IDMA)를 내장.
> 별도 GPDMA 채널 없이 `HAL_SD_ReadBlocks_DMA` / `HAL_SD_WriteBlocks_DMA` 사용 가능.

**Connectivity → SDMMC1**

| 파라미터 | 값 |
|----------|-----|
| Mode | SD 4 bits Wide bus |
| Clock Edge | Rising Edge |
| Clock Power Save | Disable |
| Bus Wide | 4 bits |
| Hardware Flow Control | Disable |
| Clock Div | 4 (→ SDMMC_CK ≈ 12.5 MHz) |
| IDMA | (자동 활성, HAL 내부 관리) |

**핀 배정 (NUCLEO-H563ZI 기본값)**

| 핀 | 기능 |
|----|------|
| PC8  | SDMMC1_D0 |
| PC9  | SDMMC1_D1 |
| PC10 | SDMMC1_D2 |
| PC11 | SDMMC1_D3 |
| PC12 | SDMMC1_CK |
| PD2  | SDMMC1_CMD |

**NVIC**

| 인터럽트 | 우선순위 | 설명 |
|----------|---------|------|
| SDMMC1 global | Preempt=4, Sub=0 | IDMA TX/RX 완료 처리 |

---

## 4. USART3 설정 (GPDMA1 TX)

**Connectivity → USART3**

| 파라미터 | 값 |
|----------|-----|
| Mode | Asynchronous |
| Baud Rate | 115200 |
| Word Length | 8 bits |
| Stop Bits | 1 |
| Parity | None |
| DMA Settings → TX | GPDMA1 Channel 0 |
| DMA Settings → RX | (선택사항, GPDMA1 Channel 1) |

**핀 배정 (NUCLEO-H563ZI – ST-Link VCP)**

| 핀 | 기능 |
|----|------|
| PD8 | USART3_TX |
| PD9 | USART3_RX |

**NVIC**

| 인터럽트 | 우선순위 | 설명 |
|----------|---------|------|
| USART3 global | Preempt=6, Sub=0 | UART 오류 처리 |

---

## 5. GPDMA1 설정

**System Core → GPDMA1**

### Channel 0 – USART3 TX

| 파라미터 | 값 |
|----------|-----|
| Request | USART3_TX |
| Direction | Memory to Peripheral |
| Source Increment | Enable |
| Destination Increment | Disable |
| Source Data Width | Byte |
| Destination Data Width | Byte |
| Priority | Low |
| Mode | Normal |
| Burst Length (Src) | 1 |
| Burst Length (Dst) | 1 |
| Allocated Port | Src=Port0, Dst=Port1 |

**NVIC**

| 인터럽트 | 우선순위 | 설명 |
|----------|---------|------|
| GPDMA1 Channel 0 | Preempt=6, Sub=0 | UART TX DMA 완료 |

> Channel 1은 USART3 RX 용으로 예약 (미사용 시 건너뜀).

---

## 6. GPIO 설정 (LED – NUCLEO-H563ZI)

**GPIO → Output**

| 핀 | Label | 초기 상태 |
|----|-------|---------|
| PB0 | LED_GREEN  | Low |
| PF4 | LED_YELLOW | Low |
| PG4 | LED_RED    | Low |

Mode: Output Push-Pull, No Pull-up/down, Low Speed

---

## 7. NVIC 우선순위 정책

```
PreemptPriority bits = 4 (Groups = 16)

우선순위 (낮은 숫자 = 높은 우선순위):
  4 : SDMMC1   – SD IDMA 완료 (빠른 응답 필요)
  6 : GPDMA1 Channel 0 – USART3 TX
  6 : USART3           – UART 오류
  15: SysTick          – HAL 틱 (최저)
```

> SysTick과 SD/UART 인터럽트 간 우선순위 충돌 방지를 위해
> HAL_Init() 에서 SysTick 우선순위를 자동으로 15로 설정.

---

## 8. 링커 스크립트 – DMA 버퍼 섹션

STM32H563 D-Cache(32B 라인) 정합을 위해 DMA 버퍼를
32바이트 정렬 섹션에 배치:

`STM32H563ZI_FLASH.ld` 수정 예:

```ld
/* SRAM1(0x20000000) 내 DMA 버퍼 전용 섹션 */
.dma_buffer (NOLOAD) :
{
    . = ALIGN(32);
    *(.DMABufferSection)
    . = ALIGN(32);
} >RAM
```

코드에서 사용:
```c
static uint8_t sd_buf[512]
    __attribute__((aligned(32), section(".DMABufferSection")));
```

---

## 9. 코드 생성 옵션

**Project Manager → Code Generator**
- [x] Generate peripheral initialization as a pair of .c/.h files
- [x] Set all free pins as analog (power saving)
- [x] Delete previously generated files when not re-generated

---

## 10. 충돌 방지 핵심 포인트

| 항목 | 문제 | 해결 |
|------|------|------|
| D-Cache vs SDMMC IDMA | CPU 캐시와 DMA 메모리 불일치 | Read 전 InvalidateDCache, Write 전 CleanDCache |
| D-Cache vs GPDMA UART | CPU 캐시와 GPDMA 메모리 불일치 | UART TX 전 CleanDCache |
| UART 블로킹 vs SD 대기 | HAL_UART_Transmit 차단으로 SD 타임아웃 | UART TX를 GPDMA 비동기로 전환 |
| 버퍼 정렬 | 비정렬 접근 시 DMA 오류 | 모든 DMA 버퍼를 32B 정렬 |
| NVIC 우선순위 | SD 완료 인터럽트 지연 | SDMMC > GPDMA > UART 우선순위 설정 |
